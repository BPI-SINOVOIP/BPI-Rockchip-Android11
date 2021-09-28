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
package com.android.car.developeroptions.connecteddevice;

import android.app.settings.SettingsEnums;
import android.content.Context;
import android.net.Uri;
import android.provider.SearchIndexableResource;

import androidx.annotation.VisibleForTesting;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.dashboard.DashboardFragment;
import com.android.car.developeroptions.search.BaseSearchIndexProvider;
import com.android.car.developeroptions.slices.SlicePreferenceController;
import com.android.settingslib.search.SearchIndexable;

import java.util.Arrays;
import java.util.List;

@SearchIndexable(forTarget = SearchIndexable.ALL & ~SearchIndexable.ARC)
public class ConnectedDeviceDashboardFragment extends DashboardFragment {

    private static final String TAG = "ConnectedDeviceFrag";

    @VisibleForTesting
    static final String KEY_CONNECTED_DEVICES = "connected_device_list";
    @VisibleForTesting
    static final String KEY_AVAILABLE_DEVICES = "available_device_list";

    @Override
    public int getMetricsCategory() {
        return SettingsEnums.SETTINGS_CONNECTED_DEVICE_CATEGORY;
    }

    @Override
    protected String getLogTag() {
        return TAG;
    }

    @Override
    public int getHelpResource() {
        return R.string.help_url_connected_devices;
    }

    @Override
    protected int getPreferenceScreenResId() {
        return R.xml.connected_devices;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        use(AvailableMediaDeviceGroupController.class).init(this);
        use(ConnectedDeviceGroupController.class).init(this);
        use(PreviouslyConnectedDevicePreferenceController.class).init(this);
        use(SlicePreferenceController.class).setSliceUri(
                Uri.parse(getString(R.string.config_nearby_devices_slice_uri)));
    }

    /**
     * For Search.
     */
    public static final SearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new BaseSearchIndexProvider() {
                @Override
                public List<SearchIndexableResource> getXmlResourcesToIndex(
                        Context context, boolean enabled) {
                    final SearchIndexableResource sir = new SearchIndexableResource(context);
                    sir.xmlResId = R.xml.connected_devices;
                    return Arrays.asList(sir);
                }
            };
}
