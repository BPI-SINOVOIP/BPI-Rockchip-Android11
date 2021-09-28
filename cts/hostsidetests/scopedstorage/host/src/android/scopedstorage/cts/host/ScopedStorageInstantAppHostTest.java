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

package android.scopedstorage.cts.host;

import static org.junit.Assert.assertTrue;

import android.platform.test.annotations.AppModeInstant;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Runs the ScopedStorageTest tests for an instant app.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class ScopedStorageInstantAppHostTest extends BaseHostJUnit4Test {
    /**
     * Runs the given phase of Test by calling into the device.
     * Throws an exception if the test phase fails.
     */
    protected void runDeviceTest(String phase) throws Exception {
        assertTrue(runDeviceTests("android.scopedstorage.cts",
                "android.scopedstorage.cts.ScopedStorageTest", phase));
    }

    @Test
    @AppModeInstant
    public void testInstantAppsCantAccessExternalStorage() throws Exception {
        runDeviceTest("testInstantAppsCantAccessExternalStorage");
    }
}
