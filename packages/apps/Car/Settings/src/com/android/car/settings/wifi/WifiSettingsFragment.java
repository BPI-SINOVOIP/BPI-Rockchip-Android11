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
package com.android.car.settings.wifi;

import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.provider.Settings;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.XmlRes;

import com.android.car.settings.R;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.settings.search.CarBaseSearchIndexProvider;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.ProgressBarController;
import com.android.settingslib.search.SearchIndexable;
import com.android.settingslib.wifi.AccessPoint;

import java.util.Collections;
import java.util.List;

/**
 * Main page to host Wifi related preferences.
 */
@SearchIndexable
public class WifiSettingsFragment extends SettingsFragment
        implements CarWifiManager.Listener {

    private static final int SEARCHING_DELAY_MILLIS = 1700;
    private static final String EXTRA_CONNECTED_ACCESS_POINT_KEY = "connected_access_point_key";

    private CarWifiManager mCarWifiManager;
    private ProgressBarController mProgressBar;
    private MenuItem mWifiSwitch;
    @Nullable
    private String mConnectedAccessPointKey;

    @Override
    public List<MenuItem> getToolbarMenuItems() {
        return Collections.singletonList(mWifiSwitch);
    }

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.wifi_list_fragment;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mCarWifiManager = new CarWifiManager(getContext());

        mWifiSwitch = new MenuItem.Builder(getContext())
                .setCheckable()
                .setChecked(mCarWifiManager.isWifiEnabled())
                .setOnClickListener(i -> {
                    if (mWifiSwitch.isChecked() != mCarWifiManager.isWifiEnabled()) {
                        mCarWifiManager.setWifiEnabled(mWifiSwitch.isChecked());
                    }
                })
                .build();

        if (savedInstanceState != null) {
            mConnectedAccessPointKey = savedInstanceState.getString(
                    EXTRA_CONNECTED_ACCESS_POINT_KEY);
        }
    }

    @Override
    public void onSaveInstanceState(@NonNull Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putString(EXTRA_CONNECTED_ACCESS_POINT_KEY, mConnectedAccessPointKey);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        mProgressBar = getToolbar().getProgressBar();
    }

    @Override
    public void onStart() {
        super.onStart();
        mCarWifiManager.addListener(this);
        mCarWifiManager.start();
        onWifiStateChanged(mCarWifiManager.getWifiState());
    }

    @Override
    public void onStop() {
        super.onStop();
        mCarWifiManager.removeListener(this);
        mCarWifiManager.stop();
        mProgressBar.setVisible(false);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mCarWifiManager.destroy();
    }

    @Override
    public void onAccessPointsChanged() {
        mProgressBar.setVisible(true);
        getView().postDelayed(() -> mProgressBar.setVisible(false), SEARCHING_DELAY_MILLIS);
        AccessPoint connectedAccessPoint = mCarWifiManager.getConnectedAccessPoint();
        if (connectedAccessPoint != null) {
            String connectedAccessPointKey = connectedAccessPoint.getKey();
            if (!connectedAccessPointKey.equals(mConnectedAccessPointKey)) {
                scrollToPreference(connectedAccessPointKey);
                mConnectedAccessPointKey = connectedAccessPointKey;
            }
        } else {
            mConnectedAccessPointKey = null;
        }
    }

    @Override
    public void onWifiStateChanged(int state) {
        mWifiSwitch.setChecked(state == WifiManager.WIFI_STATE_ENABLED
                || state == WifiManager.WIFI_STATE_ENABLING);
        switch (state) {
            case WifiManager.WIFI_STATE_ENABLING:
                mProgressBar.setVisible(true);
                break;
            default:
                mProgressBar.setVisible(false);
        }
    }

    /**
     * Data provider for Settings Search.
     */
    public static final CarBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new CarBaseSearchIndexProvider(R.xml.wifi_list_fragment, Settings.ACTION_WIFI_SETTINGS);
}
