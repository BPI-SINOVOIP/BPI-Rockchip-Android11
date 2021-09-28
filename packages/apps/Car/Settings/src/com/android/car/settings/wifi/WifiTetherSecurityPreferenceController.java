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
import android.content.SharedPreferences;
import android.net.wifi.SoftApConfiguration;

import androidx.preference.ListPreference;

import com.android.car.settings.R;
import com.android.car.settings.common.FragmentController;

/**
 * Controls WiFi Hotspot Security Type configuration.
 */
public class WifiTetherSecurityPreferenceController extends
        WifiTetherBasePreferenceController<ListPreference> {

    protected static final String KEY_SECURITY_TYPE =
            "com.android.car.settings.wifi.KEY_SECURITY_TYPE";

    private int mSecurityType;

    private final SharedPreferences mSharedPreferences = getContext().getSharedPreferences(
                    WifiTetherPasswordPreferenceController.SHARED_PREFERENCE_PATH,
                    Context.MODE_PRIVATE);

    public WifiTetherSecurityPreferenceController(Context context, String preferenceKey,
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
        mSecurityType = getCarSoftApConfig().getSecurityType();
        getPreference().setEntries(
                getContext().getResources().getStringArray(R.array.wifi_tether_security));
        String[] entryValues = {
                Integer.toString(SoftApConfiguration.SECURITY_TYPE_WPA2_PSK),
                Integer.toString(SoftApConfiguration.SECURITY_TYPE_OPEN)};
        getPreference().setEntryValues(entryValues);
        getPreference().setValue(String.valueOf(mSecurityType));
    }

    @Override
    protected boolean handlePreferenceChanged(ListPreference preference,
            Object newValue) {
        mSecurityType = Integer.parseInt(newValue.toString());
        // Rather than updating the ap config here, we will only update the security type shared
        // preference. When the user confirms their selection by going back, the config will be
        // updated by the WifiTetherPasswordPreferenceController. By updating the config in that
        // controller, we avoid running into a transient state where the (securityType, passphrase)
        // pair is invalid due to not being updated simultaneously.
        mSharedPreferences.edit().putInt(KEY_SECURITY_TYPE, mSecurityType).commit();
        refreshUi();
        return true;
    }

    @Override
    protected void updateState(ListPreference preference) {
        super.updateState(preference);
        preference.setValue(Integer.toString(mSecurityType));
    }

    @Override
    protected String getSummary() {
        int stringResId = mSecurityType == SoftApConfiguration.SECURITY_TYPE_WPA2_PSK
                ? R.string.wifi_hotspot_wpa2_personal : R.string.wifi_hotspot_security_none;
        return getContext().getString(stringResId);
    }

    @Override
    protected String getDefaultSummary() {
        return null;
    }
}
