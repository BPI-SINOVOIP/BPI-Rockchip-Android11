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
package com.android.cts.deviceandprofileowner;

import android.os.Build;


/**
 * Test cases for {@link android.app.admin.DevicePolicyManager#resetPassword(String, int)}.
 *
 * As of R, resetPassword is fully deprecated: DPCs targeting Sdk level O or above will continue
 * to receive a SecurityException when calling this, while DPC targeting N or below will just get
 * a silent failure of API returning {@code false}. This class tests these two negative cases.
 *
 */
public class ResetPasswordTest extends BaseDeviceAdminTest {

    public void testResetPasswordDeprecated() {
        waitUntilUserUnlocked();

        if (getTargetSdkLevel() >= Build.VERSION_CODES.O) {
            try {
                mDevicePolicyManager.resetPassword("1234", 0);
                fail("resetPassword() should throw SecurityException");
            } catch (SecurityException e) { }

        } else {
            assertFalse(mDevicePolicyManager.resetPassword("1234", 0));
        }
    }

    private int getTargetSdkLevel() {
        return mContext.getApplicationContext().getApplicationInfo().targetSdkVersion;
    }
}
