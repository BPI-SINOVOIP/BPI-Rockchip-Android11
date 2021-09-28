/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.android.cts.deviceadmin;

import android.os.Build;

/**
 * Tests that:
 * - need to be run as device admin (as opposed to device owner) and
 * - require resetting the password at the end.
 *
 * Note: when adding a new method, make sure to add a corresponding method in
 * BaseDeviceAdminHostSideTest.
 */
public class DeviceAdminPasswordTest extends BaseDeviceAdminTest {

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        assertNotDeviceOwner();
    }

    public void testResetPasswordDeprecated() {
        if (getTargetSdkLevel() < Build.VERSION_CODES.N) {
            assertFalse(dpm.resetPassword("1234", 0));
        } else {
            try {
                dpm.resetPassword("1234", 0);
                fail("resetPassword() should throw SecurityException");
            } catch (SecurityException e) { }
        }
    }

    private int getTargetSdkLevel() {
        return mContext.getApplicationContext().getApplicationInfo().targetSdkVersion;
    }
}
