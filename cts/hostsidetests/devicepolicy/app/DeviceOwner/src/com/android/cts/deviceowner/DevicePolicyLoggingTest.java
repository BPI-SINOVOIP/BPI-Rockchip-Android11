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

package com.android.cts.deviceowner;

import static android.provider.Settings.Global.AUTO_TIME;
import static android.provider.Settings.Global.AUTO_TIME_ZONE;
import static android.provider.Settings.Global.DATA_ROAMING;
import static android.provider.Settings.Global.USB_MASS_STORAGE_ENABLED;

import android.provider.Settings;

/**
 * Invocations of {@link android.app.admin.DevicePolicyManager} methods which are either not CTS
 * tested or the CTS tests call too many other methods. Used to verify that the relevant metrics
 * are logged. Note that the metrics verification is done on the host-side.
 */
public class DevicePolicyLoggingTest extends BaseDeviceOwnerTest {
    public void testSetKeyguardDisabledLogged() {
        mDevicePolicyManager.setKeyguardDisabled(getWho(), true);
        mDevicePolicyManager.setKeyguardDisabled(getWho(), false);
    }

    public void testSetStatusBarDisabledLogged() {
        mDevicePolicyManager.setStatusBarDisabled(getWho(), true);
        mDevicePolicyManager.setStatusBarDisabled(getWho(), false);
    }

    public void testSetGlobalSettingLogged() {
        final String autoTimeOriginalValue =
                Settings.Global.getString(mContext.getContentResolver(), AUTO_TIME);
        final String autoTimeZoneOriginalValue =
                Settings.Global.getString(mContext.getContentResolver(), AUTO_TIME_ZONE);
        final String dataRoamingOriginalValue =
                Settings.Global.getString(mContext.getContentResolver(), DATA_ROAMING);
        final String usbMassStorageOriginalValue =
                Settings.Global.getString(mContext.getContentResolver(), USB_MASS_STORAGE_ENABLED);

        try {
            mDevicePolicyManager.setGlobalSetting(getWho(), AUTO_TIME, "1");
            mDevicePolicyManager.setGlobalSetting(getWho(), AUTO_TIME_ZONE, "1");
            mDevicePolicyManager.setGlobalSetting(getWho(), DATA_ROAMING, "1");
            mDevicePolicyManager.setGlobalSetting(getWho(), USB_MASS_STORAGE_ENABLED, "1");
        } finally {
            mDevicePolicyManager.setGlobalSetting(getWho(), AUTO_TIME, autoTimeOriginalValue);
            mDevicePolicyManager.setGlobalSetting(
                    getWho(), AUTO_TIME_ZONE, autoTimeZoneOriginalValue);
            mDevicePolicyManager.setGlobalSetting(getWho(), DATA_ROAMING, dataRoamingOriginalValue);
            mDevicePolicyManager.setGlobalSetting(
                    getWho(), USB_MASS_STORAGE_ENABLED, usbMassStorageOriginalValue);
        }
    }
}
