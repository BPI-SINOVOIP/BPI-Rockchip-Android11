/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
package com.android.car.developeroptions.display;

import static android.provider.Settings.System.ADAPTIVE_SLEEP;

import android.content.Context;
import android.provider.Settings;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.core.TogglePreferenceController;


public class AdaptiveSleepPreferenceController extends TogglePreferenceController {

    private final String SYSTEM_KEY = ADAPTIVE_SLEEP;
    private final int DEFAULT_VALUE = 0;

    public AdaptiveSleepPreferenceController(Context context, String key) {
        super(context, key);
    }

    @Override
    public boolean isChecked() {
        return Settings.System.getInt(mContext.getContentResolver(),
                SYSTEM_KEY, DEFAULT_VALUE) != DEFAULT_VALUE;
    }

    @Override
    public boolean setChecked(boolean isChecked) {
        Settings.System.putInt(mContext.getContentResolver(), SYSTEM_KEY,
                isChecked ? 1 : DEFAULT_VALUE);
        return true;
    }

    @Override
    @AvailabilityStatus
    public int getAvailabilityStatus() {
        return mContext.getResources().getBoolean(
                com.android.internal.R.bool.config_adaptive_sleep_available)
                ? AVAILABLE
                : UNSUPPORTED_ON_DEVICE;
    }

    @Override
    public boolean isSliceable() {
        return true;
    }

    @Override
    public CharSequence getSummary() {
        return mContext.getText(isChecked()
                ? R.string.adaptive_sleep_summary_on
                : R.string.adaptive_sleep_summary_off);
    }
}
