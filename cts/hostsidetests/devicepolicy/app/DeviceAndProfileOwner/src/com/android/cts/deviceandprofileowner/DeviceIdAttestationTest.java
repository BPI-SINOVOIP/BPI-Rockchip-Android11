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

import static android.app.admin.DevicePolicyManager.ID_TYPE_SERIAL;

import static com.google.common.truth.Truth.assertThat;

import android.content.ComponentName;
import android.keystore.cts.KeyGenerationUtils;
import android.security.AttestedKeyPair;

public class DeviceIdAttestationTest extends BaseDeviceAdminTest {
    // Test that key generation when requesting the serial number to be included in the
    // attestation record fails if the profile owner has not been explicitly granted access
    // to it.
    public void testFailsWithoutProfileOwnerIdsGrant() {
        KeyGenerationUtils.generateKeyWithDeviceIdAttestationExpectingFailure(mDevicePolicyManager,
                getWho());
    }

    // Test that the same key generation request succeeds once the profile owner was granted
    // access to device identifiers.
    public void testSucceedsWithProfileOwnerIdsGrant() {
        if (mDevicePolicyManager.isDeviceIdAttestationSupported()) {
            KeyGenerationUtils.generateKeyWithDeviceIdAttestationExpectingSuccess(
                    mDevicePolicyManager, getWho());
        }
    }

    protected ComponentName getWho() {
        return ADMIN_RECEIVER_COMPONENT;
    }
}
