/* file: binaryconfusionmatrixresult.cpp */
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

/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
#include "daal.h"
#include "com_intel_daal_algorithms_classifier_quality_metric_binary_confusion_matrix_BinaryConfusionMatrixResult.h"

#include "com/intel/daal/common_helpers.h"

#include "com_intel_daal_algorithms_classifier_quality_metric_binary_confusion_matrix_BinaryConfusionMatrixResultId.h"
#define ConfusionMatrix com_intel_daal_algorithms_classifier_quality_metric_binary_confusion_matrix_BinaryConfusionMatrixResultId_ConfusionMatrix
#define BinaryMetrics   com_intel_daal_algorithms_classifier_quality_metric_binary_confusion_matrix_BinaryConfusionMatrixResultId_BinaryMetrics

USING_COMMON_NAMESPACES();
using namespace daal::algorithms::classifier::quality_metric;
using namespace daal::algorithms::classifier::quality_metric::binary_confusion_matrix;

/*
 * Class:     com_intel_daal_algorithms_classifier_quality_metric_binary_confusion_matrix_BinaryConfusionMatrixResult
 * Method:    cSetResultTable
 * Signature: (JIJ)V
 */
JNIEXPORT void JNICALL
    Java_com_intel_daal_algorithms_classifier_quality_1metric_binary_1confusion_1matrix_BinaryConfusionMatrixResult_cSetResultTable(JNIEnv *, jobject,
                                                                                                                                    jlong resAddr,
                                                                                                                                    jint id,
                                                                                                                                    jlong ntAddr)
{
    jniArgument<binary_confusion_matrix::Result>::set<binary_confusion_matrix::ResultId, NumericTable>(resAddr, id, ntAddr);
}

/*
 * Class:     com_intel_daal_algorithms_classifier_quality_metric_binary_confusion_matrix_BinaryConfusionMatrixResult
 * Method:    cGetResultTable
 * Signature: (JI)J
 */
JNIEXPORT jlong JNICALL
    Java_com_intel_daal_algorithms_classifier_quality_1metric_binary_1confusion_1matrix_BinaryConfusionMatrixResult_cGetResultTable(JNIEnv *, jobject,
                                                                                                                                    jlong resAddr,
                                                                                                                                    jint id)
{
    if (id == ConfusionMatrix)
    {
        return jniArgument<binary_confusion_matrix::Result>::get<binary_confusion_matrix::ResultId, NumericTable>(resAddr, confusionMatrix);
    }
    else if (id == BinaryMetrics)
    {
        return jniArgument<binary_confusion_matrix::Result>::get<binary_confusion_matrix::ResultId, NumericTable>(resAddr, binaryMetrics);
    }

    return (jlong)0;
}

JNIEXPORT jlong JNICALL
    Java_com_intel_daal_algorithms_classifier_quality_1metric_binary_1confusion_1matrix_BinaryConfusionMatrixResult_cNewResult(JNIEnv *, jobject)
{
    return jniArgument<binary_confusion_matrix::Result>::newObj();
}
