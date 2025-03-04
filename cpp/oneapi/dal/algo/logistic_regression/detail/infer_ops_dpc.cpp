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

#include "oneapi/dal/algo/logistic_regression/backend/cpu/infer_kernel.hpp"
#include "oneapi/dal/algo/logistic_regression/backend/gpu/infer_kernel.hpp"
#include "oneapi/dal/algo/logistic_regression/detail/infer_ops.hpp"
#include "oneapi/dal/backend/dispatcher.hpp"

namespace oneapi::dal::logistic_regression::detail {
namespace v1 {

template <typename Policy, typename Float, typename Method, typename Task>
struct infer_ops_dispatcher<Policy, Float, Method, Task> {
    infer_result<Task> operator()(const Policy& ctx,
                                  const descriptor_base<Task>& params,
                                  const infer_input<Task>& input) const {
        using kernel_dispatcher_t = dal::backend::kernel_dispatcher<
            KERNEL_SINGLE_NODE_CPU(backend::infer_kernel_cpu<Float, Method, Task>),
            KERNEL_SINGLE_NODE_GPU(backend::infer_kernel_gpu<Float, Method, Task>)>;
        return kernel_dispatcher_t{}(ctx, params, input);
    }
};

#define INSTANTIATE(F, M, T) \
    template struct ONEDAL_EXPORT infer_ops_dispatcher<dal::detail::data_parallel_policy, F, M, T>;

INSTANTIATE(float, method::dense_batch, task::classification)
INSTANTIATE(double, method::dense_batch, task::classification)

} // namespace v1
} // namespace oneapi::dal::logistic_regression::detail
