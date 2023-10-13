/*******************************************************************************
* Copyright 2021 Intel Corporation
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

#include "oneapi/dal/algo/basic_statistics/detail/compute_ops.hpp"
#include "oneapi/dal/algo/basic_statistics/backend/cpu/compute_kernel.hpp"
#include "oneapi/dal/backend/dispatcher.hpp"

namespace oneapi::dal::basic_statistics::detail {
namespace v1 {

template <typename Policy, typename Float, typename Method, typename Task>
struct compute_ops_dispatcher<Policy, Float, Method, Task> {
    using input_t = std::conditional_t<std::is_same_v<Method, method::dense>,
                                       compute_input<Task, table>,
                                       compute_input<Task, csr_table>>;
    compute_result<Task> operator()(const Policy& policy,
                                    const descriptor_base<Task>& desc,
                                    const input_t& input) const {
        using kernel_dispatcher_t = dal::backend::kernel_dispatcher< //
            KERNEL_SINGLE_NODE_CPU(backend::compute_kernel_cpu<Float, Method, Task>)>;
        return kernel_dispatcher_t()(policy, desc, input);
    }
};

#define INSTANTIATE(F, M, T)                                                                 \
    template struct ONEDAL_EXPORT compute_ops_dispatcher<dal::detail::host_policy, F, M, T>; \
    template struct ONEDAL_EXPORT compute_ops_dispatcher<dal::detail::spmd_host_policy, F, M, T>;

INSTANTIATE(float, method::dense, task::compute)
INSTANTIATE(double, method::dense, task::compute)
INSTANTIATE(float, method::sparse, task::compute)
INSTANTIATE(double, method::sparse, task::compute)

} // namespace v1
} // namespace oneapi::dal::basic_statistics::detail
