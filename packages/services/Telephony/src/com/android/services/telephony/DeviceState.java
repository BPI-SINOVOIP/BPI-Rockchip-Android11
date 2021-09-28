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

package com.android.services.telephony;

import android.content.Context;
import android.provider.Settings;
import android.telecom.TelecomManager;

import com.android.internal.telephony.PhoneConstants;
import com.android.phone.R;

/**
 * Abstracts device state and settings for better testability.
 */
public class DeviceState {

    public boolean shouldCheckSimStateBeforeOutgoingCall(Context context) {
        return context.getResources().getBoolean(
                R.bool.config_checkSimStateBeforeOutgoingCall);
    }

    public boolean isSuplDdsSwitchRequiredForEmergencyCall(Context context) {
        return context.getResources().getBoolean(
                R.bool.config_gnss_supl_requires_default_data_for_emergency);
    }

    public boolean isRadioPowerDownAllowedOnBluetooth(Context context) {
        return context.getResources().getBoolean(R.bool.config_allowRadioPowerDownOnBluetooth);
    }

    public boolean isAirplaneModeOn(Context context) {
        return Settings.Global.getInt(context.getContentResolver(),
                Settings.Global.AIRPLANE_MODE_ON, 0) > 0;
    }

    public int getCellOnStatus(Context context) {
        return Settings.Global.getInt(context.getContentResolver(), Settings.Global.CELL_ON,
                PhoneConstants.CELL_ON_FLAG);
    }

    public boolean isTtyModeEnabled(Context context) {
        return android.provider.Settings.Secure.getInt(context.getContentResolver(),
                android.provider.Settings.Secure.PREFERRED_TTY_MODE,
                TelecomManager.TTY_MODE_OFF) != TelecomManager.TTY_MODE_OFF;
    }
}
