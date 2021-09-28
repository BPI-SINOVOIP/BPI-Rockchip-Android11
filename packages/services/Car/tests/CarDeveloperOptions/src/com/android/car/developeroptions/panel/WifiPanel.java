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

package com.android.car.developeroptions.panel;

import android.app.settings.SettingsEnums;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.SubSettings;
import com.android.car.developeroptions.slices.CustomSliceRegistry;
import com.android.car.developeroptions.slices.SliceBuilderUtils;
import com.android.car.developeroptions.wifi.WifiSettings;

import java.util.ArrayList;
import java.util.List;

/**
 * Panel data class for Wifi settings.
 */
public class WifiPanel implements PanelContent {

    private final Context mContext;

    public static WifiPanel create(Context context) {
        return new WifiPanel(context);
    }

    private WifiPanel(Context context) {
        mContext = context.getApplicationContext();
    }

    @Override
    public CharSequence getTitle() {
        return mContext.getText(R.string.wifi_settings);
    }

    @Override
    public List<Uri> getSlices() {
        final List<Uri> uris = new ArrayList<>();
        uris.add(CustomSliceRegistry.WIFI_SLICE_URI);
        return uris;
    }

    @Override
    public Intent getSeeMoreIntent() {
        final String screenTitle =
                mContext.getText(R.string.wifi_settings).toString();
        final Intent intent = SliceBuilderUtils.buildSearchResultPageIntent(mContext,
                WifiSettings.class.getName(),
                null /* key */,
                screenTitle,
                SettingsEnums.WIFI);
        intent.setClassName(mContext.getPackageName(), SubSettings.class.getName());
        return intent;
    }

    @Override
    public int getMetricsCategory() {
        return SettingsEnums.PANEL_WIFI;
    }
}
