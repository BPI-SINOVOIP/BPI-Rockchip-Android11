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

import static org.junit.Assert.*;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link ModuleParametersHelper}. */
@RunWith(JUnit4.class)
public class ModuleParametersHelperTest {

    /** Check that each values in {@link ModuleParameters} has a handler associated. */
    @Test
    public void testHandlersExists() {
        for (ModuleParameters param : ModuleParameters.values()) {
            IModuleParameter handler =
                    ModuleParametersHelper.getParameterHandler(param, /* Include optional */ true);
            assertNotNull(handler);
        }
    }
}
