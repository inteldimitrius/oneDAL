/* file: ridge_regression_training_distributed.h */
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
//  Implementation of the interface for ridge regression model-based training in the distributed processing mode
//--
*/

#ifndef __RIDGE_REGRESSION_TRAINING_DISTRIBUTED_H__
#define __RIDGE_REGRESSION_TRAINING_DISTRIBUTED_H__

#include "algorithms/algorithm.h"
#include "data_management/data/numeric_table.h"
#include "services/daal_defines.h"
#include "services/daal_memory.h"
#include "algorithms/ridge_regression/ridge_regression_training_types.h"
#include "algorithms/ridge_regression/ridge_regression_training_online.h"
#include "algorithms/ridge_regression/ridge_regression_model.h"

namespace daal
{
namespace algorithms
{
namespace ridge_regression
{
namespace training
{
namespace interface1
{
/**
 * @defgroup ridge_regression_distributed Distributed
 * @ingroup ridge_regression_training
 * @{
 */
/**
 * <a name="DAAL-CLASS-ALGORITHMS__RIDGE_REGRESSION__TRAINING__DISTRIBUTEDCONTAINER"></a>
 * \brief Class containing methods for ridge regression model-based training in the distributed processing mode
 */
template <ComputeStep step, typename algorithmFPType, Method method, CpuType cpu>
class DistributedContainer
{};

/**
 * <a name="DAAL-CLASS-ALGORITHMS__RIDGE_REGRESSION__TRAINING__DISTRIBUTEDCONTAINER_STEP2MASTER_ALGORITHMFPTYPE_METHOD_CPU"></a>
 * \brief Class containing methods for ridge regression model-based training in the second step of the distributed processing mode
 */
template <typename algorithmFPType, Method method, CpuType cpu>
class DistributedContainer<step2Master, algorithmFPType, method, cpu> : public TrainingContainerIface<distributed>
{
public:
    /**
     * Constructs a container for ridge regression model-based training with a specified environment in the distributed processing mode
     * \param[in] daalEnv   Environment object
     */
    DistributedContainer(daal::services::Environment::env * daalEnv);

    /** Default destructor */
    ~DistributedContainer();

    /**
     * Computes a partial result of ridge regression model-based training in the second step of the distributed processing mode
     *
     * \return Status of computations
     */
    services::Status compute() DAAL_C11_OVERRIDE;

    /**
     * Computes the result of ridge regression model-based training in the second step of the distributed processing mode
     *
     * \return Status of computations
     */
    services::Status finalizeCompute() DAAL_C11_OVERRIDE;
};

/**
 * <a name="DAAL-CLASS-ALGORITHMS__RIDGE_REGRESSION__TRAINING__DISTRIBUTED"></a>
 * \brief Provides methods for ridge regression model-based training in the distributed processing mode
 * <!-- \n<a href="DAAL-REF-RIDGEREGRESSION-ALGORITHM">Ridge regression algorithm description and usage models</a> -->
 *
 * \tparam step             Step of the algorithm in the distributed processing mode, \ref ComputeStep
 * \tparam algorithmFPType  Data type to use in intermediate computations for ridge regression model-based training, double or float
 * \tparam method           Ridge regression model-based training method, \ref Method
 *
 * \par Enumerations
 *      - \ref Method  Computation methods
 *
 * \par References
 *      - \ref interface1::Parameter "Parameter" class
 *      - \ref ridge_regression::interface1::Model "ridge_regression::Model" class
 *      - \ref ridge_regression::interface1::ModelNormEq "ridge_regression::ModelNormEq" class
 *      - \ref prediction::interface1::Batch "prediction::Batch" class
 */
template <ComputeStep step, typename algorithmFPType = DAAL_ALGORITHM_FP_TYPE, Method method = normEqDense>
class DAAL_EXPORT Distributed : public Training<distributed>
{};

/**
 * <a name="DAAL-CLASS-ALGORITHMS__RIDGE_REGRESSION__TRAINING__DISTRIBUTED_STEP1LOCAL_ALGORITHMFPTYPE_METHOD"></a>
 * \brief Performs ridge regression model-based training in the the first step of the distributed processing mode
 * <!-- \n<a href="DAAL-REF-RIDGEREGRESSION-ALGORITHM">Ridge regression algorithm description and usage models</a> -->
 *
 * \tparam algorithmFPType  Data type to use in intermediate computations for ridge regression model-based training, double or float
 * \tparam method           Ridge regression training method, \ref Method
 *
 * \par Enumerations
 *      -  \ref Method  Computation methods
 *
 * \par References
 *      - \ref interface1::Parameter "Parameter" class
 *      - \ref ridge_regression::interface1::Model "ridge_regression::Model" class
 *      - \ref ridge_regression::interface1::ModelNormEq "ridge_regression::ModelNormEq" class
 *      - \ref prediction::interface1::Batch "prediction::Batch" class
 */
template <typename algorithmFPType, Method method>
class DAAL_EXPORT Distributed<step1Local, algorithmFPType, method> : public Online<algorithmFPType, method>
{
public:
    typedef Online<algorithmFPType, method> super;

