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

package com.android.car.dialer.ui.settings;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.ViewModelProviders;
import androidx.preference.Preference;

import com.android.car.dialer.R;
import com.android.car.ui.preference.PreferenceFragment;

/**
 * A fragment that displays the settings page
 */
public class DialerSettingsFragment extends PreferenceFragment {

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        LiveData<String> connectedDeviceName = ViewModelProviders.of(this)
                .get(DialerSettingsViewModel.class)
                .getFirstHfpConnectedDeviceName();
        connectedDeviceName.observe(this, (name) -> {
            Preference preference = findPreference(getString((R.string.pref_connected_phone_key)));
            if (preference != null) {
                preference.setSummary(name);
            }
        });
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.settings_page, rootKey);
    }
}
