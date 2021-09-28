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

package com.android.car.settings.network;

import android.content.Context;
import android.net.ConnectivityManager;

import androidx.annotation.XmlRes;

import com.android.car.settings.R;
import com.android.car.settings.common.CarSettingActivities;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.settings.search.CarBaseSearchIndexProvider;
import com.android.settingslib.search.SearchIndexable;

/**
 * Shows the list of mobile networks if there are multiple. Clicking into any one of these mobile
 * networks shows {@link MobileNetworkFragment} for that specific mobile network.
 */
@SearchIndexable
public class MobileNetworkListFragment extends SettingsFragment {

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.mobile_network_list_fragment;
    }

    public static final CarBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new CarBaseSearchIndexProvider(R.xml.mobile_network_fragment,
                    CarSettingActivities.MobileNetworkListActivity.class) {
                @Override
                protected boolean isPageSearchEnabled(Context context) {
                    return NetworkUtils.hasMobileNetwork(
                            context.getSystemService(ConnectivityManager.class));
                }
            };
}