    typedef typename super::InputType InputType;
    typedef typename super::ParameterType ParameterType;
    typedef typename super::ResultType ResultType;
    typedef typename super::PartialResultType PartialResultType;

    /** Default constructor */
    Distributed<step1Local, algorithmFPType, method>() : Online<algorithmFPType, method>() {}

    /**
     * Constructs a ridge regression training algorithm in the first step of the distributed processing mode by copying input objects and parameters
     * of another ridge regression training algorithm
     * \param[in] other Algorithm to use as the source to initialize the input objects and parameters of the algorithm
     */
    Distributed(const Distributed<step1Local, algorithmFPType, method> & other) : Online<algorithmFPType, method>(other) {}

    ~Distributed() {}

    /**
     * Returns a pointer to a newly allocated ridge regression training algorithm with a copy of the input objects and parameters of this ridge
     * regression training algorithm in the first step of the distributed processing mode
     * \return Pointer to the newly allocated algorithm
     */
    services::SharedPtr<Distributed<step1Local, algorithmFPType, method> > clone() const
    {
        return services::SharedPtr<Distributed<step1Local, algorithmFPType, method> >(cloneImpl());
    }

protected:
    virtual Distributed<step1Local, algorithmFPType, method> * cloneImpl() const DAAL_C11_OVERRIDE
    {
        return new Distributed<step1Local, algorithmFPType, method>(*this);
    }

private:
    Distributed & operator=(const Distributed &);
}; // class  : public Training

/**
 * <a name="DAAL-CLASS-ALGORITHMS__RIDGE_REGRESSION__TRAINING__DISTRIBUTED_STEP2MASTER_ALGORITHMFPTYPE_METHOD"></a>
 * \brief Performs ridge regression model-based training in the the second step of distributed processing mode
 * <!-- \n<a href="DAAL-REF-RIDGEREGRESSION-ALGORITHM">Ridge regression algorithm description and usage models</a> -->
 *
 * \tparam algorithmFPType  Data type to use in intermediate computations for ridge regression model-based training, double or float
 * \tparam method           Ridge regression training method, \ref Method
 *
 * \par Enumerations
 *      -  \ref Method  Computation methods
 *
 * \par References
 *      - \ref ridge_regression::interface1::Model "ridge_regression::Model" class
 *      - \ref ridge_regression::interface1::ModelNormEq "ridge_regression::ModelNormEq" class
 *      - \ref prediction::interface1::Batch "prediction::Batch" class
 */
template <typename algorithmFPType, Method method>
class DAAL_EXPORT Distributed<step2Master, algorithmFPType, method> : public Training<distributed>
{
public:
    typedef algorithms::ridge_regression::training::DistributedInput<step2Master> InputType;
    typedef algorithms::ridge_regression::TrainParameter ParameterType;
    typedef algorithms::ridge_regression::training::Result ResultType;
    typedef algorithms::ridge_regression::training::PartialResult PartialResultType;

