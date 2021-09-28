/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.cts.host.blob;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
public class InstantAppAccessTest extends BaseBlobStoreHostTest {
    private static final String TEST_CLASS = TARGET_PKG + ".InstantAppAccessTest";

    @Before
    public void setUp() throws Exception {
        uninstallPackage(TARGET_PKG);
        installPackage(TARGET_APK, "--instant");
    }

    @After
    public void tearDown() throws Exception {
        uninstallPackage(TARGET_PKG);
    }

    @Test
    public void testInstantAppAccess() throws Exception {
        runDeviceTest(TARGET_PKG, TEST_CLASS, "testInstantAppAccess");
    }
}
