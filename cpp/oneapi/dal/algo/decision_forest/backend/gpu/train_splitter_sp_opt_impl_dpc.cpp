/*******************************************************************************
* Copyright 2023 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include "oneapi/dal/detail/error_messages.hpp"
#include "oneapi/dal/detail/policy.hpp"
#include "oneapi/dal/table/row_accessor.hpp"
#include "oneapi/dal/detail/profiler.hpp"
#include "oneapi/dal/algo/decision_forest/backend/gpu/train_helpers.hpp"

#ifdef ONEDAL_DATA_PARALLEL

#include "oneapi/dal/algo/decision_forest/backend/gpu/train_splitter_sp_opt_impl.hpp"
#include "oneapi/dal/algo/decision_forest/backend/gpu/train_splitter_helpers.hpp"

namespace oneapi::dal::decision_forest::backend {

namespace de = dal::detail;
namespace bk = dal::backend;
namespace pr = dal::backend::primitives;

using alloc = sycl::usm::alloc;
using address = sycl::access::address_space;

using sycl::ext::oneapi::plus;
using sycl::ext::oneapi::minimum;
using sycl::ext::oneapi::maximum;

template <typename Float, typename Bin, typename Index, typename Task, Index sbg_size>
sycl::event train_splitter_sp_opt_impl<Float, Bin, Index, Task, sbg_size>::random_split_single_pass(
    sycl::queue& queue,
    const context_t& ctx,
    const pr::ndarray<Bin, 2>& data,
    const pr::ndview<Float, 1>& response,
    const pr::ndarray<Index, 1>& tree_order,
    const pr::ndarray<Index, 1>& selected_ftr_list,
    const pr::ndarray<Float, 1>& random_bins_com,
    const pr::ndarray<Index, 1>& bin_offset_list,
    const imp_data_t& imp_data_list,
    const pr::ndarray<Index, 1>& node_ind_list,
    Index node_ind_ofs,
    pr::ndarray<Index, 1>& node_list,
    imp_data_t& left_child_imp_data_list,
    pr::ndarray<Float, 1>& node_imp_dec_list,
    bool update_imp_dec_required,
    Index node_count,
    const bk::event_vector& deps) {
    ONEDAL_PROFILER_TASK(random_split_single_pass, queue);
    using split_smp_t = split_smp<Float, Index, Task>;
    using split_info_t = split_info<Float, Index, Task>;

    Index hist_prop_count = 0;
    if constexpr (std::is_same_v<std::decay_t<Task>, task::classification>) {
        hist_prop_count = ctx.class_count_;
    }
    else {
        hist_prop_count = impl_const<Index, task::regression>::hist_prop_count_;
    }

    ONEDAL_ASSERT(data.get_count() == ctx.row_count_ * ctx.column_count_);
    ONEDAL_ASSERT(response.get_count() == ctx.row_count_);
    ONEDAL_ASSERT(tree_order.get_count() == ctx.tree_in_block_ * ctx.selected_row_total_count_);
    ONEDAL_ASSERT(selected_ftr_list.get_count() >= node_count * ctx.selected_ftr_count_);
    ONEDAL_ASSERT(bin_offset_list.get_count() == ctx.column_count_ + 1);
    ONEDAL_ASSERT(imp_data_list.imp_list_.get_count() >=
                  node_count * impl_const_t::node_imp_prop_count_);
    if constexpr (std::is_same_v<Task, task::classification>) {
        ONEDAL_ASSERT(imp_data_list.class_hist_list_.get_count() >= node_count * ctx.class_count_);
    }
    ONEDAL_ASSERT(node_ind_list.get_count() >= (node_ind_ofs + node_count));
    ONEDAL_ASSERT(node_list.get_count() >=
                  (node_ind_ofs + node_count) * impl_const_t::node_prop_count_);
    ONEDAL_ASSERT(left_child_imp_data_list.imp_list_.get_count() >=
                  node_count * impl_const_t::node_imp_prop_count_);
    if constexpr (std::is_same_v<Task, task::classification>) {
        ONEDAL_ASSERT(left_child_imp_data_list.class_hist_list_.get_count() >=
                      node_count * ctx.class_count_);
    }

    if (update_imp_dec_required) {
        ONEDAL_ASSERT(node_imp_dec_list.get_count() >= node_count);
    }

    const Bin* data_ptr = data.get_data();
    const Float* response_ptr = response.get_data();
    const Index* tree_order_ptr = tree_order.get_data();

    const Index* selected_ftr_list_ptr = selected_ftr_list.get_data();

    imp_data_list_ptr<Float, Index, Task> imp_list_ptr(imp_data_list);

    const Index* node_indices_ptr = node_ind_list.get_data();
    Index* node_list_ptr = node_list.get_mutable_data();
    Float* node_imp_decr_list_ptr =
        update_imp_dec_required ? node_imp_dec_list.get_mutable_data() : nullptr;

    imp_data_list_ptr_mutable<Float, Index, Task> left_imp_list_ptr(left_child_imp_data_list);

    const Float* ftr_rnd_ptr = random_bins_com.get_data();

    const Index column_count = ctx.column_count_;
    const Index selected_ftr_count = ctx.selected_ftr_count_;
    const Index index_max = ctx.index_max_;

    Index max_wg_size = bk::device_max_wg_size(queue);
    ONEDAL_ASSERT(node_t::get_small_node_max_row_count() <= max_wg_size);
    Index local_size = std::min(bk::up_pow2(node_t::get_small_node_max_row_count()), max_wg_size);

    std::size_t local_hist_buf_size = hist_prop_count * 2; // x2 because bs_hist and ts_hist

    std::size_t local_buf_int_size = local_size;
    std::size_t local_buf_float_size = local_size;

    sycl::event last_event;

    const Index class_count = ctx.class_count_;
    const Float imp_threshold = ctx.impurity_threshold_;
    const Index min_obs_leaf = ctx.min_observations_in_leaf_node_;
    const Index max_bin_count_among_ftrs = ctx.max_bin_count_among_ftrs_;

    const Float* node_imp_list_ptr = imp_list_ptr.imp_list_ptr_;
    Float* left_child_imp_list_ptr = left_imp_list_ptr.imp_list_ptr_;

    // following vars are not used for regression, but should present to compile kernel
    const Index* class_hist_list_ptr = imp_list_ptr.get_class_hist_list_ptr_or_null();
    Index* left_child_class_hist_list_ptr = left_imp_list_ptr.get_class_hist_list_ptr_or_null();

    Index node_in_block_count = max_wg_count_ / local_size;

    std::size_t local_buf_byte_size = local_buf_int_size * sizeof(Index) +
                                      local_buf_float_size * sizeof(Float) +
                                      local_hist_buf_size * sizeof(hist_type_t);
    ONEDAL_ASSERT(device_has_enough_local_mem(queue, local_buf_byte_size));

    for (Index processed_node_cnt = 0; processed_node_cnt < node_count;
         processed_node_cnt += node_in_block_count, node_ind_ofs += node_in_block_count) {
        const sycl::nd_range<2> nd_range =
            bk::make_multiple_nd_range_2d({ local_size, node_in_block_count }, { local_size, 1 });

        last_event = queue.submit([&](sycl::handler& cgh) {
            cgh.depends_on(deps);

            local_accessor_rw_t<byte_t> local_byte_buf(local_buf_byte_size, cgh);

            cgh.parallel_for(
                nd_range,
                [=](sycl::nd_item<2> item) [[intel::reqd_sub_group_size(sbg_size)]] {
                    auto sbg = item.get_sub_group();
                    const Index node_idx = item.get_global_id(1);
                    if ((processed_node_cnt + node_idx) > (node_count - 1)) {
                        return;
                    }

                    const Index node_id = node_indices_ptr[node_ind_ofs + node_idx];
                    Index* node_ptr = node_list_ptr + node_id * impl_const_t::node_prop_count_;

                    const Index local_id = item.get_local_id(0);

                    const Index row_ofs = node_ptr[impl_const_t::ind_ofs];
                    const Index row_count = node_ptr[impl_const_t::ind_lrc];

                    split_smp_t sp_hlp;
                    split_info_t bs;

                    // slm pointers declaration
                    byte_t* local_byte_buf_ptr = local_byte_buf.get_pointer().get();
                    hist_type_t* local_hist_buf_ptr =
                        get_buf_ptr<hist_type_t>(&local_byte_buf_ptr, local_hist_buf_size);
                    Float* local_buf_float_ptr =
                        get_buf_ptr<Float>(&local_byte_buf_ptr, local_buf_float_size);

                    bs.init_clear(item, local_hist_buf_ptr + 0 * hist_prop_count, hist_prop_count);

                    for (Index ftr_idx = 0; ftr_idx < selected_ftr_count; ftr_idx++) {
                        split_info_t ts;
                        ts.init(local_hist_buf_ptr + 1 * hist_prop_count, hist_prop_count);
                        ts.ftr_id = selected_ftr_list_ptr[node_id * selected_ftr_count + ftr_idx];
                        Index i = local_id;
                        Index id = (i < row_count) ? tree_order_ptr[row_ofs + i] : index_max;
                        Index bin =
                            (i < row_count) ? data_ptr[id * column_count + ts.ftr_id] : index_max;
                        Float response = (i < row_count) ? response_ptr[id] : Float(0);
                        Index response_int = (i < row_count) ? static_cast<Index>(response) : -1;

                        Index min_bin = sycl::reduce_over_group(
                            item.get_group(),
                            bin < index_max ? bin : max_bin_count_among_ftrs,
                            minimum<Index>());
                        Index max_bin =
                            sycl::reduce_over_group(item.get_group(),
                                                    bin < max_bin_count_among_ftrs ? bin : 0,
                                                    maximum<Index>());

                        const Float rand_val = ftr_rnd_ptr[node_id * selected_ftr_count + ftr_idx];
                        const Index random_bin_ofs =
                            static_cast<Index>(rand_val * (max_bin - min_bin + 1));
                        ts.ftr_bin = min_bin + random_bin_ofs;

                        const Index count = (bin <= ts.ftr_bin) ? 1 : 0;

                        if constexpr (std::is_same_v<Task, task::classification>) {
                            const Index left_count =
                                sycl::reduce_over_group(item.get_group(), count, plus<Index>());
                            const Index val = (bin <= ts.ftr_bin) ? response_int : -1;
                            Index all_class_count = 0;

                            for (Index class_id = 0; class_id < class_count - 1; class_id++) {
                                Index total_class_count =
                                    sycl::reduce_over_group(item.get_group(),
                                                            Index(class_id == val),
                                                            plus<Index>());
                                all_class_count += total_class_count;
                                ts.left_hist[class_id] = total_class_count;
                            }

                            ts.left_count = left_count;

                            ts.left_hist[class_count - 1] = ts.left_count - all_class_count;
                        }
                        else {
                            const Float val = (bin <= ts.ftr_bin) ? response : Float(0);

                            Float left_count =
                                Float(sycl::reduce_over_group(sbg, count, plus<Index>()));
                            Float sum = sycl::reduce_over_group(sbg, val, plus<Float>());

                            Float mean = sum / left_count;

                            Float val_s2c =
                                (bin <= ts.ftr_bin) ? (val - mean) * (val - mean) : Float(0);

                            Float sum2cent = sycl::reduce_over_group(sbg, val_s2c, plus<Float>());

                            reduce_hist_over_group(item,
                                                   local_buf_float_ptr,
                                                   left_count,
                                                   mean,
                                                   sum2cent);

                            ts.left_count = Index(left_count);

                            ts.left_hist[0] = left_count;
                            ts.left_hist[1] = mean;
                            ts.left_hist[2] = sum2cent;
                        }

                        if (local_id == 0) {
                            if constexpr (std::is_same_v<Task, task::classification>) {
                                sp_hlp.calc_imp_dec(ts,
                                                    node_ptr,
                                                    node_imp_list_ptr,
                                                    class_hist_list_ptr,
                                                    class_count,
                                                    node_id);
                                sp_hlp.choose_best_split(bs,
                                                         ts,
                                                         node_imp_list_ptr,
                                                         class_count,
                                                         node_id,
                                                         imp_threshold,
                                                         min_obs_leaf);
                            }
                            else {
                                sp_hlp.calc_imp_dec(ts, node_ptr, node_imp_list_ptr, node_id);
                                sp_hlp.choose_best_split(bs,
                                                         ts,
                                                         node_imp_list_ptr,
                                                         impl_const_t::hist_prop_count_,
                                                         node_id,
                                                         imp_threshold,
                                                         min_obs_leaf);
                            }
                        }
                    }

                    if (local_id == 0) {
                        sp_hlp.update_node_bs_info(bs,
                                                   node_ptr,
                                                   node_imp_decr_list_ptr,
                                                   node_id,
                                                   index_max,
                                                   update_imp_dec_required);
                        if constexpr (std::is_same_v<Task, task::classification>) {
                            sp_hlp.update_left_child_imp(left_child_imp_list_ptr,
                                                         left_child_class_hist_list_ptr,
                                                         bs.left_imp,
                                                         bs.left_hist,
                                                         node_id,
                                                         class_count);
                        }
                        else {
                            sp_hlp.update_left_child_imp(left_child_imp_list_ptr,
                                                         bs.left_hist,
                                                         node_id);
                        }
                    }
                });
        });

        last_event.wait_and_throw();
    }
    return last_event;
}


template <typename Float, typename Bin, typename Index, typename Task, Index sbg_size>
sycl::event
train_splitter_sp_opt_impl<Float, Bin, Index, Task, sbg_size>::best_split_single_pass_large(
    sycl::queue& queue,
    const context_t& ctx,
    const pr::ndarray<Bin, 2>& data,
    const pr::ndview<Float, 1>& response,
    const pr::ndarray<Index, 1>& tree_order,
    const pr::ndarray<Index, 1>& selected_ftr_list,
    const pr::ndarray<Index, 1>& bin_offset_list,
    const imp_data_t& imp_data_list,
    const pr::ndarray<Index, 1>& node_ind_list,
    Index node_ind_ofs,
    pr::ndarray<Index, 1>& node_list,
    imp_data_t& left_child_imp_data_list,
    pr::ndarray<Float, 1>& node_imp_dec_list,
    bool update_imp_dec_required,
    Index node_count,
    const bk::event_vector& deps) {
    ONEDAL_PROFILER_TASK(best_split_single_pass_large, queue);

    const Bin* data_ptr = data.get_data();
    const Float* response_ptr = response.get_data();
    const Index* tree_order_ptr = tree_order.get_data();

    const Index* selected_ftr_list_ptr = selected_ftr_list.get_data();

    imp_data_list_ptr<Float, Index, Task> imp_list_ptr(imp_data_list);

    const Index* node_indices_ptr = node_ind_list.get_data();
    Index* node_list_ptr = node_list.get_mutable_data();
    Float* node_imp_decr_list_ptr =
        update_imp_dec_required ? node_imp_dec_list.get_mutable_data() : nullptr;

    imp_data_list_ptr_mutable<Float, Index, Task> left_imp_list_ptr(left_child_imp_data_list);

    const Float* node_imp_list_ptr = imp_list_ptr.imp_list_ptr_;
    Float* left_child_imp_list_ptr = left_imp_list_ptr.imp_list_ptr_;

    using split_smp_t = split_smp<Float, Index, Task>;
    using split_scalar_t = split_scalar<Float, Index>;
    using split_info_t = split_info<Float, Index, Task>;

    Index hist_prop_count = 0;
    if constexpr (std::is_same_v<std::decay_t<Task>, task::classification>) {
        hist_prop_count = ctx.class_count_;
    }
    else {
        hist_prop_count = impl_const<Index, task::regression>::hist_prop_count_;
    }

    const Index class_count = ctx.class_count_;
    const Index column_count = ctx.column_count_;
    const Float imp_threshold = ctx.impurity_threshold_;
    const Index min_obs_leaf = ctx.min_observations_in_leaf_node_;
    const Index index_max = ctx.index_max_;
    
    // following vars are not used for regression, but should present to compile kernel
    const Index* class_hist_list_ptr = imp_list_ptr.get_class_hist_list_ptr_or_null();
    Index* left_child_class_hist_list_ptr = left_imp_list_ptr.get_class_hist_list_ptr_or_null();

    const Index selected_ftr_count = ctx.selected_ftr_count_;
    const Index max_bin_size = ctx.max_bin_count_among_ftrs_;
    const Index local_size = bk::device_max_wg_size(queue);
    // const Index wg_ftr_count = sbg_size;
    const Index node_in_block_count = max_wg_count_;
    const Index total_split_count = selected_ftr_count * max_bin_size;

    sycl::event last_event;

    const std::uint32_t local_hist_size = total_split_count * hist_prop_count;
    const std::uint32_t best_splits_size = total_split_count;

    std::cout << "total_split_count=" << total_split_count << ", local_size=" << local_size << std::endl;
    for (Index processed_nodes = 0; processed_nodes < node_count; processed_nodes += node_in_block_count){
        auto nd_range = bk::make_multiple_nd_range_2d({local_size, node_in_block_count}, {local_size, 1});
        last_event = queue.submit([&](sycl::handler& cgh){
            cgh.depends_on(deps);
            local_accessor_rw_t<hist_type_t> local_hist(2 * local_hist_size, cgh);
            local_accessor_rw_t<split_scalar_t> scalars_buf(best_splits_size, cgh);
            sycl::stream out(2048, 512, cgh);
            cgh.parallel_for(
                nd_range,
                [=](sycl::nd_item<2> item) {
                    const Index node_idx = item.get_global_id(1);
                    if ((node_idx + processed_nodes) > (node_count - 1)) {
                        return;
                    }
                    // Load common data
                    const Index node_id = node_indices_ptr[node_ind_ofs + node_idx];
                    Index* node_ptr = node_list_ptr + node_id * impl_const_t::node_prop_count_;
                    
                    const Index local_id = item.get_local_id(0);

                    const Index row_ofs = node_ptr[impl_const_t::ind_ofs];
                    const Index row_count = node_ptr[impl_const_t::ind_lrc];

                    hist_type_t* local_hist_ptr = local_hist.get_pointer().get();
                    hist_type_t* tmp_hist_ptr = local_hist_ptr + local_hist_size;
                    split_scalar_t* scalars_buf_ptr = scalars_buf.get_pointer().get();
                    scalars_buf_ptr[local_id].ftr_id = impl_const_t::leaf_mark_;
                    scalars_buf_ptr[local_id].ftr_bin = impl_const_t::leaf_mark_;
                    scalars_buf_ptr[local_id].left_count = 0;
                    scalars_buf_ptr[local_id].imp_dec = -de::limits<Float>::max();

                    split_smp_t sp_hlp;

                    // Calculate histogram
                    for (Index ftr_idx = 0; ftr_idx < selected_ftr_count && local_id < row_count; ++ftr_idx) {
                        Index ts_ftr_id = selected_ftr_list_ptr[node_id * selected_ftr_count + ftr_idx];
                        for (Index i = local_id; i < row_count; i += local_size) {
                            // Index i = local_id;
                            Index id = tree_order_ptr[row_ofs + i];
                            Index bin = data_ptr[id * column_count + ts_ftr_id];
                            Float response = response_ptr[id];
                            Index response_int = static_cast<Index>(response);

                            const Index cur_hist_pos = local_hist_size + (ftr_idx * max_bin_size + bin) * hist_prop_count; // VOT TUT BILA ERROR

                            if constexpr (std::is_same_v<Task, task::classification>) {
                                sycl::atomic_ref<Index,
                                        sycl::memory_order_acq_rel,
                                        sycl::memory_scope_work_group,
                                        sycl::access::address_space::local_space>
                                    hist_resp(local_hist[cur_hist_pos + response_int]);
                                hist_resp += 1;

                            }
                            else {
                                sycl::atomic_ref<Float,
                                        sycl::memory_order_acq_rel,
                                        sycl::memory_scope_work_group,
                                        sycl::access::address_space::local_space>
                                    hist_resp_count(local_hist[cur_hist_pos + 0]);
                                hist_resp_count += 1;
                                sycl::atomic_ref<Float,
                                        sycl::memory_order_acq_rel,
                                        sycl::memory_scope_work_group,
                                        sycl::access::address_space::local_space>
                                    hist_resp_sum(local_hist[cur_hist_pos + 1]);
                                hist_resp_sum += response;
                            }
                        }
                    }
                    // for (Index work_bin = local_id; work_bin < total_split_count; work_bin += local_size) {
                    //     Index i = 0;
                    //     const Index cur_bin = work_bin % max_bin_size;
                    //     const Index ftr_idx = work_bin / max_bin_size;
                    //     const Index ts_ftr_id = selected_ftr_list_ptr[node_id * selected_ftr_count + ftr_idx];
                    //     const Index cur_hist_pos = work_bin * hist_prop_count;
                    //     hist_type_t* cur_hist = tmp_hist_ptr + cur_hist_pos;
                    //     Index iter_id = tree_order_ptr[row_ofs + i];
                    //     Index iter_bin = data_ptr[iter_id * column_count + ts_ftr_id];
                    //     for (; iter_bin < cur_bin && i < row_count; ++i) {
                    //         iter_id = tree_order_ptr[row_ofs + i];
                    //         iter_bin = data_ptr[iter_id * column_count + ts_ftr_id];
                    //     }
                    //     for (; i < row_count && iter_bin == cur_bin; ++i){
                    //         iter_id = tree_order_ptr[row_ofs + i];
                    //         iter_bin = data_ptr[iter_id * column_count + ts_ftr_id];
                    //         const Float response = response_ptr[iter_id];
                    //         const Index response_int = static_cast<Index>(response);
                    //         if constexpr (std::is_same_v<Task, task::classification>) {
                    //             cur_hist[cur_hist_pos + response_int] += 1;
                    //         }
                    //         else {
                    //             cur_hist[cur_hist_pos + 0] += 1;
                    //             cur_hist[cur_hist_pos + 1] += response;
                    //         }
                    //     }
                    // }
                    item.barrier(sycl::access::fence_space::local_space);
                    split_info_t ts;
                    // Finilize histograms
                    for (Index work_bin = local_id; work_bin < total_split_count; work_bin += local_size) {
                        Index ftr_idx = work_bin / max_bin_size;
                        Index cur_bin = work_bin % max_bin_size;
                        const Index cur_hist_pos = work_bin * hist_prop_count;
                        hist_type_t* cur_hist = local_hist_ptr + cur_hist_pos;
                        hist_type_t* init_hist = tmp_hist_ptr + cur_hist_pos;
                        hist_type_t* ftr_hist = tmp_hist_ptr + (work_bin - cur_bin) * hist_prop_count;
                        // Collect all hists on the left side of the current bin
                        Index left_count = 0;
                        if constexpr (std::is_same_v<Task, task::classification>) {
                            for (Index bin_idx = 0; bin_idx <= cur_bin; ++bin_idx) {
                                for (Index cls = 0; cls < hist_prop_count; ++cls) {
                                    cur_hist[cls] += ftr_hist[bin_idx * hist_prop_count + cls];
                                    left_count += ftr_hist[bin_idx * hist_prop_count + cls];
                                }
                            }
                        }
                        else {
                            // First bin must be copied
                            cur_hist[0] = init_hist[0];
                            cur_hist[1] = init_hist[1];
                            cur_hist[2] = init_hist[2];
                            for (Index bin_idx = 0; bin_idx < cur_bin; ++bin_idx){
                                hist_type_t* iter_left_hist = ftr_hist + bin_idx * hist_prop_count;
                                if (cur_hist[0] > 0) {
                                    Float cur_resp = init_hist[0] >  0 ? init_hist[1] / init_hist[0] : 0;
                                    cur_hist[0] += iter_left_hist[0];
                                    cur_hist[1] = (iter_left_hist[1] + cur_hist[1] * cur_hist[0]) / (cur_hist[0] + iter_left_hist[0]); 
                                    cur_hist[2] = (cur_resp - cur_hist[1]) * (cur_resp - cur_hist[1]);
                                }
                                else {
                                    cur_hist[0] = iter_left_hist[0];
                                    cur_hist[1] = iter_left_hist[1];
                                    cur_hist[2] = iter_left_hist[2];
                                }
                            }
                            left_count += Index(cur_hist[0]);
                        }
                        if (left_count == 0) {
                            continue;
                        }
                        ts.init(cur_hist, hist_prop_count);
                        ts.ftr_id = selected_ftr_list_ptr[node_id * selected_ftr_count + ftr_idx];
                        ts.ftr_bin = cur_bin;
                        // out << "ftr_bin=" << cur_bin << ", ftr_id=" << ts.ftr_id << ", left_count=" << left_count << ", cur_hist[0]=" << cur_hist[0] << "\n";
                        ts.left_count = left_count;
                        if constexpr (std::is_same_v<Task, task::classification>) {
                            sp_hlp.calc_imp_dec(ts,
                                                node_ptr,
                                                node_imp_list_ptr,
                                                class_hist_list_ptr,
                                                class_count,
                                                node_id);
                        }
                        else {
                            sp_hlp.calc_imp_dec(ts, node_ptr, node_imp_list_ptr, node_id);
                        }
                        ts.store_scalar(scalars_buf_ptr[work_bin]);
                        // out << scalars_buf_ptr[work_bin].ftr_bin << ", " << scalars_buf_ptr[work_bin].ftr_id << ", " << scalars_buf_ptr[work_bin].left_count << ", left_hist[0]=" << cur_hist[0] << "\n";
                    }
                    // item.barrier(sycl::access::fence_space::local_space);
                    // Init best and current split info
                    split_info_t bs;
                    bs.init_clear(item, tmp_hist_ptr + local_id * hist_prop_count, hist_prop_count);
                    if (local_id < total_split_count) {
                        ts.init(local_hist_ptr + local_id * hist_prop_count, hist_prop_count);
                        ts.load_scalar(scalars_buf_ptr[local_id]);
                        if constexpr (std::is_same_v<Task, task::classification>) {
                            sp_hlp.choose_best_split(bs,
                                                        ts,
                                                        node_imp_list_ptr,
                                                        class_count,
                                                        node_id,
                                                        imp_threshold,
                                                        min_obs_leaf);
                        }
                        else {
                            sp_hlp.choose_best_split(bs,
                                                        ts,
                                                        node_imp_list_ptr,
                                                        impl_const_t::hist_prop_count_,
                                                        node_id,
                                                        imp_threshold,
                                                        min_obs_leaf);
                        }
                    }
                    // Tree reduction and selecting best
                    for (Index i = local_size / 2; i > 0; i >>= 1) {
                        item.barrier(sycl::access::fence_space::local_space);
                        if (local_id < i && (local_id + i) < total_split_count) {
                            ts.init(local_hist_ptr + (local_id + i) * hist_prop_count, hist_prop_count);
                            ts.load_scalar(scalars_buf_ptr[local_id + i]);
                            if (local_id == 0) {
                                // out << "i=" << i << ", ts.ftr_id=" << ts.ftr_id << ", ts.ftr_bin=" << ts.ftr_bin << ", ts.left_count=" << ts.left_count << ", ts.left_hist[0]=" << ts.left_hist[0] << "\n";
                                // out << "i=" << i << ", bs.ftr_id=" << bs.ftr_id << ", bs.ftr_bin=" << bs.ftr_bin << ", bs.left_count=" << bs.left_count << ", bs.left_hist[0]=" << bs.left_hist[0] << "\n";
                            }
                            if constexpr (std::is_same_v<Task, task::classification>) {
                                sp_hlp.choose_best_split(bs,
                                                            ts,
                                                            node_imp_list_ptr,
                                                            class_count,
                                                            node_id,
                                                            imp_threshold,
                                                            min_obs_leaf);
                            }
                            else {
                                sp_hlp.choose_best_split(bs,
                                                            ts,
                                                            node_imp_list_ptr,
                                                            impl_const_t::hist_prop_count_,
                                                            node_id,
                                                            imp_threshold,
                                                            min_obs_leaf);
                            }
                        }
                    }
                    // Update global split info
                    if (local_id == 0) {
                        out << "best: bs.ftr_id=" << bs.ftr_id << ", bs.ftr_bin=" << bs.ftr_bin << ", bs.left_count=" << bs.left_count << "\n";
                        sp_hlp.update_node_bs_info(bs,
                                                    node_ptr,
                                                    node_imp_decr_list_ptr,
                                                    node_id,
                                                    index_max,
                                                    update_imp_dec_required);
                        if constexpr (std::is_same_v<Task, task::classification>) {
                            sp_hlp.update_left_child_imp(left_child_imp_list_ptr,
                                                            left_child_class_hist_list_ptr,
                                                            bs.left_imp,
                                                            bs.left_hist,
                                                            node_id,
                                                            class_count);
                        }
                        else {
                            sp_hlp.update_left_child_imp(left_child_imp_list_ptr,
                                                            bs.left_hist,
                                                            node_id);
                        }
                    }
            });
        });
        last_event.wait_and_throw();
    }
    return last_event;
}

// template <typename Float, typename Bin, typename Index, typename Task, Index sbg_size>
// sycl::event
// train_splitter_sp_opt_impl<Float, Bin, Index, Task, sbg_size>::best_split_single_pass_large(
//     sycl::queue& queue,
//     const context_t& ctx,
//     const pr::ndarray<Bin, 2>& data,
//     const pr::ndview<Float, 1>& response,
//     const pr::ndarray<Index, 1>& tree_order,
//     const pr::ndarray<Index, 1>& selected_ftr_list,
//     const pr::ndarray<Index, 1>& bin_offset_list,
//     const imp_data_t& imp_data_list,
//     const pr::ndarray<Index, 1>& node_ind_list,
//     Index node_ind_ofs,
//     pr::ndarray<Index, 1>& node_list,
//     imp_data_t& left_child_imp_data_list,
//     pr::ndarray<Float, 1>& node_imp_dec_list,
//     bool update_imp_dec_required,
//     Index node_count,
//     const bk::event_vector& deps) {
//     ONEDAL_PROFILER_TASK(best_split_single_pass_large, queue);

//     using split_smp_t = split_smp<Float, Index, Task>;
//     using split_info_t = split_info<Float, Index, Task>;

//     Index hist_prop_count = 0;
//     if constexpr (std::is_same_v<std::decay_t<Task>, task::classification>) {
//         hist_prop_count = ctx.class_count_;
//     }
//     else {
//         hist_prop_count = impl_const<Index, task::regression>::hist_prop_count_;
//     }

//     ONEDAL_ASSERT(data.get_count() == ctx.row_count_ * ctx.column_count_);
//     ONEDAL_ASSERT(response.get_count() == ctx.row_count_);
//     ONEDAL_ASSERT(tree_order.get_count() == ctx.tree_in_block_ * ctx.selected_row_total_count_);
//     ONEDAL_ASSERT(selected_ftr_list.get_count() >= node_count * ctx.selected_ftr_count_);
//     ONEDAL_ASSERT(bin_offset_list.get_count() == ctx.column_count_ + 1);
//     ONEDAL_ASSERT(imp_data_list.imp_list_.get_count() >=
//                   node_count * impl_const_t::node_imp_prop_count_);
//     if constexpr (std::is_same_v<Task, task::classification>) {
//         ONEDAL_ASSERT(imp_data_list.class_hist_list_.get_count() >= node_count * ctx.class_count_);
//     }
//     ONEDAL_ASSERT(node_ind_list.get_count() >= (node_ind_ofs + node_count));
//     ONEDAL_ASSERT(node_list.get_count() >=
//                   (node_ind_ofs + node_count) * impl_const_t::node_prop_count_);
//     ONEDAL_ASSERT(left_child_imp_data_list.imp_list_.get_count() >=
//                   node_count * impl_const_t::node_imp_prop_count_);
//     if constexpr (std::is_same_v<Task, task::classification>) {
//         ONEDAL_ASSERT(left_child_imp_data_list.class_hist_list_.get_count() >=
//                       node_count * ctx.class_count_);
//     }

//     if (update_imp_dec_required) {
//         ONEDAL_ASSERT(node_imp_dec_list.get_count() >= node_count);
//     }

//     const Bin* data_ptr = data.get_data();
//     const Float* response_ptr = response.get_data();
//     const Index* tree_order_ptr = tree_order.get_data();

//     const Index* selected_ftr_list_ptr = selected_ftr_list.get_data();

//     imp_data_list_ptr<Float, Index, Task> imp_list_ptr(imp_data_list);

//     const Index* node_indices_ptr = node_ind_list.get_data();
//     Index* node_list_ptr = node_list.get_mutable_data();
//     Float* node_imp_decr_list_ptr =
//         update_imp_dec_required ? node_imp_dec_list.get_mutable_data() : nullptr;

//     imp_data_list_ptr_mutable<Float, Index, Task> left_imp_list_ptr(left_child_imp_data_list);

//     const Index column_count = ctx.column_count_;

//     const Index selected_ftr_count = ctx.selected_ftr_count_;

//     const Index index_max = ctx.index_max_;

//     Index max_wg_size = bk::device_max_wg_size(queue);
//     ONEDAL_ASSERT(node_t::get_small_node_max_row_count() <= max_wg_size);
//     Index local_size = std::min(bk::up_pow2(node_t::get_small_node_max_row_count()), max_wg_size);

//     std::size_t max_bin = ctx.max_bin_count_among_ftrs_;
//     std::size_t local_hist_buf_size = hist_prop_count * (max_feature_worker_per_node_count_ + max_bin + 1); // +1 because bs_hist

//     std::size_t local_buf_int_size = local_size;
//     std::size_t local_buf_float_size = local_size;

//     // 1 counter for global count of processed ftrs for node, and max_sbg_size - num of slot flags
//     std::size_t global_aux_ftr_buf_int_size = sbg_size;

//     sycl::event last_event;

//     const Index class_count = ctx.class_count_;
//     const Float imp_threshold = ctx.impurity_threshold_;
//     const Index min_obs_leaf = ctx.min_observations_in_leaf_node_;

//     const Float* node_imp_list_ptr = imp_list_ptr.imp_list_ptr_;
//     Float* left_child_imp_list_ptr = left_imp_list_ptr.imp_list_ptr_;

//     // following vars are not used for regression, but should present to compile kernel
//     const Index* class_hist_list_ptr = imp_list_ptr.get_class_hist_list_ptr_or_null();
//     Index* left_child_class_hist_list_ptr = left_imp_list_ptr.get_class_hist_list_ptr_or_null();

//     Index ftr_worker_per_node_count =
//         std::min(bk::down_pow2(selected_ftr_count), max_feature_worker_per_node_count_);
//     Index node_in_block_count = max_wg_count_ / ftr_worker_per_node_count;

//     std::int64_t global_buf_byte_size =
//         max_wg_count_ * split_info_t::get_cache_byte_size(hist_prop_count);

//     ONEDAL_ASSERT(global_buf_byte_size > 0);
//     auto global_byte_buf =
//         pr::ndarray<byte_t, 1>::empty(queue, { global_buf_byte_size }, alloc::device);
//     byte_t* global_byte_ptr = global_byte_buf.get_mutable_data();
//     auto global_aux_ftr_buf_int = pr::ndarray<Index, 1>::empty(
//         queue,
//         { std::int64_t(node_in_block_count * global_aux_ftr_buf_int_size) },
//         alloc::device);

//     Index* global_aux_ftr_buf_int_ptr = global_aux_ftr_buf_int.get_mutable_data();
//     std::size_t local_buf_byte_size = local_buf_int_size * sizeof(Index) +
//                                       local_buf_float_size * sizeof(Float) +
//                                       local_hist_buf_size * sizeof(hist_type_t);
//     ONEDAL_ASSERT(device_has_enough_local_mem(queue, local_buf_byte_size));
//     // TODO: add separate branch to process situation when there isn't enough local mem

//     for (Index processed_node_cnt = 0; processed_node_cnt < node_count;
//          processed_node_cnt += node_in_block_count, node_ind_ofs += node_in_block_count) {
//         auto fill_aux_ftr_event = global_aux_ftr_buf_int.fill(queue, 0);

//         const sycl::nd_range<2> nd_range =
//             bk::make_multiple_nd_range_2d({ local_size, max_wg_count_ }, { local_size, 1 });

//         last_event = queue.submit([&](sycl::handler& cgh) {
//             cgh.depends_on(deps);
//             cgh.depends_on(fill_aux_ftr_event);

//             local_accessor_rw_t<byte_t> local_byte_buf(local_buf_byte_size, cgh);

//             cgh.parallel_for(
//                 nd_range,
//                 [=](sycl::nd_item<2> item) [[intel::reqd_sub_group_size(sbg_size)]] {
//                     auto sbg = item.get_sub_group();
//                     const Index group_id = item.get_group(1);
//                     const Index ftr_group_id = group_id % ftr_worker_per_node_count;
//                     const Index node_idx = item.get_global_id(1) / ftr_worker_per_node_count;
//                     if ((processed_node_cnt + node_idx) > (node_count - 1) ||
//                         ftr_group_id > (selected_ftr_count - 1)) {
//                         return;
//                     }

//                     const Index node_id = node_indices_ptr[node_ind_ofs + node_idx];
//                     Index* node_ptr = node_list_ptr + node_id * impl_const_t::node_prop_count_;

//                     const Index local_id = item.get_local_id(0);

//                     const Index sub_group_id = sbg.get_group_id();
//                     const Index sub_group_local_id = sbg.get_local_id();

//                     const Index row_ofs = node_ptr[impl_const_t::ind_ofs];
//                     const Index row_count = node_ptr[impl_const_t::ind_lrc];

//                     split_smp_t sp_hlp;
//                     split_info<Float, Index, Task> bs;

//                     // slm pointers declaration
//                     byte_t* local_byte_buf_ptr = local_byte_buf.get_pointer().get();
//                     hist_type_t* local_hist_buf_ptr =
//                         get_buf_ptr<hist_type_t>(&local_byte_buf_ptr, local_hist_buf_size);
//                     Float* local_buf_float_ptr =
//                         get_buf_ptr<Float>(&local_byte_buf_ptr, local_buf_float_size);

//                     bs.init_clear(item, local_hist_buf_ptr + 0 * hist_prop_count, hist_prop_count);

//                     Index processed_ftr_count = 0;
//                     for (Index ftr_idx = ftr_group_id; ftr_idx < selected_ftr_count;
//                          ftr_idx += ftr_worker_per_node_count) {
//                         processed_ftr_count++;

//                         Index ts_ftr_id = selected_ftr_list_ptr[node_id * selected_ftr_count + ftr_idx];

//                         Index i = local_id;
//                         Index id = (i < row_count) ? tree_order_ptr[row_ofs + i] : index_max;
//                         Index bin =
//                             (i < row_count) ? data_ptr[id * column_count + ts_ftr_id] : index_max;
//                         Float response = (i < row_count) ? response_ptr[id] : Float(0);
//                         Index response_int = (i < row_count) ? static_cast<Index>(response) : -1;

//                         split_info<Float, Index, Task> ts;
//                         ts.init_clear(item, local_hist_buf_ptr + (ftr_idx + bin + 1) * hist_prop_count, hist_prop_count);
//                         ts.ftr_id = ts_ftr_id;

//                         ts.ftr_bin = -1;
//                         ts.ftr_bin = sycl::reduce_over_group(item.get_group(),
//                                                              bin > ts.ftr_bin ? bin : index_max,
//                                                              minimum<Index>());

//                         while (ts.ftr_bin < index_max) {
//                             const Index count = (bin <= ts.ftr_bin) ? 1 : 0;

//                             if constexpr (std::is_same_v<Task, task::classification>) {
//                                 const Index left_count =
//                                     sycl::reduce_over_group(item.get_group(), count, plus<Index>());
//                                 const Index val = (bin <= ts.ftr_bin) ? response_int : -1;
//                                 Index all_class_count = 0;

//                                 for (Index class_id = 0; class_id < class_count - 1; class_id++) {
//                                     Index total_class_count =
//                                         sycl::reduce_over_group(item.get_group(),
//                                                                 Index(class_id == val),
//                                                                 plus<Index>());
//                                     all_class_count += total_class_count;
//                                     ts.left_hist[class_id] = total_class_count;
//                                 }

//                                 ts.left_count = left_count;

//                                 ts.left_hist[class_count - 1] = ts.left_count - all_class_count;
//                             }
//                             else {
//                                 auto sbg = item.get_sub_group();
//                                 const Float val = (bin <= ts.ftr_bin) ? response : Float(0);

//                                 Float left_count =
//                                     Float(sycl::reduce_over_group(sbg, count, plus<Index>()));
//                                 Float sum = sycl::reduce_over_group(sbg, val, plus<Float>());

//                                 Float mean = sum / left_count;

//                                 Float val_s2c =
//                                     (bin <= ts.ftr_bin) ? (val - mean) * (val - mean) : Float(0);

//                                 Float sum2cent =
//                                     sycl::reduce_over_group(sbg, val_s2c, plus<Float>());

//                                 reduce_hist_over_group(item,
//                                                        local_buf_float_ptr,
//                                                        left_count,
//                                                        mean,
//                                                        sum2cent);

//                                 ts.left_count = Index(left_count);

//                                 ts.left_hist[0] = left_count;
//                                 ts.left_hist[1] = mean;
//                                 ts.left_hist[2] = sum2cent;
//                             }

//                             if (local_id == 0) {
//                                 if constexpr (std::is_same_v<Task, task::classification>) {
//                                     sp_hlp.calc_imp_dec(ts,
//                                                         node_ptr,
//                                                         node_imp_list_ptr,
//                                                         class_hist_list_ptr,
//                                                         class_count,
//                                                         node_id);
//                                     sp_hlp.choose_best_split(bs,
//                                                              ts,
//                                                              node_imp_list_ptr,
//                                                              class_count,
//                                                              node_id,
//                                                              imp_threshold,
//                                                              min_obs_leaf);
//                                 }
//                                 else {
//                                     sp_hlp.calc_imp_dec(ts, node_ptr, node_imp_list_ptr, node_id);
//                                     sp_hlp.choose_best_split(bs,
//                                                              ts,
//                                                              node_imp_list_ptr,
//                                                              impl_const_t::hist_prop_count_,
//                                                              node_id,
//                                                              imp_threshold,
//                                                              min_obs_leaf);
//                                 }
//                             }

//                             ts.ftr_bin = sycl::reduce_over_group(item.get_group(),
//                                                                  bin > ts.ftr_bin ? bin : index_max,
//                                                                  minimum<Index>());
//                         }
//                     }

//                     if (sub_group_id > 0) {
//                         return;
//                     }

//                     Index total_processed_ftr_count = 0;

//                     byte_t* global_byte_buf_ptr =
//                         global_byte_ptr + node_idx * ftr_worker_per_node_count *
//                                               split_info_t::get_cache_byte_size(hist_prop_count);

//                     if (local_id == 0) {
//                         bs.store(global_byte_buf_ptr, ftr_group_id, ftr_worker_per_node_count);
//                         total_processed_ftr_count = bk::atomic_global_sum(
//                             global_aux_ftr_buf_int_ptr + node_idx * global_aux_ftr_buf_int_size,
//                             processed_ftr_count);
//                     }

//                     // read slm marker
//                     total_processed_ftr_count =
//                         sycl::group_broadcast(sbg, total_processed_ftr_count, 0);

//                     if (total_processed_ftr_count == selected_ftr_count) {
//                         bs.load(global_byte_buf_ptr,
//                                 sub_group_local_id % ftr_worker_per_node_count,
//                                 ftr_worker_per_node_count);
//                         if (sp_hlp
//                                 .my_split_is_best_for_sbg(item, bs, node_ptr, node_id, index_max)) {
//                             sp_hlp.update_node_bs_info(bs,
//                                                        node_ptr,
//                                                        node_imp_decr_list_ptr,
//                                                        node_id,
//                                                        index_max,
//                                                        update_imp_dec_required);
//                             if constexpr (std::is_same_v<Task, task::classification>) {
//                                 sp_hlp.update_left_child_imp(left_child_imp_list_ptr,
//                                                              left_child_class_hist_list_ptr,
//                                                              bs.left_imp,
//                                                              bs.left_hist,
//                                                              node_id,
//                                                              class_count);
//                             }
//                             else {
//                                 sp_hlp.update_left_child_imp(left_child_imp_list_ptr,
//                                                              bs.left_hist,
//                                                              node_id);
//                             }
//                         }
//                     }
//                 });
//         });

//         last_event.wait_and_throw();
//     }
//     return last_event;
// }

template <typename Float, typename Bin, typename Index, typename Task, Index sbg_size>
sycl::event
train_splitter_sp_opt_impl<Float, Bin, Index, Task, sbg_size>::best_split_single_pass_small(
    sycl::queue& queue,
    const context_t& ctx,
    const pr::ndarray<Bin, 2>& data,
    const pr::ndview<Float, 1>& response,
    const pr::ndarray<Index, 1>& tree_order,
    const pr::ndarray<Index, 1>& selected_ftr_list,
    const pr::ndarray<Index, 1>& bin_offset_list,
    const imp_data_t& imp_data_list,
    const node_group_view_t& node_group,
    node_list_t& level_node_list,
    imp_data_t& left_child_imp_data_list,
    pr::ndarray<Float, 1>& node_imp_dec_list,
    bool update_imp_dec_required,
    const bk::event_vector& deps) {
    ONEDAL_PROFILER_TASK(best_split_single_pass_small, queue);

    using split_smp_t = split_smp<Float, Index, Task>;
    using split_info_t = split_info<Float, Index, Task>;

    Index hist_prop_count = 0;
    if constexpr (std::is_same_v<std::decay_t<Task>, task::classification>) {
        hist_prop_count = ctx.class_count_;
    }
    else {
        hist_prop_count = impl_const<Index, task::regression>::hist_prop_count_;
    }

    ONEDAL_ASSERT(node_group.state_is_valid());
    ONEDAL_ASSERT(level_node_list.state_is_valid());

    Index node_count = node_group.get_node_count();
    ONEDAL_ASSERT(data.get_count() == ctx.row_count_ * ctx.column_count_);
    ONEDAL_ASSERT(response.get_count() == ctx.row_count_);
    ONEDAL_ASSERT(tree_order.get_count() == ctx.tree_in_block_ * ctx.selected_row_total_count_);
    ONEDAL_ASSERT(selected_ftr_list.get_count() >= node_count * ctx.selected_ftr_count_);
    ONEDAL_ASSERT(bin_offset_list.get_count() == ctx.column_count_ + 1);
    ONEDAL_ASSERT(imp_data_list.imp_list_.get_count() >=
                  node_count * impl_const_t::node_imp_prop_count_);
    if constexpr (std::is_same_v<Task, task::classification>) {
        ONEDAL_ASSERT(imp_data_list.class_hist_list_.get_count() >= node_count * ctx.class_count_);
    }

    ONEDAL_ASSERT(level_node_list.get_list().get_count() >=
                  node_count * impl_const_t::node_prop_count_);
    ONEDAL_ASSERT(left_child_imp_data_list.imp_list_.get_count() >=
                  node_count * impl_const_t::node_imp_prop_count_);
    if constexpr (std::is_same_v<Task, task::classification>) {
        ONEDAL_ASSERT(left_child_imp_data_list.class_hist_list_.get_count() >=
                      node_count * ctx.class_count_);
    }

    if (update_imp_dec_required) {
        ONEDAL_ASSERT(node_imp_dec_list.get_count() >= node_count);
    }

    const Bin* data_ptr = data.get_data();
    const Float* response_ptr = response.get_data();
    const Index* tree_order_ptr = tree_order.get_data();

    const Index* selected_ftr_list_ptr = selected_ftr_list.get_data();

    imp_data_list_ptr<Float, Index, Task> imp_list_ptr(imp_data_list);

    const Index* node_indices_ptr = node_group.get_node_indices_list_ptr();
    Index* node_list_ptr = level_node_list.get_list().get_mutable_data();
    Float* node_imp_decr_list_ptr =
        update_imp_dec_required ? node_imp_dec_list.get_mutable_data() : nullptr;

    imp_data_list_ptr_mutable<Float, Index, Task> left_imp_list_ptr(left_child_imp_data_list);

    const Index column_count = ctx.column_count_;

    const Index selected_ftr_count = ctx.selected_ftr_count_;

    const Index index_max = ctx.index_max_;

    Index max_wg_size = bk::device_max_wg_size(queue);

    Index required_sbg_count = std::min(bk::down_pow2(selected_ftr_count), sbg_size);
    Index local_size =
        std::max(min_local_size_, std::min(max_wg_size, required_sbg_count * sbg_size));

    Index target_sbg_count = (local_size / sbg_size);

    std::size_t hist_buf_size =
        hist_prop_count * 2; // x2 because of one hist for best split and another for test split
    std::size_t local_buf_byte_size =
        target_sbg_count *
        (hist_buf_size * sizeof(hist_type_t) + split_info_t::get_cache_without_hist_byte_size());

    sycl::event last_event;

    const Index class_count = ctx.class_count_;
    const Float imp_threshold = ctx.impurity_threshold_;
    const Index min_obs_leaf = ctx.min_observations_in_leaf_node_;

    const Float* node_imp_list_ptr = imp_list_ptr.imp_list_ptr_;
    Float* left_child_imp_list_ptr = left_imp_list_ptr.imp_list_ptr_;

    // following vars are not used for regression, but should present to compile kernel
    const Index* class_hist_list_ptr = imp_list_ptr.get_class_hist_list_ptr_or_null();
    Index* left_child_class_hist_list_ptr = left_imp_list_ptr.get_class_hist_list_ptr_or_null();

    Index node_in_block_count = max_wg_count_;

    ONEDAL_ASSERT(device_has_enough_local_mem(queue, local_buf_byte_size));
    // TODO: add separate branch to process situation when there isn't enough local mem

    for (Index processed_node_cnt = 0; processed_node_cnt < node_count;
         processed_node_cnt += node_in_block_count) {
        const sycl::nd_range<2> nd_range =
            bk::make_multiple_nd_range_2d({ local_size, max_wg_count_ }, { local_size, 1 });

        last_event = queue.submit([&](sycl::handler& cgh) {
            cgh.depends_on(deps);
            local_accessor_rw_t<byte_t> local_byte_buf(local_buf_byte_size, cgh);

            cgh.parallel_for(
                nd_range,
                [=](sycl::nd_item<2> item) [[intel::reqd_sub_group_size(sbg_size)]] {
                    auto sbg = item.get_sub_group();

                    const Index node_idx = item.get_global_id(1);

                    if ((processed_node_cnt + node_idx) > (node_count - 1)) {
                        return;
                    }

                    const Index node_id = node_indices_ptr[processed_node_cnt + node_idx];
                    Index* node_ptr = node_list_ptr + node_id * impl_const_t::node_prop_count_;

                    const Index sub_group_id = sbg.get_group_id();
                    const Index sub_group_local_id = sbg.get_local_id();

                    const Index row_ofs = node_ptr[impl_const_t::ind_ofs];
                    const Index row_count = node_ptr[impl_const_t::ind_lrc];

                    split_smp_t sp_hlp;
                    split_info<Float, Index, Task> bs;

                    // slm pointers declaration
                    byte_t* local_byte_buf_ptr = local_byte_buf.get_pointer().get();

                    hist_type_t* local_bs_hist_buf_ptr =
                        get_buf_ptr<hist_type_t>(&local_byte_buf_ptr,
                                                 target_sbg_count * hist_prop_count);
                    hist_type_t* local_ts_hist_buf_ptr =
                        get_buf_ptr<hist_type_t>(&local_byte_buf_ptr,
                                                 target_sbg_count * hist_prop_count);

                    bs.init_clear(item,
                                  local_bs_hist_buf_ptr + sub_group_id * hist_prop_count,
                                  hist_prop_count);

                    for (Index ftr_idx = sub_group_id; ftr_idx < selected_ftr_count;
                         ftr_idx += target_sbg_count) {
                        split_info<Float, Index, Task> ts;
                        ts.init(local_ts_hist_buf_ptr + sub_group_id * hist_prop_count,
                                hist_prop_count);
                        ts.ftr_id = selected_ftr_list_ptr[node_id * selected_ftr_count + ftr_idx];

                        Index i = sub_group_local_id;
                        Index id = (i < row_count) ? tree_order_ptr[row_ofs + i] : index_max;
                        Index bin =
                            (i < row_count) ? data_ptr[id * column_count + ts.ftr_id] : index_max;
                        Float response = (i < row_count) ? response_ptr[id] : Float(0);
                        Index response_int = (i < row_count) ? static_cast<Index>(response) : -1;

                        ts.ftr_bin = -1;

                        while ((ts.ftr_bin =
                                    sycl::reduce_over_group(sbg,
                                                            bin > ts.ftr_bin ? bin : index_max,
                                                            minimum<Index>())) < index_max) {
                            const Index count = (bin <= ts.ftr_bin) ? 1 : 0;

                            ts.left_count = sycl::reduce_over_group(sbg, count, plus<Index>());

                            if constexpr (std::is_same_v<Task, task::classification>) {
                                const Index val = (bin <= ts.ftr_bin) ? response_int : -1;
                                Index all_class_count = 0;

                                for (Index class_id = 0; class_id < class_count - 1; class_id++) {
                                    Index total_class_count =
                                        sycl::reduce_over_group(sbg,
                                                                Index(class_id == val),
                                                                plus<Index>());
                                    all_class_count += total_class_count;
                                    ts.left_hist[class_id] = total_class_count;
                                }

                                ts.left_hist[class_count - 1] = ts.left_count - all_class_count;
                            }
                            else {
                                const Float val = (bin <= ts.ftr_bin) ? response : Float(0);
                                const Float sum = sycl::reduce_over_group(sbg, val, plus<Float>());

                                const Float mean = sum / ts.left_count;
                                ts.right_count = row_count - ts.left_count;

                                Float val_s2c =
                                    (bin <= ts.ftr_bin) ? (val - mean) * (val - mean) : Float(0);

                                Float sum2cent =
                                    sycl::reduce_over_group(sbg, val_s2c, plus<Float>());

                                ts.left_hist[0] = ts.left_count;
                                ts.left_hist[1] = mean;
                                ts.left_hist[2] = sum2cent;
                            }

                            if (sub_group_local_id == 0) {
                                if constexpr (std::is_same_v<Task, task::classification>) {
                                    sp_hlp.calc_imp_dec(ts,
                                                        node_ptr,
                                                        node_imp_list_ptr,
                                                        class_hist_list_ptr,
                                                        class_count,
                                                        node_id);
                                    sp_hlp.choose_best_split(bs,
                                                             ts,
                                                             node_imp_list_ptr,
                                                             class_count,
                                                             node_id,
                                                             imp_threshold,
                                                             min_obs_leaf);
                                }
                                else {
                                    sp_hlp.calc_imp_dec(ts, node_ptr, node_imp_list_ptr, node_id);
                                    sp_hlp.choose_best_split(bs,
                                                             ts,
                                                             node_imp_list_ptr,
                                                             impl_const_t::hist_prop_count_,
                                                             node_id,
                                                             imp_threshold,
                                                             min_obs_leaf);
                                }
                            }
                        }
                    }

                    byte_t* local_buf_ptr = get_buf_ptr<byte_t>(
                        &local_byte_buf_ptr,
                        target_sbg_count * split_info_t::get_cache_without_hist_byte_size());

                    if (sub_group_local_id == 0) {
                        bs.store_without_hist(local_buf_ptr, sub_group_id, target_sbg_count);
                    }
                    item.barrier(sycl::access::fence_space::local_space);

                    if (sub_group_id == 0) {
                        bs.load_without_hist(local_buf_ptr,
                                             sub_group_local_id % target_sbg_count,
                                             target_sbg_count);
                        bs.set_left_hist_ptr(local_bs_hist_buf_ptr +
                                             (sub_group_local_id % target_sbg_count) *
                                                 bs.hist_prop_count);

                        if (sp_hlp
                                .my_split_is_best_for_sbg(item, bs, node_ptr, node_id, index_max)) {
                            sp_hlp.update_node_bs_info(bs,
                                                       node_ptr,
                                                       node_imp_decr_list_ptr,
                                                       node_id,
                                                       index_max,
                                                       update_imp_dec_required);
                            if constexpr (std::is_same_v<Task, task::classification>) {
                                sp_hlp.update_left_child_imp(left_child_imp_list_ptr,
                                                             left_child_class_hist_list_ptr,
                                                             bs.left_imp,
                                                             bs.left_hist,
                                                             node_id,
                                                             class_count);
                            }
                            else {
                                sp_hlp.update_left_child_imp(left_child_imp_list_ptr,
                                                             bs.left_hist,
                                                             node_id);
                            }
                        }
                    }
                });
        });

        last_event.wait_and_throw();
    }

    return last_event;
}

#define INSTANTIATE(F, B, I, T) template class train_splitter_sp_opt_impl<F, B, I, T>;

INSTANTIATE(float, std::uint32_t, std::int32_t, task::classification);
INSTANTIATE(float, std::uint32_t, std::int32_t, task::regression);

INSTANTIATE(double, std::uint32_t, std::int32_t, task::classification);
INSTANTIATE(double, std::uint32_t, std::int32_t, task::regression);

} // namespace oneapi::dal::decision_forest::backend

#endif
