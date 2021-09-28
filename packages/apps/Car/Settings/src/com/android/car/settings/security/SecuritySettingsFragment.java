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

package com.android.car.settings.security;

import android.provider.Settings;

import com.android.car.settings.R;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.settings.search.CarBaseSearchIndexProvider;
import com.android.settingslib.search.SearchIndexable;

/**
 * Security home page which includes "choose lock type" and "clear credentials".
 */
@SearchIndexable
public class SecuritySettingsFragment extends SettingsFragment {

    @Override
    protected int getPreferenceScreenResId() {
        return R.xml.security_settings_fragment;
    }

    public static final CarBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new CarBaseSearchIndexProvider(R.xml.security_settings_fragment,
                    Settings.ACTION_SECURITY_SETTINGS);
}
