/*
 * Copyright (C) 2021 The Android Open Source Project
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

package com.android.car.settings.location;

import android.content.Context;

import androidx.preference.Preference;

import com.android.car.ui.preference.CarUiPreference;
import com.android.settingslib.location.InjectedSetting;
import com.android.settingslib.location.SettingsInjector;

/**
 * Car-specific class to use CarUiPreferences for preferences loaded from {@link SettingsInjector}.
 */
public class CarLocationSettingsInjector extends SettingsInjector {
    public CarLocationSettingsInjector(Context context) {
        super(context);
    }

    @Override
    protected Preference createPreference(Context prefContext, InjectedSetting setting) {
        return new CarUiPreference(prefContext);
    }
}
