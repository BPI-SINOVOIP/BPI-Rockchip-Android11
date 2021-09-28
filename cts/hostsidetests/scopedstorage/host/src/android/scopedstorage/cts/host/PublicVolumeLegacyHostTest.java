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

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
public class PublicVolumeLegacyHostTest extends LegacyStorageHostTest {
    /** Used to clean up the virtual volume after the test */
    private static ITestDevice sDevice = null;
    private boolean mIsPublicVolumeSetup = false;

    private void setupNewPublicVolume() throws Exception {
        if (!mIsPublicVolumeSetup) {
            assertTrue(runDeviceTests("android.scopedstorage.cts",
                    "android.scopedstorage.cts.PublicVolumeTestHelper", "setupNewPublicVolume"));
            mIsPublicVolumeSetup = true;
        }
    }

    private void setupDevice() {
        if (sDevice == null) {
            sDevice = getDevice();
        }
    }

    /**
     * Runs the given phase of PublicVolumeTest by calling into the device.
     * Throws an exception if the test phase fails.
     */
    @Override
    void runDeviceTest(String phase) throws Exception {
        assertTrue(runDeviceTests("android.scopedstorage.cts.legacy",
                "android.scopedstorage.cts.legacy.PublicVolumeLegacyTest", phase));
    }

    @Before
    public void setup() throws Exception {
        setupDevice();
        setupNewPublicVolume();
        super.setup();
    }

    @AfterClass
    public static void deletePublicVolumes() throws Exception {
        if (sDevice != null) {
            sDevice.executeShellCommand("sm set-virtual-disk false");
        }
    }
}
