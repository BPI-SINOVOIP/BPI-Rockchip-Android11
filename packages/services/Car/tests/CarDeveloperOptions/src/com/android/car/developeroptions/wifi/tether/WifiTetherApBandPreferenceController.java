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

package com.android.car.developeroptions.wifi.tether;

import android.content.Context;
import android.content.res.Resources;
import android.net.wifi.SoftApConfiguration;
import android.util.Log;

import androidx.annotation.VisibleForTesting;
import androidx.preference.ListPreference;
import androidx.preference.Preference;

import com.android.car.developeroptions.R;

public class WifiTetherApBandPreferenceController extends WifiTetherBasePreferenceController {

    private static final String TAG = "WifiTetherApBandPref";
    private static final String PREF_KEY = "wifi_tether_network_ap_band";

    private String[] mBandEntries;
    private String[] mBandSummaries;
    private int mBand;

    public WifiTetherApBandPreferenceController(Context context,
            OnTetherConfigUpdateListener listener) {
        super(context, listener);
        updatePreferenceEntries();
    }

    @Override
    public void updateDisplay() {
        final SoftApConfiguration config = mWifiManager.getSoftApConfiguration();
        if (config == null) {
            mBand = SoftApConfiguration.BAND_2GHZ;
            Log.d(TAG, "Updating band to 2GHz because no config");
        } else if (is5GhzBandSupported()) {
            mBand = validateSelection(config.getBand());
            Log.d(TAG, "Updating band to " + mBand);
        } else {
            SoftApConfiguration newConfig = new SoftApConfiguration.Builder(config)
                    .setBand(SoftApConfiguration.BAND_2GHZ)
                    .build();
            mWifiManager.setSoftApConfiguration(newConfig);
            mBand = newConfig.getBand();
            Log.d(TAG, "5Ghz not supported, updating band to " + mBand);
        }
        ListPreference preference = (ListPreference) mPreference;
        preference.setEntries(mBandSummaries);
        preference.setEntryValues(mBandEntries);

        if (!is5GhzBandSupported()) {
            preference.setEnabled(false);
            preference.setSummary(R.string.wifi_ap_choose_2G);
        } else {
            preference.setValue(Integer.toString(config.getBand()));
            preference.setSummary(getConfigSummary());
        }
    }

    String getConfigSummary() {
        switch (mBand) {
            case SoftApConfiguration.BAND_2GHZ | SoftApConfiguration.BAND_5GHZ:
                return mContext.getString(R.string.wifi_ap_prefer_5G);
            case SoftApConfiguration.BAND_2GHZ:
                return mBandSummaries[0];
            case SoftApConfiguration.BAND_5GHZ:
                return mBandSummaries[1];
            default:
                Log.e(TAG, "Unknown band: " + mBand);
                return mContext.getString(R.string.wifi_ap_prefer_5G);
        }
    }

    @Override
    public String getPreferenceKey() {
        return PREF_KEY;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        mBand = validateSelection(Integer.parseInt((String) newValue));
        Log.d(TAG, "Band preference changed, updating band to " + mBand);
        preference.setSummary(getConfigSummary());
        mListener.onTetherConfigUpdated();
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

    @VisibleForTesting
    void updatePreferenceEntries() {
        Resources res = mContext.getResources();
        int entriesRes = R.array.wifi_ap_band;
        int summariesRes = R.array.wifi_ap_band_summary;
        mBandEntries = res.getStringArray(entriesRes);
        mBandSummaries = res.getStringArray(summariesRes);
    }

    private boolean is5GhzBandSupported() {
        final String countryCode = mWifiManager.getCountryCode();
        return mWifiManager.is5GHzBandSupported() && countryCode != null;
    }

    public int getBand() {
        return mBand;
    }
}
