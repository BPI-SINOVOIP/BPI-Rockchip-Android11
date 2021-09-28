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

package com.android.cts.deviceandprofileowner;

import static com.google.common.truth.Truth.assertThat;

import android.app.admin.DevicePolicyManager;

public class KeyguardDisabledFeaturesTest extends BaseDeviceAdminTest {

    public void testSetKeyguardDisabledFeatures() {
        mDevicePolicyManager.setKeyguardDisabledFeatures(ADMIN_RECEIVER_COMPONENT,
                DevicePolicyManager.KEYGUARD_DISABLE_SECURE_CAMERA);

        // Check if the admin has disabled the camera specifically for the keyguard
        assertThat(mDevicePolicyManager.getKeyguardDisabledFeatures(
                ADMIN_RECEIVER_COMPONENT)).isEqualTo(
                DevicePolicyManager.KEYGUARD_DISABLE_SECURE_CAMERA);

        removeKeyguardDisableFeatures(mDevicePolicyManager);
        mDevicePolicyManager.setKeyguardDisabledFeatures(ADMIN_RECEIVER_COMPONENT,
                DevicePolicyManager.KEYGUARD_DISABLE_SECURE_NOTIFICATIONS);

        // Check if the admin has disabled notifications specifically for the keyguard
        assertThat(mDevicePolicyManager.getKeyguardDisabledFeatures(
                ADMIN_RECEIVER_COMPONENT)).isEqualTo(
                DevicePolicyManager.KEYGUARD_DISABLE_SECURE_NOTIFICATIONS);
        removeKeyguardDisableFeatures(mDevicePolicyManager);
    }

    public void testSetKeyguardDisabledFeatures_onParentSilentIgnoreWhenCallerIsNotOrgOwnedPO() {
        DevicePolicyManager parentDevicePolicyManager =
                mDevicePolicyManager.getParentProfileInstance(ADMIN_RECEIVER_COMPONENT);

        parentDevicePolicyManager.setKeyguardDisabledFeatures(ADMIN_RECEIVER_COMPONENT,
                DevicePolicyManager.KEYGUARD_DISABLE_SECURE_CAMERA);

        assertThat(mDevicePolicyManager.getKeyguardDisabledFeatures(
                ADMIN_RECEIVER_COMPONENT)).isEqualTo(0);
    }

    public void testSetKeyguardDisabledFeatures_onParent() {
        DevicePolicyManager parentDevicePolicyManager =
                mDevicePolicyManager.getParentProfileInstance(ADMIN_RECEIVER_COMPONENT);

        parentDevicePolicyManager.setKeyguardDisabledFeatures(ADMIN_RECEIVER_COMPONENT,
                DevicePolicyManager.KEYGUARD_DISABLE_SECURE_CAMERA);

        // Check if the admin has disabled the camera specifically for the keyguard
        assertThat(parentDevicePolicyManager.getKeyguardDisabledFeatures(
                ADMIN_RECEIVER_COMPONENT)).isEqualTo(
                DevicePolicyManager.KEYGUARD_DISABLE_SECURE_CAMERA);

        removeKeyguardDisableFeatures(parentDevicePolicyManager);
        parentDevicePolicyManager.setKeyguardDisabledFeatures(ADMIN_RECEIVER_COMPONENT,
                DevicePolicyManager.KEYGUARD_DISABLE_SECURE_NOTIFICATIONS);

        // Check if the admin has disabled notifications specifically for the keyguard
        assertThat(parentDevicePolicyManager.getKeyguardDisabledFeatures(
                ADMIN_RECEIVER_COMPONENT)).isEqualTo(
                DevicePolicyManager.KEYGUARD_DISABLE_SECURE_NOTIFICATIONS);
        removeKeyguardDisableFeatures(parentDevicePolicyManager);
    }

    private void removeKeyguardDisableFeatures(DevicePolicyManager devicePolicyManager) {
        devicePolicyManager.setKeyguardDisabledFeatures(ADMIN_RECEIVER_COMPONENT,
                DevicePolicyManager.KEYGUARD_DISABLE_FEATURES_NONE);
    }
}
