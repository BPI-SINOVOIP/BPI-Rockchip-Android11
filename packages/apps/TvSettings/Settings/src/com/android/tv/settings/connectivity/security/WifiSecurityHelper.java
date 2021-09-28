/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.tv.settings.connectivity.security;

import android.content.Context;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.text.TextUtils;

import androidx.fragment.app.FragmentActivity;
import androidx.leanback.widget.GuidedAction;
import androidx.lifecycle.ViewModelProviders;

import com.android.settingslib.wifi.AccessPoint;
import com.android.tv.settings.R;
import com.android.tv.settings.connectivity.setup.AdvancedOptionsFlowInfo;
import com.android.tv.settings.connectivity.setup.UserChoiceInfo;

import java.util.ArrayList;
import java.util.List;

/** Helper class to handle Wi-Fi security */
public class WifiSecurityHelper {

    private static final String TAG = "WifiSecurityConfigInfo";

    public static List<GuidedAction> getSecurityTypes(Context context) {
        WifiManager wifiManager = context.getSystemService(WifiManager.class);
        List<GuidedAction> securityTypes = new ArrayList<>();
        securityTypes.add(new GuidedAction.Builder(context)
                .title(context.getString(R.string.wifi_security_type_none))
                .id(AccessPoint.SECURITY_NONE)
                .build());
        if (wifiManager.isEnhancedOpenSupported()) {
            securityTypes.add(new GuidedAction.Builder(context)
                    .title(context.getString(R.string.wifi_security_owe))
                    .id(AccessPoint.SECURITY_OWE)
                    .build());
        }
        securityTypes.add(new GuidedAction.Builder(context)
                .title(context.getString(R.string.wifi_security_wep))
                .id(AccessPoint.SECURITY_WEP)
                .build());
        securityTypes.add(new GuidedAction.Builder(context)
                .title(context.getString(R.string.wifi_security_wpa_wpa2))
                .id(AccessPoint.SECURITY_PSK)
                .build());
        if (wifiManager.isWpa3SaeSupported()) {
            securityTypes.add(new GuidedAction.Builder(context)
                    .title(context.getString(R.string.wifi_security_sae))
                    .id(AccessPoint.SECURITY_SAE)
                    .build());
        }
        return securityTypes;
    }

    public static List<GuidedAction> getActionsList(FragmentActivity context,
            @UserChoiceInfo.PAGE int page) {
        switch (page) {
            case UserChoiceInfo.SECURITY:
                return getSecurityTypes(context);
            default:
                return null;
        }
    }

    public static String getSsid(FragmentActivity context) {
        UserChoiceInfo userChoiceInfo = ViewModelProviders.of(context).get(UserChoiceInfo.class);
        String savedSsid = userChoiceInfo.getPageSummary(UserChoiceInfo.SSID);
        return savedSsid != null
                ? savedSsid : userChoiceInfo.getWifiConfiguration().getPrintableSsid();
    }

    public static Integer getSecurity(FragmentActivity context) {
        UserChoiceInfo userChoiceInfo = ViewModelProviders.of(context).get(UserChoiceInfo.class);
        Integer savedSecurity = userChoiceInfo.getChoice(UserChoiceInfo.SECURITY);
        return savedSecurity != null ? savedSecurity : userChoiceInfo.getWifiSecurity();
    }

    public static WifiConfiguration getConfig(FragmentActivity context) {
        UserChoiceInfo userChoiceInfo = ViewModelProviders.of(context).get(UserChoiceInfo.class);
        AdvancedOptionsFlowInfo advancedOptionsFlowInfo = ViewModelProviders.of(context).get(
                AdvancedOptionsFlowInfo.class);
        WifiConfiguration config = userChoiceInfo.getWifiConfiguration();

        if (TextUtils.isEmpty(config.SSID)) {
            // If the user adds a network manually, assume that it is hidden.
            config.hiddenSSID = true;
        }

        if (userChoiceInfo.getPageSummary(UserChoiceInfo.SSID) != null) {
            config.SSID = AccessPoint.convertToQuotedString(
                    userChoiceInfo.getPageSummary(UserChoiceInfo.SSID));
        }
        int accessPointSecurity = getSecurity(context);
        String password = userChoiceInfo.getPageSummary(UserChoiceInfo.PASSWORD);
        int length = password != null ? password.length() : 0;

        switch (accessPointSecurity) {
            case AccessPoint.SECURITY_NONE:
                config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
                break;

            case AccessPoint.SECURITY_WEP:
                config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
                config.allowedAuthAlgorithms.set(WifiConfiguration.AuthAlgorithm.OPEN);
                config.allowedAuthAlgorithms.set(WifiConfiguration.AuthAlgorithm.SHARED);
                // WEP-40, WEP-104, and 256-bit WEP (WEP-232?)
                if ((length == 10 || length == 26 || length == 58)
                        && password.matches("[0-9A-Fa-f]*")) {
                    config.wepKeys[0] = password;
                } else {
                    config.wepKeys[0] = '"' + password + '"';
                }
                break;

            case AccessPoint.SECURITY_PSK:
                config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_PSK);
                if (length != 0) {
                    if (password.matches("[0-9A-Fa-f]{64}")) {
                        config.preSharedKey = password;
                    } else {
                        config.preSharedKey = '"' + password + '"';
                    }
                }
                break;

            case AccessPoint.SECURITY_SAE:
                config.setSecurityParams(WifiConfiguration.SECURITY_TYPE_SAE);
                if (password.length() != 0) {
                    config.preSharedKey = '"' + password + '"';
                }
                break;
            case AccessPoint.SECURITY_OWE:
                config.setSecurityParams(WifiConfiguration.SECURITY_TYPE_OWE);
                break;

            default:
                return null;
        }

        config.setIpConfiguration(advancedOptionsFlowInfo.getIpConfiguration());
        return config;
    }
}
