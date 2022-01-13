/* file: cholesky_batch_container.h */
/*******************************************************************************
* Copyright 2014-2022 Intel Corporation
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

/*
//++
//  Implementation of cholesky calculation algorithm container.
//--
*/

#include "algorithms/cholesky/cholesky.h"
#include "src/algorithms/cholesky/cholesky_kernel.h"

namespace daal
{
namespace algorithms
{
namespace cholesky
{
namespace interface1
{
template <typename algorithmFPType, Method method, CpuType cpu>
BatchContainer<algorithmFPType, method, cpu>::BatchContainer(daal::services::Environment::env * daalEnv) : AnalysisContainerIface<batch>(daalEnv)
{
    __DAAL_INITIALIZE_KERNELS(internal::CholeskyKernel, algorithmFPType, method);
}

template <typename algorithmFPType, Method method, CpuType cpu>
BatchContainer<algorithmFPType, method, cpu>::~BatchContainer()
{
    __DAAL_DEINITIALIZE_KERNELS();
}

template <typename algorithmFPType, Method method, CpuType cpu>
services::Status BatchContainer<algorithmFPType, method, cpu>::compute()
{
    Input * input   = static_cast<Input *>(_in);
    Result * result = static_cast<Result *>(_res);

    daal::algorithms::Parameter * par      = nullptr;
    daal::services::Environment::env & env = *_env;

    __DAAL_CALL_KERNEL(env, internal::CholeskyKernel, __DAAL_KERNEL_ARGUMENTS(algorithmFPType, method), compute, input->get(data).get(),
                       result->get(choleskyFactor).get(), par);
}

} // namespace interface1
} // namespace cholesky
} // namespace algorithms
} // namespace daal
