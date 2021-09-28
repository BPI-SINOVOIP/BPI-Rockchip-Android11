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

package com.android.car.developeroptions.wifi;

import android.content.Context;
import android.provider.Settings;

import com.android.car.developeroptions.core.TogglePreferenceController;

/**
 * CellularFallbackPreferenceController controls whether we should fall back to celluar when
 * wifi is bad.
 */
public class CellularFallbackPreferenceController extends TogglePreferenceController {

    public CellularFallbackPreferenceController(Context context, String key) {
        super(context, key);
    }

    @Override
    public int getAvailabilityStatus() {
        return !avoidBadWifiConfig() ? AVAILABLE : UNSUPPORTED_ON_DEVICE;
    }

    @Override
    public boolean isChecked() {
        return avoidBadWifiCurrentSettings();
    }

    @Override
    public boolean setChecked(boolean isChecked) {
        // On: avoid bad wifi. Off: prompt.
        return Settings.Global.putString(mContext.getContentResolver(),
                Settings.Global.NETWORK_AVOID_BAD_WIFI, isChecked ? "1" : null);
    }

    private boolean avoidBadWifiConfig() {
        return mContext.getResources().getInteger(
                com.android.internal.R.integer.config_networkAvoidBadWifi) == 1;
    }

    private boolean avoidBadWifiCurrentSettings() {
        return "1".equals(Settings.Global.getString(mContext.getContentResolver(),
                Settings.Global.NETWORK_AVOID_BAD_WIFI));
    }
}