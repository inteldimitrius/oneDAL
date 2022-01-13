/* file: predict_result.cpp */
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

#include <jni.h>

#include "com_intel_daal_algorithms_elastic_net_prediction_PredictionResult.h"

#include "com/intel/daal/common_helpers.h"

#include "com_intel_daal_algorithms_elastic_net_prediction_PredictionResultId.h"
#define defaultDenseValue com_intel_daal_algorithms_elastic_net_prediction_PredictionMethod_defaultDenseValue
#define predictionId      com_intel_daal_algorithms_elastic_net_prediction_PredictionResultId_PredictionId

USING_COMMON_NAMESPACES();
using namespace daal::algorithms::elastic_net::prediction;

/*
 * Class:     com_intel_daal_algorithms_elastic_net_prediction_PredictionResult
 * Method:    cNewResult
 * Signature:()J
 */
JNIEXPORT jlong JNICALL Java_com_intel_daal_algorithms_elastic_1net_prediction_PredictionResult_cNewResult(JNIEnv * env, jobject thisObj)
{
    return jniArgument<elastic_net::prediction::Result>::newObj();
}

/*
 * Class:     com_intel_daal_algorithms_elastic_net_prediction_PredictionResult
 * Method:    cGetPredictionResult
 * Signature:(JI)J
 */
JNIEXPORT jlong JNICALL Java_com_intel_daal_algorithms_elastic_1net_prediction_PredictionResult_cGetPredictionResult(JNIEnv * env, jobject thisObj,
                                                                                                                     jlong resAddr, jint id)
{
    return jniArgument<elastic_net::prediction::Result>::get<elastic_net::prediction::ResultId, NumericTable>(resAddr, id);
}

/*
 * Class:     com_intel_daal_algorithms_elastic_net_prediction_PredictionResult
 * Method:    cSetPredictionResult
 * Signature:(JIJ)V
 */
JNIEXPORT void JNICALL Java_com_intel_daal_algorithms_elastic_1net_prediction_PredictionResult_cSetPredictionResult(JNIEnv * env, jobject thisObj,
                                                                                                                    jlong resAddr, jint id,
                                                                                                                    jlong ntAddr)
{
    jniArgument<elastic_net::prediction::Result>::set<elastic_net::prediction::ResultId, NumericTable>(resAddr, id, ntAddr);
}
