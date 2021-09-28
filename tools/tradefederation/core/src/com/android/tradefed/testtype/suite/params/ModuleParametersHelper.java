/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.tradefed.testtype.suite.params;

import java.util.HashMap;
import java.util.Map;

/** Helper to get the {@link IModuleParameter} associated with the parameter. */
public class ModuleParametersHelper {

    private static Map<ModuleParameters, IModuleParameter> sHandlerMap = new HashMap<>();

    static {
        sHandlerMap.put(ModuleParameters.INSTANT_APP, new InstantAppHandler());
        sHandlerMap.put(ModuleParameters.NOT_INSTANT_APP, new NegativeHandler());

        sHandlerMap.put(ModuleParameters.MULTI_ABI, new NegativeHandler());
        sHandlerMap.put(ModuleParameters.NOT_MULTI_ABI, new NotMultiAbiHandler());
    }

    /**
     * Optional parameters are params that will not automatically be created when the module
     * parameterization is enabled. They will need to be explicitly enabled. They represent a second
     * set of parameterization that is less commonly requested to run. They could be upgraded to
     * main parameters in the future by moving them above.
     */
    private static Map<ModuleParameters, IModuleParameter> sOptionalHandlerMap = new HashMap<>();

    static {
        sOptionalHandlerMap.put(ModuleParameters.SECONDARY_USER, new SecondaryUserHandler());
        sOptionalHandlerMap.put(ModuleParameters.NOT_SECONDARY_USER, new NegativeHandler());
    }

    /**
     * Returns the {@link IModuleParameter} associated with the requested parameter.
     *
     * @param withOptional Whether or not to also check optional params.
     */
    public static IModuleParameter getParameterHandler(
            ModuleParameters param, boolean withOptional) {
        IModuleParameter value = sHandlerMap.get(param);
        if (value == null && withOptional) {
            return sOptionalHandlerMap.get(param);
        }
        return value;
    }
}
