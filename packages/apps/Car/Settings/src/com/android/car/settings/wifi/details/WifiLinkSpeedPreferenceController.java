/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.car.settings.wifi.details;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;

import com.android.car.settings.R;
import com.android.car.settings.common.FragmentController;

/**
 * Shows info about Wifi link speed info.
 */
public class WifiLinkSpeedPreferenceController extends
        WifiDetailsBasePreferenceController<WifiDetailsPreference> {

    public WifiLinkSpeedPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    protected Class<WifiDetailsPreference> getPreferenceType() {
        return WifiDetailsPreference.class;
    }

    @Override
    protected void updateState(WifiDetailsPreference preference) {
        int linkSpeedMbps = getWifiInfoProvider().getWifiInfo().getLinkSpeed();
        preference.setVisible(linkSpeedMbps >= 0);
        preference.setDetailText(getContext().getString(
                R.string.link_speed, getWifiInfoProvider().getWifiInfo().getLinkSpeed()));
    }
}
