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

package com.android.car.settings.privacy;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.os.Bundle;

import androidx.preference.Preference;

import com.android.car.settings.common.ExtraSettingsPreferenceController;
import com.android.car.settings.common.FragmentController;

import java.util.Map;

/**
 * Injects preferences from other system applications at a placeholder location into the
 * {@link PrivacySettingsFragment}.
 */
public class PrivacyExtraPreferenceController extends ExtraSettingsPreferenceController {

    public PrivacyExtraPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions restrictionInfo) {
        super(context, preferenceKey, fragmentController, restrictionInfo);
    }

    @Override
    protected void addExtraSettings(Map<Preference, Bundle> preferenceBundleMap) {
        for (Preference setting : preferenceBundleMap.keySet()) {
            // IA framework allows for icons to be defined, but we don't want any icons on this page
            setting.setIcon(null);
            getPreference().addPreference(setting);
        }
    }
}
