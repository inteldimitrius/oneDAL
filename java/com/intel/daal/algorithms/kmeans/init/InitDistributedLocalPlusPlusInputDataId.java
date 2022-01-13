/* file: InitDistributedLocalPlusPlusInputDataId.java */
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

/**
 * @ingroup kmeans_init_distributed
 * @{
 */
package com.intel.daal.algorithms.kmeans.init;

/**
 * <a name="DAAL-CLASS-ALGORITHMS__KMEANS__INIT__INITDISTRIBUTEDLOCALPLUSPLUSINPUTDATAID"></a>
 * @brief Available identifiers of input objects for computing initial clusters for the K-Means algorithm
 *        used with plusPlus and parallelPlus methods only on a local node.
 */
public final class InitDistributedLocalPlusPlusInputDataId {
    private int _value;

    /**
     * Constructs the initialization input object identifier using the provided value
     * @param value     Value corresponding to the initialization input object identifier
     */
    public InitDistributedLocalPlusPlusInputDataId(int value) {
        _value = value;
    }

    /**
     * Returns the value corresponding to the initialization input object identifier
     * @return Value corresponding to the initialization input object identifier
     */
    public int getValue() {
        return _value;
    }

    private static final int internalInputValue = 1;

    /** DataCollection with internal algorithm data calculated by previous steps on this node */
    public static final InitDistributedLocalPlusPlusInputDataId internalInput = new InitDistributedLocalPlusPlusInputDataId(
            internalInputValue);
}
/** @} */
