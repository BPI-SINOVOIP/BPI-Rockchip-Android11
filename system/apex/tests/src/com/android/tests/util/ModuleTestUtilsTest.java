/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.tests.util;

import static com.google.common.truth.Truth.assertWithMessage;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;

@RunWith(DeviceJUnit4ClassRunner.class)
public class ModuleTestUtilsTest extends BaseHostJUnit4Test {
    private static final String SHIM_V2 = "com.android.apex.cts.shim.v2.apex";
    private final ModuleTestUtils mUtils = new ModuleTestUtils(this);

    @Before
    public void setUp() throws Exception {
        mUtils.abandonActiveStagedSession();
        mUtils.uninstallShimApexIfNecessary();
    }
    /**
     * Uninstalls any version greater than 1 of shim apex and reboots the device if necessary
     * to complete the uninstall.
     */
    @After
    public void tearDown() throws Exception {
        mUtils.abandonActiveStagedSession();
        mUtils.uninstallShimApexIfNecessary();
    }

    /**
     * Unit test for {@link ModuleTestUtils#abandonActiveStagedSession()}
     */
    @Test
    public void testAbandonActiveStagedSession() throws Exception {
        File apexFile = mUtils.getTestFile(SHIM_V2);

        // Install apex package
        String installResult = getDevice().installPackage(apexFile, false, "--wait");
        assertWithMessage(String.format("failed to install apex first time %s. Reason: %s",
                        SHIM_V2, installResult)).that(installResult).isNull();

        // Abandon ready session
        mUtils.abandonActiveStagedSession();

        // Install apex again
        installResult = getDevice().installPackage(apexFile, false, "--wait");
        assertWithMessage(String.format("failed to install apex again %s. Reason: %s",
                SHIM_V2, installResult)).that(installResult).isNull();
    }
}
