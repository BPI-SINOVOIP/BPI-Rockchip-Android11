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
package com.android.cts.deviceandprofileowner;

import static com.google.common.truth.Truth.assertThat;

import android.app.admin.DevicePolicyManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.test.InstrumentationTestCase;

/**
 * Helper to set app restriction managing package for
 * {@link DevicePolicyManager#isCallerApplicationRestrictionsManagingPackage()}.
 * <p>The method name starts with "test" to be recognized by {@link InstrumentationTestCase}.
 */
public class ApplicationRestrictionsIsCallerDelegateHelper extends BaseDeviceAdminTest {

    private static final String APP_RESTRICTIONS_TARGET_PKG = "com.android.cts.delegate";

    public void testSetApplicationRestrictionsManagingPackageToDelegate()
            throws NameNotFoundException {
        mDevicePolicyManager.setApplicationRestrictionsManagingPackage(
            ADMIN_RECEIVER_COMPONENT, APP_RESTRICTIONS_TARGET_PKG);
        assertThat(APP_RESTRICTIONS_TARGET_PKG)
            .isEqualTo(mDevicePolicyManager.getApplicationRestrictionsManagingPackage(
                ADMIN_RECEIVER_COMPONENT));
    }
}
