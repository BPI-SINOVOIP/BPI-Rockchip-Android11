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
import android.net.wifi.WifiManager;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;
import androidx.preference.ListPreference;

import com.android.car.settings.R;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;
import com.android.settingslib.wifi.AccessPoint;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Business logic to select the security type when adding a hidden network. */
public class NetworkSecurityPreferenceController extends PreferenceController<ListPreference> {

    /** Action used in the {@link Intent} sent by the {@link LocalBroadcastManager}. */
    public static final String ACTION_SECURITY_CHANGE =
            "com.android.car.settings.wifi.SecurityChangeAction";
    /** Key used to store the selected security type. */
    public static final String KEY_SECURITY_TYPE = "security_type";

    private static final Map<Integer, Integer> SECURITY_TYPE_TO_DESC_RES =
            createSecurityTypeDescMap();

    private static final List<Integer> SECURITY_TYPES = Arrays.asList(
            AccessPoint.SECURITY_NONE,
            AccessPoint.SECURITY_WEP,
            AccessPoint.SECURITY_PSK,
            AccessPoint.SECURITY_EAP,
            AccessPoint.SECURITY_SAE,
            AccessPoint.SECURITY_OWE,
            AccessPoint.SECURITY_EAP_SUITE_B);

    private CharSequence[] mSecurityTypeNames;
    private CharSequence[] mSecurityTypeIds;
    private int mSelectedSecurityType;

    public NetworkSecurityPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    protected Class<ListPreference> getPreferenceType() {
        return ListPreference.class;
    }

    @Override
    protected void onCreateInternal() {
        // Security type setup.
        List<String> securityTypeNamesList = new ArrayList<String>();
        List<String> securityTypeIdsList = new ArrayList<String>();
        WifiManager wifiManager = getContext().getSystemService(WifiManager.class);

        securityTypeNamesList.add(getContext().getString(
                SECURITY_TYPE_TO_DESC_RES.get(AccessPoint.SECURITY_NONE)));
        securityTypeIdsList.add(Integer.toString(AccessPoint.SECURITY_NONE));

        if (wifiManager.isEnhancedOpenSupported()) {
            securityTypeNamesList.add(getContext().getString(
                    SECURITY_TYPE_TO_DESC_RES.get(AccessPoint.SECURITY_OWE)));
            securityTypeIdsList.add(Integer.toString(AccessPoint.SECURITY_OWE));
        }

        securityTypeNamesList.add(getContext().getString(
                SECURITY_TYPE_TO_DESC_RES.get(AccessPoint.SECURITY_WEP)));
        securityTypeIdsList.add(Integer.toString(AccessPoint.SECURITY_WEP));

        securityTypeNamesList.add(getContext().getString(
                SECURITY_TYPE_TO_DESC_RES.get(AccessPoint.SECURITY_PSK)));
        securityTypeIdsList.add(Integer.toString(AccessPoint.SECURITY_PSK));

        if (wifiManager.isWpa3SaeSupported()) {
            securityTypeNamesList.add(getContext().getString(
                    SECURITY_TYPE_TO_DESC_RES.get(AccessPoint.SECURITY_SAE)));
            securityTypeIdsList.add(Integer.toString(AccessPoint.SECURITY_SAE));
        }

        securityTypeNamesList.add(getContext().getString(
                SECURITY_TYPE_TO_DESC_RES.get(AccessPoint.SECURITY_EAP)));
        securityTypeIdsList.add(Integer.toString(AccessPoint.SECURITY_EAP));

        if (wifiManager.isWpa3SuiteBSupported()) {
            securityTypeNamesList.add(getContext().getString(
                    SECURITY_TYPE_TO_DESC_RES.get(AccessPoint.SECURITY_EAP_SUITE_B)));
            securityTypeIdsList.add(Integer.toString(AccessPoint.SECURITY_EAP_SUITE_B));
        }

        mSelectedSecurityType = AccessPoint.SECURITY_NONE;
        mSecurityTypeNames = new CharSequence[securityTypeNamesList.size()];
        mSecurityTypeNames = securityTypeNamesList.toArray(mSecurityTypeNames);
        mSecurityTypeIds = new CharSequence[securityTypeIdsList.size()];
        mSecurityTypeIds = securityTypeIdsList.toArray(mSecurityTypeIds);

        getPreference().setEntries(mSecurityTypeNames);
        getPreference().setEntryValues(mSecurityTypeIds);
        getPreference().setDefaultValue(Integer.toString(AccessPoint.SECURITY_NONE));
    }

    @Override
    protected void updateState(ListPreference preference) {
        preference.setSummary(SECURITY_TYPE_TO_DESC_RES.get(mSelectedSecurityType));
    }

    @Override
    protected boolean handlePreferenceChanged(ListPreference preference, Object newValue) {
        mSelectedSecurityType = Integer.parseInt(newValue.toString());
        notifySecurityChange(mSelectedSecurityType);
        refreshUi();
        return true;
    }

    private void notifySecurityChange(int securityType) {
        Intent intent = new Intent(ACTION_SECURITY_CHANGE);
        intent.putExtra(KEY_SECURITY_TYPE, securityType);
        LocalBroadcastManager.getInstance(getContext()).sendBroadcastSync(intent);
    }

    private static Map<Integer, Integer> createSecurityTypeDescMap() {
        Map<Integer, Integer> map = new HashMap<>();
        map.put(AccessPoint.SECURITY_NONE, R.string.wifi_security_none);
        map.put(AccessPoint.SECURITY_WEP, R.string.wifi_security_wep);
        map.put(AccessPoint.SECURITY_PSK, R.string.wifi_security_psk_generic);
        map.put(AccessPoint.SECURITY_EAP, R.string.wifi_security_eap);
        map.put(AccessPoint.SECURITY_SAE, R.string.wifi_security_sae);
        map.put(AccessPoint.SECURITY_OWE, R.string.wifi_security_owe);
        map.put(AccessPoint.SECURITY_EAP_SUITE_B, R.string.wifi_security_eap_suiteb);
        return map;
    }
}