    /** Default constructor */
    Distributed() { initialize(); }

    /**
     * Constructs a ridge regression training algorithm in the second step of the distributed processing mode by copying input objects and parameters
     * of another ridge regression training algorithm
     * \param[in] other An algorithm to be used as the source to initialize the input objects and parameters of the algorithm
     */
    Distributed(const Distributed<step2Master, algorithmFPType, method> & other) : input(other.input), parameter(other.parameter) { initialize(); }

    ~Distributed() {}

    /**
    * Returns the method of the algorithm
    * \return Method of the algorithm
    */
    virtual int getMethod() const DAAL_C11_OVERRIDE { return (int)method; }

    /**
     * Registers user-allocated memory to store a partial result of ridge regression model-based training
     * \param[in] partialResult    Structure to store a partial result of ridge regression model-based training
     *
     * \return Status of computations
     */
    services::Status setPartialResult(const PartialResultPtr & partialResult)
    {
        DAAL_CHECK(partialResult, services::ErrorNullPartialResult);
        _partialResult = partialResult;
        _pres          = _partialResult.get();
        return services::Status();
    }

    /**
     * Returns the structure that contains a partial result of ridge regression model-based training
     * \return Structure that contains a partial result of ridge regression model-based training
     */
    PartialResultPtr getPartialResult() { return _partialResult; }

    /**
     * Registers user-allocated memory to store the result of ridge regression model-based training
     * \param[in] res    Structure to store the result of ridge regression model-based training
     *
     * \return Status of computations
     */
    services::Status setResult(const ResultPtr & res)
    {
        DAAL_CHECK(res, services::ErrorNullResult)
        _result = res;
        _res    = _result.get();
        return services::Status();
    }

    /**
     * Returns the structure that contains the result of ridge regression model-based training in the second step of the distributed processing mode
     * \return Structure that contains the result of ridge regression model-based training in the second step of the distributed processing mode
     */
    ResultPtr getResult() { return _result; }

    /**
     * Returns a pointer to a newly allocated ridge regression training algorithm with a copy of the input objects and parameters of this ridge
     * regression training algorithm in the second step of the distributed processing mode
     * \return Pointer to the newly allocated algorithm
     */
    services::SharedPtr<Distributed<step2Master, algorithmFPType, method> > clone() const
    {
        return services::SharedPtr<Distributed<step2Master, algorithmFPType, method> >(cloneImpl());
    }

    DistributedInput<step2Master> input; /*!< %Input data structure */
    ParameterType parameter;             /*!< %Training \ref interface1::Parameter "parameters" */

protected:
    PartialResultPtr _partialResult;
    ResultPtr _result;

    virtual Distributed<step2Master, algorithmFPType, method> * cloneImpl() const DAAL_C11_OVERRIDE
    {
        return new Distributed<step2Master, algorithmFPType, method>(*this);
    }

    services::Status allocateResult() DAAL_C11_OVERRIDE
    {
        services::Status s = _result->allocate<algorithmFPType>(_pres, &parameter, method);
        _res               = _result.get();
        return s;
    }

    services::Status allocatePartialResult() DAAL_C11_OVERRIDE
    {
        services::Status s = _partialResult->allocate<algorithmFPType>(&input, &parameter, method);
        _pres              = _partialResult.get();
        return s;
    }

    services::Status initializePartialResult() DAAL_C11_OVERRIDE { return services::Status(); }

    void initialize()
    {
        _ac  = new __DAAL_ALGORITHM_CONTAINER(distributed, DistributedContainer, step2Master, algorithmFPType, method)(&_env);
        _in  = &input;
        _par = &parameter;
        _partialResult.reset(new PartialResultType());
        _result.reset(new ResultType());
    }

private:
    Distributed & operator=(const Distributed &);
}; // class  : public Training
/** @} */
} // namespace interface1

using interface1::DistributedContainer;
using interface1::Distributed;

} // namespace training
} // namespace ridge_regression
} // namespace algorithms
} // namespace daal

#endif
