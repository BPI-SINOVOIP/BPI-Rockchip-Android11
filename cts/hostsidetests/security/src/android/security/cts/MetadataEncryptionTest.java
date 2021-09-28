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

package android.security.cts;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceTestCase;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.compatibility.common.util.CddTest;
import com.android.compatibility.common.util.PropertyUtil;

/**
 * Host-side test for metadata encryption.  This is a host-side test because
 * the "ro.crypto.metadata.enabled" property is not exposed to apps.
 */
public class MetadataEncryptionTest extends DeviceTestCase implements IDeviceTest {
    private ITestDevice mDevice;

    /**
     * {@inheritDoc}
     */
    @Override
    public void setDevice(ITestDevice device) {
        super.setDevice(device);
        mDevice = device;
    }

    /**
     * Test that metadata encryption is enabled.
     * To enable metadata encryption, see
     * https://source.android.com/security/encryption/metadata
     *
     * @throws Exception
     */
    @CddTest(requirement="9.9.3/C-1-5")
    public void testMetadataEncryptionIsEnabled() throws Exception {
        if (PropertyUtil.getFirstApiLevel(mDevice) <= 29) {
          return; // Requirement does not apply to devices running Q or earlier
        }
        assertTrue("Metadata encryption must be enabled",
            mDevice.getBooleanProperty("ro.crypto.metadata.enabled", false));
    }
}
