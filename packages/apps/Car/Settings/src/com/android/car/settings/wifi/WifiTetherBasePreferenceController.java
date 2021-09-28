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

package com.android.car.settings.wifi;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.Intent;
import android.net.wifi.SoftApConfiguration;
import android.text.TextUtils;

import androidx.annotation.CallSuper;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;
import androidx.preference.Preference;

import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;

/**
 * Shared business logic for preference controllers related to Wifi Tethering
 *
 * @param <V> the upper bound on the type of {@link Preference} on which the controller
 *            expects to operate.
 */
public abstract class WifiTetherBasePreferenceController<V extends Preference> extends
        PreferenceController<V> {

    /**
     * Action used in the {@link Intent} sent by the {@link LocalBroadcastManager} to request wifi
     * restart.
     */
    public static final String ACTION_RESTART_WIFI_TETHERING =
            "com.android.car.settings.wifi.ACTION_RESTART_WIFI_TETHERING";

    private CarWifiManager mCarWifiManager;

    public WifiTetherBasePreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    @CallSuper
    protected void onCreateInternal() {
        mCarWifiManager = new CarWifiManager(getContext());
    }

    @Override
    @CallSuper
    protected void onStartInternal() {
        mCarWifiManager.start();
        getPreference().setPersistent(true);
    }

    @Override
    @CallSuper
    protected void onStopInternal() {
        mCarWifiManager.stop();
    }

    @Override
    @CallSuper
    protected void onDestroyInternal() {
        mCarWifiManager.destroy();
    }

    @Override
    @CallSuper
    protected void updateState(V preference) {
        String summary = getSummary();
        String defaultSummary = getDefaultSummary();

        if (TextUtils.isEmpty(summary)) {
            preference.setSummary(defaultSummary);
        } else {
            preference.setSummary(summary);
        }
    }

    protected SoftApConfiguration getCarSoftApConfig() {
        return mCarWifiManager.getSoftApConfig();
    }

    protected void setCarSoftApConfig(SoftApConfiguration configuration) {
        mCarWifiManager.setSoftApConfig(configuration);
        requestWifiTetherRestart();
    }

    protected CarWifiManager getCarWifiManager() {
        return mCarWifiManager;
    }

    protected abstract String getSummary();

    protected abstract String getDefaultSummary();

    protected void requestWifiTetherRestart() {
        Intent intent = new Intent(ACTION_RESTART_WIFI_TETHERING);
        LocalBroadcastManager.getInstance(getContext()).sendBroadcast(intent);
    }
}
