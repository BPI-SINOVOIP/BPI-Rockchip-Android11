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

package com.android.cts.deviceandprofileowner;


import android.app.admin.DevicePolicyManager;
import android.content.pm.PackageManager;

/**
 * Tests some password APIs in {@link DevicePolicyManager} on devices which don't support the
 * {@link PackageManager#FEATURE_SECURE_LOCK_SCREEN} feature.
 */
public class UnavailableSecureLockScreenTest extends BaseDeviceAdminTest {

    private static final byte[] TOKEN = "abcdefghijklmnopqrstuvwxyz0123456789".getBytes();
    private static final String COMPLEX_PASSWORD = "XYZabc123#&*!.";

    public void testResetWithTokenAndPasswordSufficiency() throws Exception {
        // The password is always empty on devices which don't support the secure lock screen
        // feature.

        // Initially, when no password quality requirements are set, the empty password is
        // sufficient.
        assertPasswordSufficiency(true);
        try {

            // Resetting password should fail - it is not possible to set a password reset token or
            // a password on a device without the secure lock screen feature.
            assertFalse(mDevicePolicyManager.setResetPasswordToken(ADMIN_RECEIVER_COMPONENT, TOKEN));
            assertFalse(mDevicePolicyManager.resetPasswordWithToken(ADMIN_RECEIVER_COMPONENT,
                    COMPLEX_PASSWORD, TOKEN, 0));

            // As soon as some requirement for minimal password quality is set...
            mDevicePolicyManager.setPasswordQuality(
                    ADMIN_RECEIVER_COMPONENT, DevicePolicyManager.PASSWORD_QUALITY_SOMETHING);

            // ... the password is not sufficient any more.
            assertPasswordSufficiency(false);

            // If the requirement for minimal password quality is removed later...
            mDevicePolicyManager.setPasswordQuality(
                    ADMIN_RECEIVER_COMPONENT, DevicePolicyManager.PASSWORD_QUALITY_UNSPECIFIED);

            // The empty password is sufficient again.
            assertPasswordSufficiency(true);
        } finally {
            mDevicePolicyManager.clearResetPasswordToken(ADMIN_RECEIVER_COMPONENT);
            mDevicePolicyManager.setPasswordQuality(
                    ADMIN_RECEIVER_COMPONENT, DevicePolicyManager.PASSWORD_QUALITY_UNSPECIFIED);
        }
    }
}
