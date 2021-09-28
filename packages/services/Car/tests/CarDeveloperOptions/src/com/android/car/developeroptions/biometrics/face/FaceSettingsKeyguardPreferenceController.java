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
 * limitations under the License
 */

package com.android.car.developeroptions.biometrics.face;

import static android.provider.Settings.Secure.FACE_UNLOCK_KEYGUARD_ENABLED;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.os.UserHandle;
import android.provider.Settings;

import androidx.preference.Preference;

import com.android.car.developeroptions.core.TogglePreferenceController;

/**
 * Preference controller for Face settings page controlling the ability to unlock the phone
 * with face.
 */
public class FaceSettingsKeyguardPreferenceController extends FaceSettingsPreferenceController {

    static final String KEY = "security_settings_face_keyguard";

    private static final int ON = 1;
    private static final int OFF = 0;
    private static final int DEFAULT = ON;  // face unlock is enabled on keyguard by default

    public FaceSettingsKeyguardPreferenceController(Context context, String preferenceKey) {
        super(context, preferenceKey);
    }

    public FaceSettingsKeyguardPreferenceController(Context context) {
        this(context, KEY);
    }

    @Override
    public boolean isChecked() {
        if (!FaceSettings.isAvailable(mContext)) {
            return false;
        } else if (adminDisabled()) {
            return false;
        }
        return Settings.Secure.getIntForUser(mContext.getContentResolver(),
                FACE_UNLOCK_KEYGUARD_ENABLED, DEFAULT, getUserId()) == ON;
    }

    @Override
    public boolean setChecked(boolean isChecked) {
        return Settings.Secure.putIntForUser(mContext.getContentResolver(),
                FACE_UNLOCK_KEYGUARD_ENABLED, isChecked ? ON : OFF, getUserId());
    }

    @Override
    public int getAvailabilityStatus() {
        return AVAILABLE;
    }

    @Override
    public void updateState(Preference preference) {
        super.updateState(preference);
        if (!FaceSettings.isAvailable(mContext)) {
            preference.setEnabled(false);
        } else if (adminDisabled()) {
            preference.setEnabled(false);
        } else {
            preference.setEnabled(true);
        }
    }

    private boolean adminDisabled() {
        DevicePolicyManager dpm =
                (DevicePolicyManager) mContext.getSystemService(Context.DEVICE_POLICY_SERVICE);
        return dpm != null &&
                (dpm.getKeyguardDisabledFeatures(null, UserHandle.myUserId())
                        & DevicePolicyManager.KEYGUARD_DISABLE_FACE)
                        != 0;
    }
}
