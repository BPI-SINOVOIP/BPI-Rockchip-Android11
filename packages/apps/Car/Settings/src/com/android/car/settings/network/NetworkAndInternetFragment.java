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

package com.android.car.settings.network;

import android.content.Context;
import android.net.ConnectivityManager;
import android.provider.Settings;

import androidx.annotation.XmlRes;

import com.android.car.settings.R;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.settings.search.CarBaseSearchIndexProvider;
import com.android.settingslib.search.SearchIndexable;

import java.util.List;

/** Fragment for all wifi/mobile data connectivity preferences. */
@SearchIndexable
public class NetworkAndInternetFragment extends SettingsFragment {

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.network_and_internet_fragment;
    }

    public static final CarBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new CarBaseSearchIndexProvider(R.xml.network_and_internet_fragment,
                    Settings.Panel.ACTION_INTERNET_CONNECTIVITY) {
                @Override
                public List<String> getNonIndexableKeys(Context context) {
                    List<String> nonIndexableKeys = super.getNonIndexableKeys(context);
                    if (!NetworkUtils.hasMobileNetwork(
                            context.getSystemService(ConnectivityManager.class))) {
                        nonIndexableKeys.add(
                                context.getString(R.string.pk_mobile_network_settings_entry));
                        nonIndexableKeys.add(
                                context.getString(R.string.pk_data_usage_settings_entry));
                    }
                    return nonIndexableKeys;
                }
            };
}
