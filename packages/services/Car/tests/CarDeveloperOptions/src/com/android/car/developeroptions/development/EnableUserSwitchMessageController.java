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

package com.android.car.developeroptions.development;

import static android.car.settings.CarSettings.Global.ENABLE_USER_SWITCH_DEVELOPER_MESSAGE;

import android.content.Context;
import android.provider.Settings;

import androidx.preference.Preference;
import androidx.preference.SwitchPreference;

import com.android.car.developeroptions.core.PreferenceControllerMixin;
import com.android.settingslib.development.DeveloperOptionsPreferenceController;

public final class EnableUserSwitchMessageController extends
        DeveloperOptionsPreferenceController implements Preference.OnPreferenceChangeListener,
        PreferenceControllerMixin {

    private static final String ENABLE_USER_SWITCH_MESSAGE_KEY = "enable_user_switch_message";

    private static final String SETTING_VALUE_ON = "true";
    private static final String SETTING_VALUE_OFF = "false";

    public EnableUserSwitchMessageController(Context context) {
        super(context);
    }

    @Override
    public String getPreferenceKey() {
        return ENABLE_USER_SWITCH_MESSAGE_KEY;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        final boolean isEnabled = (Boolean) newValue;
        Settings.Global.putString(mContext.getContentResolver(),
                ENABLE_USER_SWITCH_DEVELOPER_MESSAGE,
                isEnabled ? SETTING_VALUE_ON : SETTING_VALUE_OFF);
        return true;
    }

    @Override
    public void updateState(Preference preference) {
        final String enableUserSwitchDeveloperMessage = Settings.Global.getString(
                mContext.getContentResolver(),
                ENABLE_USER_SWITCH_DEVELOPER_MESSAGE);
        ((SwitchPreference) mPreference)
                .setChecked(SETTING_VALUE_ON.equals(enableUserSwitchDeveloperMessage));
    }

    @Override
    protected void onDeveloperOptionsSwitchDisabled() {
        super.onDeveloperOptionsSwitchDisabled();
        Settings.Global.putString(mContext.getContentResolver(),
                ENABLE_USER_SWITCH_DEVELOPER_MESSAGE, SETTING_VALUE_OFF);
        ((SwitchPreference) mPreference).setChecked(false);
    }
}
