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
package com.android.cts.devicepolicy;

import static com.android.cts.devicepolicy.DeviceAndProfileOwnerTest.ADMIN_RECEIVER_TEST_CLASS;
import static com.android.cts.devicepolicy.DeviceAndProfileOwnerTest.DEVICE_ADMIN_APK;
import static com.android.cts.devicepolicy.DeviceAndProfileOwnerTest.DEVICE_ADMIN_PKG;
import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.assertMetricsLogged;
import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.isStatsdEnabled;

import com.android.cts.devicepolicy.metrics.DevicePolicyEventWrapper;
import com.android.tradefed.device.DeviceNotAvailableException;
import java.io.FileNotFoundException;

import android.stats.devicepolicy.EventId;

import org.junit.Test;

public class AdbProvisioningTests extends BaseDevicePolicyTest {

    @Override
    public void setUp() throws Exception {
        if (!mHasFeature) {
            return;
        }
        super.setUp();
        installAppAsUser(DEVICE_ADMIN_APK, mPrimaryUserId);
    }

    @Override
    public void tearDown() throws Exception {
        if (!mHasFeature) {
            return;
        }
        super.tearDown();
        getDevice().uninstallPackage(DEVICE_ADMIN_PKG);
    }

    @Test
    public void testAdbDeviceOwnerLogged() throws Exception {
        if (!mHasFeature || !isStatsdEnabled(getDevice())) {
            return;
        }
        assertMetricsLogged(getDevice(), () -> {
            setDeviceOwner(DEVICE_ADMIN_PKG + "/" + ADMIN_RECEIVER_TEST_CLASS, mPrimaryUserId,
                    /* expectFailure */ false);
        }, new DevicePolicyEventWrapper.Builder(EventId.PROVISIONING_ENTRY_POINT_ADB_VALUE)
                    .setAdminPackageName(DEVICE_ADMIN_PKG)
                    .setBoolean(false)
                    .setStrings("device-owner")
                    .build());
    }

    @Test
    public void testAdbProfileOwnerLogged() throws Exception {
        if (!mHasFeature || !isStatsdEnabled(getDevice())) {
            return;
        }
        assertMetricsLogged(getDevice(), () -> {
            setProfileOwner(DEVICE_ADMIN_PKG + "/" + ADMIN_RECEIVER_TEST_CLASS, mPrimaryUserId,
                    /* expectFailure */ false);
        }, new DevicePolicyEventWrapper.Builder(EventId.PROVISIONING_ENTRY_POINT_ADB_VALUE)
                .setAdminPackageName(DEVICE_ADMIN_PKG)
                .setBoolean(false)
                .setStrings("profile-owner")
                .build());
    }
}
