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

package com.android.car.settings.wifi;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.net.wifi.WifiManager;

import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;
import com.android.car.ui.preference.CarUiTwoActionSwitchPreference;

/**
 * Controller which determines if the top level entry into Wi-Fi settings should be displayed
 * based on device capabilities.
 */
public class WifiEntryPreferenceController extends
        PreferenceController<CarUiTwoActionSwitchPreference>
        implements CarWifiManager.Listener {

    private CarWifiManager mCarWifiManager;

    public WifiEntryPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    protected Class<CarUiTwoActionSwitchPreference> getPreferenceType() {
        return CarUiTwoActionSwitchPreference.class;
    }

    @Override
    protected void onCreateInternal() {
        mCarWifiManager = new CarWifiManager(getContext());
        getPreference().setOnSecondaryActionClickListener(isChecked -> {
            if (isChecked != mCarWifiManager.isWifiEnabled()) {
                mCarWifiManager.setWifiEnabled(isChecked);
            }
        });
    }

    @Override
    protected void onStartInternal() {
        mCarWifiManager.addListener(this);
        mCarWifiManager.start();
        getPreference().setSecondaryActionChecked(mCarWifiManager.isWifiEnabled());
    }

    @Override
    protected void onStopInternal() {
        mCarWifiManager.removeListener(this);
        mCarWifiManager.stop();
    }

    @Override
    protected void onDestroyInternal() {
        mCarWifiManager.destroy();
    }

    @Override
    protected int getAvailabilityStatus() {
        return WifiUtil.isWifiAvailable(getContext()) ? AVAILABLE : UNSUPPORTED_ON_DEVICE;
    }

    @Override
    public void onWifiStateChanged(int state) {
        getPreference().setSecondaryActionChecked(state == WifiManager.WIFI_STATE_ENABLED
                || state == WifiManager.WIFI_STATE_ENABLING);
    }

    @Override
    public void onAccessPointsChanged() {
        // don't care.
        return;
    }
}
