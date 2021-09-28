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
package com.android.compatibility.common.util;

import com.android.tradefed.device.ITestDevice;

public class LocationModeSetter implements AutoCloseable {

    /**
     * Copied from {@link android.provider.Settings}
     */
    private static final String SETTINGS_SECURE = "secure";
    private static final String LOCATION_MODE = "location_mode";
    private static final String LOCATION_MODE_HIGH_ACCURACY = "3";
    private static final String LOCATION_MODE_OFF = "0";

    private final String mOldLocationSetting;
    private final ITestDevice mDevice;

    public LocationModeSetter(ITestDevice device) throws Exception {
        mDevice = device;
        mOldLocationSetting = mDevice.getSetting(SETTINGS_SECURE, LOCATION_MODE);
    }

    public void setLocationEnabled(boolean enabled) throws Exception {
        if (enabled) {
            mDevice.setSetting(SETTINGS_SECURE, LOCATION_MODE, LOCATION_MODE_HIGH_ACCURACY);
        } else {
            mDevice.setSetting(SETTINGS_SECURE, LOCATION_MODE, LOCATION_MODE_OFF);
        }
    }

    @Override
    public void close() throws Exception {
        mDevice.setSetting(SETTINGS_SECURE, LOCATION_MODE, mOldLocationSetting);
    }
}