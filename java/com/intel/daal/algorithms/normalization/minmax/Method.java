/* file: Method.java */
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
 * @ingroup minmax
 * @{
 */
package com.intel.daal.algorithms.normalization.minmax;

import com.intel.daal.utils.*;
/**
 * <a name="DAAL-CLASS-ALGORITHMS__NORMALIZATION__MINMAX__METHOD"></a>
 * @brief Available methods for Min-max normalization
 */
public final class Method {
    /** @private */
    static {
        LibUtils.loadLibrary();
    }

    private int _value;

    /**
     * Constructs the method object using the provided value
     * @param value     Value corresponding to the method object
     */
    public Method(int value) {
        _value = value;
    }

    /**
     * Returns the value corresponding to the method object
     * @return Value corresponding to the method object
     */
    public int getValue() {
        return _value;
    }

    private static final int DefaultDense = 0;
    public static final Method defaultDense = new Method(DefaultDense); /*!< Default: performance-oriented method.
                                                                          Works with all types  of input numeric tables */
}
/** @} */
