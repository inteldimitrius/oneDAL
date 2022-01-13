/* file: mcg59_batch_container.h */
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
//  Implementation of mcg59 calculation algorithm container.
//--
*/

#ifndef __MCG59_BATCH_CONTAINER_H__
#define __MCG59_BATCH_CONTAINER_H__

#include "algorithms/engines/mcg59/mcg59.h"
#include "src/algorithms/engines/mcg59/mcg59_kernel.h"

namespace daal
{
namespace algorithms
{
namespace engines
{
namespace mcg59
{
namespace interface1
{
template <typename algorithmFPType, Method method, CpuType cpu>
BatchContainer<algorithmFPType, method, cpu>::BatchContainer(daal::services::Environment::env * daalEnv) : AnalysisContainerIface<batch>(daalEnv)
{
    __DAAL_INITIALIZE_KERNELS(internal::Mcg59Kernel, algorithmFPType, method);
}

template <typename algorithmFPType, Method method, CpuType cpu>
BatchContainer<algorithmFPType, method, cpu>::~BatchContainer()
{
    __DAAL_DEINITIALIZE_KERNELS();
}

template <typename algorithmFPType, Method method, CpuType cpu>
services::Status BatchContainer<algorithmFPType, method, cpu>::compute()
{
    daal::services::Environment::env & env = *_env;
    engines::Result * result               = static_cast<engines::Result *>(_res);
    NumericTable * resultTable             = result->get(engines::randomNumbers).get();

    __DAAL_CALL_KERNEL(env, internal::Mcg59Kernel, __DAAL_KERNEL_ARGUMENTS(algorithmFPType, method), compute, resultTable);
}

} // namespace interface1
} // namespace mcg59
} // namespace engines
} // namespace algorithms
} // namespace daal

#endif
