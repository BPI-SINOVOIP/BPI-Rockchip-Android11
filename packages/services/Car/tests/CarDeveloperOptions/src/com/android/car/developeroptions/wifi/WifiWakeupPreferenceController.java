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

package com.android.car.developeroptions.wifi;

import static com.android.car.developeroptions.wifi.ConfigureWifiSettings.WIFI_WAKEUP_REQUEST_CODE;

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.location.LocationManager;
import android.net.wifi.WifiManager;
import android.provider.Settings;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.preference.SwitchPreference;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.core.PreferenceControllerMixin;
import com.android.car.developeroptions.dashboard.DashboardFragment;
import com.android.car.developeroptions.utils.AnnotationSpan;
import com.android.settingslib.core.AbstractPreferenceController;
import com.android.settingslib.core.lifecycle.Lifecycle;
import com.android.settingslib.core.lifecycle.LifecycleObserver;
import com.android.settingslib.core.lifecycle.events.OnPause;
import com.android.settingslib.core.lifecycle.events.OnResume;

/**
 * {@link PreferenceControllerMixin} that controls whether the Wi-Fi Wakeup feature should be
 * enabled.
 */
public class WifiWakeupPreferenceController extends AbstractPreferenceController implements
        LifecycleObserver, OnPause, OnResume {

    private static final String TAG = "WifiWakeupPrefController";
    private static final String KEY_ENABLE_WIFI_WAKEUP = "enable_wifi_wakeup";

    private final Fragment mFragment;

    @VisibleForTesting
    SwitchPreference mPreference;
    @VisibleForTesting
    LocationManager mLocationManager;

    @VisibleForTesting
    WifiManager mWifiManager;
    private final BroadcastReceiver mLocationReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            updateState(mPreference);
        }
    };
    private final IntentFilter mLocationFilter =
            new IntentFilter(LocationManager.MODE_CHANGED_ACTION);

    public WifiWakeupPreferenceController(Context context, DashboardFragment fragment,
            Lifecycle lifecycle) {
        super(context);
        mFragment = fragment;
        mLocationManager = (LocationManager) context.getSystemService(Service.LOCATION_SERVICE);
        mWifiManager = context.getSystemService(WifiManager.class);
        lifecycle.addObserver(this);
    }

    @Override
    public void displayPreference(PreferenceScreen screen) {
        super.displayPreference(screen);
        mPreference = screen.findPreference(KEY_ENABLE_WIFI_WAKEUP);
        updateState(mPreference);
    }

    @Override
    public boolean isAvailable() {
        return true;
    }

    @Override
    public boolean handlePreferenceTreeClick(Preference preference) {
        if (!TextUtils.equals(preference.getKey(), KEY_ENABLE_WIFI_WAKEUP)) {
            return false;
        }
        if (!(preference instanceof SwitchPreference)) {
            return false;
        }

        if (!mLocationManager.isLocationEnabled()) {
            final Intent intent = new Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS);
            mFragment.startActivity(intent);
        } else if (getWifiWakeupEnabled()) {
            setWifiWakeupEnabled(false);
        } else if (!getWifiScanningEnabled()) {
            showScanningDialog();
        } else {
            setWifiWakeupEnabled(true);
        }

        updateState(mPreference);
        return true;
    }

    @Override
    public String getPreferenceKey() {
        return KEY_ENABLE_WIFI_WAKEUP;
    }

    @Override
    public void updateState(Preference preference) {
        if (!(preference instanceof SwitchPreference)) {
            return;
        }
        final SwitchPreference enableWifiWakeup = (SwitchPreference) preference;

        enableWifiWakeup.setChecked(getWifiWakeupEnabled()
                && getWifiScanningEnabled()
                && mLocationManager.isLocationEnabled());
        if (!mLocationManager.isLocationEnabled()) {
            preference.setSummary(getNoLocationSummary());
        } else {
            preference.setSummary(R.string.wifi_wakeup_summary);
        }
    }

    @VisibleForTesting
    CharSequence getNoLocationSummary() {
        AnnotationSpan.LinkInfo linkInfo = new AnnotationSpan.LinkInfo("link", null);
        CharSequence locationText = mContext.getText(R.string.wifi_wakeup_summary_no_location);
        return AnnotationSpan.linkify(locationText, linkInfo);
    }

    public void onActivityResult(int requestCode, int resultCode) {
        if (requestCode != WIFI_WAKEUP_REQUEST_CODE) {
            return;
        }
        if (mLocationManager.isLocationEnabled()) {
            setWifiWakeupEnabled(true);
        }
        updateState(mPreference);
    }

    private boolean getWifiScanningEnabled() {
        return mWifiManager.isScanAlwaysAvailable();
    }

    private void showScanningDialog() {
        final WifiScanningRequiredFragment dialogFragment =
                WifiScanningRequiredFragment.newInstance();
        dialogFragment.setTargetFragment(mFragment, WIFI_WAKEUP_REQUEST_CODE /* requestCode */);
        dialogFragment.show(mFragment.getFragmentManager(), TAG);
    }

    private boolean getWifiWakeupEnabled() {
        return mWifiManager.isAutoWakeupEnabled();
    }

    private void setWifiWakeupEnabled(boolean enabled) {
        mWifiManager.setAutoWakeupEnabled(enabled);
    }

    @Override
    public void onResume() {
        mContext.registerReceiver(mLocationReceiver, mLocationFilter);
    }

    @Override
    public void onPause() {
        mContext.unregisterReceiver(mLocationReceiver);
    }
}
