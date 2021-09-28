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
package com.android.tv.settings.connectivity;

import android.net.wifi.ScanResult;
import android.net.wifi.WifiConfiguration;
import android.text.TextUtils;

import com.android.settingslib.wifi.AccessPoint;

/** Helper class for Wifi configuration. */
public class WifiUtils {
    /**
     * Gets security value from ScanResult.
     *
     * Duplicated method from {@link AccessPoint#getSecurity(ScanResult)}.
     * TODO(b/120827021): Should be removed if the there is have a common one in shared place (e.g.
     * SettingsLib).
     *
     * @param result ScanResult
     * @return Related security value based on {@link AccessPoint}.
     */
    public static int getAccessPointSecurity(ScanResult result) {
        if (result.capabilities.contains("WEP")) {
            return AccessPoint.SECURITY_WEP;
        } else if (result.capabilities.contains("SAE")) {
            return AccessPoint.SECURITY_SAE;
        } else if (result.capabilities.contains("PSK")) {
            return AccessPoint.SECURITY_PSK;
        } else if (result.capabilities.contains("EAP_SUITE_B_192")) {
            return AccessPoint.SECURITY_EAP_SUITE_B;
        } else if (result.capabilities.contains("EAP")) {
            return AccessPoint.SECURITY_EAP;
        } else if (result.capabilities.contains("OWE")) {
            return AccessPoint.SECURITY_OWE;
        }

        return AccessPoint.SECURITY_NONE;
    }

    /**
     * Provides a simple way to generate a new {@link WifiConfiguration} obj from
     * {@link ScanResult} or {@link AccessPoint}. Either {@code accessPoint} or {@code scanResult
     * } input should be not null for retrieving information, otherwise will throw
     * IllegalArgumentException.
     * This method prefers to take {@link AccessPoint} input in priority. Therefore this method
     * will take {@link AccessPoint} input as preferred data extraction source when you input
     * both {@link AccessPoint} and {@link ScanResult}, and ignore {@link ScanResult} input.
     *
     * Duplicated and simplified method from {@link WifiConfigController#getConfig()}.
     * TODO(b/120827021): Should be removed if the there is have a common one in shared place (e.g.
     * SettingsLib).
     *
     * @param accessPoint Input data for retrieving WifiConfiguration.
     * @param scanResult  Input data for retrieving WifiConfiguration.
     * @return WifiConfiguration obj based on input.
     */
    public static WifiConfiguration getWifiConfig(AccessPoint accessPoint, ScanResult scanResult,
            String password) {
        if (accessPoint == null && scanResult == null) {
            throw new IllegalArgumentException(
                    "At least one of AccessPoint and ScanResult input is required.");
        }

        final WifiConfiguration config = new WifiConfiguration();
        final int security;

        if (accessPoint == null) {
            config.SSID = AccessPoint.convertToQuotedString(scanResult.SSID);
            security = getAccessPointSecurity(scanResult);
        } else {
            if (!accessPoint.isSaved()) {
                config.SSID = AccessPoint.convertToQuotedString(
                        accessPoint.getSsidStr());
            } else {
                config.networkId = accessPoint.getConfig().networkId;
                config.hiddenSSID = accessPoint.getConfig().hiddenSSID;
            }
            security = accessPoint.getSecurity();
        }

        switch (security) {
            case AccessPoint.SECURITY_NONE:
                config.setSecurityParams(WifiConfiguration.SECURITY_TYPE_OPEN);
                break;

            case AccessPoint.SECURITY_WEP:
                config.setSecurityParams(WifiConfiguration.SECURITY_TYPE_WEP);
                if (!TextUtils.isEmpty(password)) {
                    int length = password.length();
                    // WEP-40, WEP-104, and 256-bit WEP (WEP-232?)
                    if ((length == 10 || length == 26 || length == 58)
                            && password.matches("[0-9A-Fa-f]*")) {
                        config.wepKeys[0] = password;
                    } else {
                        config.wepKeys[0] = '"' + password + '"';
                    }
                }
                break;

            case AccessPoint.SECURITY_PSK:
                config.setSecurityParams(WifiConfiguration.SECURITY_TYPE_PSK);
                if (!TextUtils.isEmpty(password)) {
                    if (password.matches("[0-9A-Fa-f]{64}")) {
                        config.preSharedKey = password;
                    } else {
                        config.preSharedKey = '"' + password + '"';
                    }
                }
                break;

            case AccessPoint.SECURITY_EAP:
            case AccessPoint.SECURITY_EAP_SUITE_B:
                if (security == AccessPoint.SECURITY_EAP_SUITE_B) {
                    // allowedSuiteBCiphers will be set according to certificate type
                    config.setSecurityParams(WifiConfiguration.SECURITY_TYPE_EAP_SUITE_B);
                } else {
                    config.setSecurityParams(WifiConfiguration.SECURITY_TYPE_EAP);
                }

                if (!TextUtils.isEmpty(password)) {
                    config.enterpriseConfig.setPassword(password);
                }
                break;
            case AccessPoint.SECURITY_SAE:
                config.setSecurityParams(WifiConfiguration.SECURITY_TYPE_SAE);
                if (!TextUtils.isEmpty(password)) {
                    config.preSharedKey = '"' + password + '"';
                }
                break;

            case AccessPoint.SECURITY_OWE:
                config.setSecurityParams(WifiConfiguration.SECURITY_TYPE_OWE);
                break;

            default:
                break;
        }

        return config;
    }
}
