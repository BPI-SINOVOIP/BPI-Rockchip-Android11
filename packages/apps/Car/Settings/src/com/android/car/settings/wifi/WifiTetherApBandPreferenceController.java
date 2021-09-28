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
import android.content.res.Resources;
import android.net.wifi.SoftApConfiguration;
import android.util.Log;

import androidx.preference.ListPreference;

import com.android.car.settings.R;
import com.android.car.settings.common.FragmentController;

/**
 * Controls WiFi Hotspot AP Band configuration.
 */
public class WifiTetherApBandPreferenceController extends
        WifiTetherBasePreferenceController<ListPreference> {
    private static final String TAG = "CarWifiTetherApBandPref";

    private String[] mBandEntries;
    private String[] mBandSummaries;
    private int mBand;

    public WifiTetherApBandPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    protected Class<ListPreference> getPreferenceType() {
        return ListPreference.class;
    }

    @Override
    protected void onCreateInternal() {
        super.onCreateInternal();
        updatePreferenceEntries();
        getPreference().setEntries(mBandSummaries);
        getPreference().setEntryValues(mBandEntries);
    }

    @Override
    public void updateState(ListPreference preference) {
        super.updateState(preference);

        SoftApConfiguration config = getCarSoftApConfig();
        if (config == null) {
            mBand = SoftApConfiguration.BAND_2GHZ;
        } else if (!is5GhzBandSupported() && config.getBand() == SoftApConfiguration.BAND_5GHZ) {
            SoftApConfiguration newConfig = new SoftApConfiguration.Builder(config)
                    .setBand(SoftApConfiguration.BAND_2GHZ)
                    .build();
            setCarSoftApConfig(newConfig);
            mBand = newConfig.getBand();
        } else {
            mBand = validateSelection(config.getBand());
        }

        if (!is5GhzBandSupported()) {
            preference.setEnabled(false);
            preference.setSummary(R.string.wifi_ap_choose_2G);
        } else {
            preference.setValue(Integer.toString(config.getBand()));
            preference.setSummary(getSummary());
        }

    }

    @Override
    protected String getSummary() {
        if (!is5GhzBandSupported()) {
            return getContext().getString(R.string.wifi_ap_choose_2G);
        }
        switch (mBand) {
            case SoftApConfiguration.BAND_2GHZ | SoftApConfiguration.BAND_5GHZ:
                return getContext().getString(R.string.wifi_ap_prefer_5G);
            case SoftApConfiguration.BAND_2GHZ:
                return mBandSummaries[0];
            case SoftApConfiguration.BAND_5GHZ:
                return mBandSummaries[1];
            default:
                Log.e(TAG, "Unknown band: " + mBand);
                return getContext().getString(R.string.wifi_ap_prefer_5G);
        }
    }

    @Override
    protected String getDefaultSummary() {
        return null;
    }

    @Override
    public boolean handlePreferenceChanged(ListPreference preference, Object newValue) {
        mBand = validateSelection(Integer.parseInt((String) newValue));
        updateApBand(); // updating AP band because mBandIndex may have been assigned a new value.
        refreshUi();
        return true;
    }

    private int validateSelection(int band) {
        // unsupported states:
        // 1: BAND_5GHZ only - include 2GHZ since some of countries doesn't support 5G hotspot
        // 2: no 5 GHZ support means we can't have BAND_5GHZ - default to 2GHZ
        if (SoftApConfiguration.BAND_5GHZ == band) {
            if (!is5GhzBandSupported()) {
                return SoftApConfiguration.BAND_2GHZ;
            }
            return SoftApConfiguration.BAND_5GHZ | SoftApConfiguration.BAND_2GHZ;
        }
        return band;
    }

    private void updatePreferenceEntries() {
        Resources res = getContext().getResources();
        int entriesRes = R.array.wifi_ap_band;
        int summariesRes = R.array.wifi_ap_band_summary;
        mBandEntries = res.getStringArray(entriesRes);
        mBandSummaries = res.getStringArray(summariesRes);
    }

    private void updateApBand() {
        SoftApConfiguration config = new SoftApConfiguration.Builder(getCarSoftApConfig())
                .setBand(mBand)
                .build();
        setCarSoftApConfig(config);
        getPreference().setValue(getBandEntry());
    }

    private String getBandEntry() {
        switch (mBand) {
            case SoftApConfiguration.BAND_2GHZ | SoftApConfiguration.BAND_5GHZ:
            case SoftApConfiguration.BAND_2GHZ:
                return mBandEntries[0];
            case SoftApConfiguration.BAND_5GHZ:
                return mBandEntries[1];
            default:
                Log.e(TAG, "Unknown band: " + mBand + ", defaulting to 2GHz");
                return mBandEntries[0];
        }
    }

    private boolean is5GhzBandSupported() {
        String countryCode = getCarWifiManager().getCountryCode();
        return getCarWifiManager().is5GhzBandSupported() && countryCode != null;
    }
}
