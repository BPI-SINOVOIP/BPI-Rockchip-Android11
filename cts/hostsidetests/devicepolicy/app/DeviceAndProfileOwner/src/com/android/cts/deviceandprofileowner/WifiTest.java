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

import android.content.ComponentName;
import android.content.pm.PackageManager;
import android.text.TextUtils;

/**
 * Tests that require the WiFi feature.
 */
public class WifiTest extends BaseDeviceAdminTest {
    /** Mac address returned when the caller doesn't have access. */
    private static final String DEFAULT_MAC_ADDRESS = "02:00:00:00:00:00";

    public static final ComponentName ADMIN_RECEIVER_COMPONENT = new ComponentName(
            BaseDeviceAdminTest.BasicAdminReceiver.class.getPackage().getName(),
            BaseDeviceAdminTest.BasicAdminReceiver.class.getName());

    public void testGetWifiMacAddress() {
        if (!mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_WIFI)) {
            // wifi not supported.
            return;
        }
        final String macAddress = mDevicePolicyManager.getWifiMacAddress(ADMIN_RECEIVER_COMPONENT);

        assertFalse("Device owner should be able to get the real MAC address",
                DEFAULT_MAC_ADDRESS.equals(macAddress));
        assertFalse("getWifiMacAddress() returned an empty string.  WiFi not enabled?",
                TextUtils.isEmpty(macAddress));
    }

    public void testCannotGetWifiMacAddress() {
        try {
            mDevicePolicyManager.getWifiMacAddress(ADMIN_RECEIVER_COMPONENT);
            fail("Profile owner shouldn't be able to get the MAC address");
        } catch (SecurityException expected) {
        }
    }
}
