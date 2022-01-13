/* file: train_input.cpp */
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
#include <jni.h>/* Header for class com_intel_daal_algorithms_classifier_training_TrainingInput */

#include "daal.h"
#include "com_intel_daal_algorithms_classifier_training_TrainingInput.h"

#include "com/intel/daal/common_helpers.h"

USING_COMMON_NAMESPACES();
using namespace daal::algorithms::classifier;
using namespace daal::algorithms::classifier::training;

#include "com/intel/daal/common_defines.i"

#include "com_intel_daal_algorithms_classifier_training_InputId.h"
#define Data    com_intel_daal_algorithms_classifier_training_InputId_Data
#define Labels  com_intel_daal_algorithms_classifier_training_InputId_Labels
#define Weights com_intel_daal_algorithms_classifier_training_InputId_Weights

/*
 * Class:     com_intel_daal_algorithms_classifier_training_TrainingInput
 * Method:    cInit
 * Signature: (JIJ)I
 */
JNIEXPORT jlong JNICALL Java_com_intel_daal_algorithms_classifier_training_TrainingInput_cInit(JNIEnv * env, jobject thisObj, jlong algAddr,
                                                                                               jint cmode)
{
    classifier::training::Input * inputPtr = NULL;

    if (cmode == jBatch)
    {
        SharedPtr<Batch> alg = staticPointerCast<Batch, AlgorithmIface>(*(SharedPtr<AlgorithmIface> *)algAddr);
        inputPtr             = alg->getInput();
    }
    else if (cmode == jOnline)
    {
        SharedPtr<Online> alg = staticPointerCast<Online, AlgorithmIface>(*(SharedPtr<AlgorithmIface> *)algAddr);
        inputPtr              = alg->getInput();
    }

    return (jlong)inputPtr;
}

/*
 * Class:     Java_com_intel_daal_algorithms_classifier_training_TrainingInput
 * Method:    cSetInput
 * Signature:(JIJ)I
 */
JNIEXPORT void JNICALL Java_com_intel_daal_algorithms_classifier_training_TrainingInput_cSetInput(JNIEnv * env, jobject thisObj, jlong inputAddr,
                                                                                                  jint id, jlong ntAddr)
{
    if (id != Data && id != Labels) return;

    jniInput<classifier::training::Input>::set<classifier::training::InputId, NumericTable>(inputAddr, id, ntAddr);
}

/*
 * Class:     com_intel_daal_algorithms_classifier_training_TrainingInput
 * Method:    cGetInputTable
 * Signature: (JI)J
 */
JNIEXPORT jlong JNICALL Java_com_intel_daal_algorithms_classifier_training_TrainingInput_cGetInputTable(JNIEnv * env, jobject thisObj,
                                                                                                        jlong inputAddr, jint id)
{
    if (id != Data && id != Labels) return (jlong)-1;

    return jniInput<classifier::training::Input>::get<classifier::training::InputId, NumericTable>(inputAddr, id);
}
