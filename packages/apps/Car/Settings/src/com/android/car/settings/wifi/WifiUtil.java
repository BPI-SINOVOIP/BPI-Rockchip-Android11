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
 * limitations under the License
 */
package com.android.car.settings.wifi;

import static android.net.wifi.WifiConfiguration.NetworkSelectionStatus.NETWORK_SELECTION_ENABLED;

import android.annotation.DrawableRes;
import android.annotation.Nullable;
import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.PackageManager;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.provider.Settings;
import android.text.TextUtils;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;

import com.android.car.settings.R;
import com.android.car.settings.common.Logger;
import com.android.settingslib.wifi.AccessPoint;

import java.util.regex.Pattern;

/**
 * A collections of util functions for WIFI.
 */
public class WifiUtil {

    private static final Logger LOG = new Logger(WifiUtil.class);

    /** Value that is returned when we fail to connect wifi. */
    public static final int INVALID_NET_ID = -1;
    private static final Pattern HEX_PATTERN = Pattern.compile("^[0-9A-F]+$");

    @DrawableRes
    public static int getIconRes(int state) {
        switch (state) {
            case WifiManager.WIFI_STATE_ENABLING:
            case WifiManager.WIFI_STATE_DISABLED:
                return R.drawable.ic_settings_wifi_disabled;
            default:
                return R.drawable.ic_settings_wifi;
        }
    }

    public static boolean isWifiOn(int state) {
        switch (state) {
            case WifiManager.WIFI_STATE_ENABLING:
            case WifiManager.WIFI_STATE_DISABLED:
                return false;
            default:
                return true;
        }
    }

    /**
     * @return 0 if no proper description can be found.
     */
    @StringRes
    public static Integer getStateDesc(int state) {
        switch (state) {
            case WifiManager.WIFI_STATE_ENABLING:
                return R.string.wifi_starting;
            case WifiManager.WIFI_STATE_DISABLING:
                return R.string.wifi_stopping;
            case WifiManager.WIFI_STATE_DISABLED:
                return R.string.wifi_disabled;
            default:
                return 0;
        }
    }

    /**
     * Returns {@Code true} if wifi is available on this device.
     */
    public static boolean isWifiAvailable(Context context) {
        return context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_WIFI);
    }

    /**
     * Gets a unique key for a {@link AccessPoint}.
     */
    public static String getKey(AccessPoint accessPoint) {
        return String.valueOf(accessPoint.hashCode());
    }

    /**
     * This method is a stripped and negated version of WifiConfigStore.canModifyNetwork.
     *
     * @param context Context of caller
     * @param config  The WiFi config.
     * @return {@code true} if Settings cannot modify the config due to lockDown.
     */
    public static boolean isNetworkLockedDown(Context context, WifiConfiguration config) {
        if (config == null) {
            return false;
        }

        final DevicePolicyManager dpm =
                (DevicePolicyManager) context.getSystemService(Context.DEVICE_POLICY_SERVICE);
        final PackageManager pm = context.getPackageManager();

        // Check if device has DPM capability. If it has and dpm is still null, then we
        // treat this case with suspicion and bail out.
        if (pm.hasSystemFeature(PackageManager.FEATURE_DEVICE_ADMIN) && dpm == null) {
            return true;
        }

        boolean isConfigEligibleForLockdown = false;
        if (dpm != null) {
            final ComponentName deviceOwner = dpm.getDeviceOwnerComponentOnAnyUser();
            if (deviceOwner != null) {
                final int deviceOwnerUserId = dpm.getDeviceOwnerUserId();
                try {
                    final int deviceOwnerUid = pm.getPackageUidAsUser(deviceOwner.getPackageName(),
                            deviceOwnerUserId);
                    isConfigEligibleForLockdown = deviceOwnerUid == config.creatorUid;
                } catch (PackageManager.NameNotFoundException e) {
                    // don't care
                }
            }
        }
        if (!isConfigEligibleForLockdown) {
            return false;
        }

        final ContentResolver resolver = context.getContentResolver();
        final boolean isLockdownFeatureEnabled = Settings.Global.getInt(resolver,
                Settings.Global.WIFI_DEVICE_OWNER_CONFIGS_LOCKDOWN, 0) != 0;
        return isLockdownFeatureEnabled;
    }

    /**
     * Returns {@code true} if the network security type doesn't require authentication.
     */
    public static boolean isOpenNetwork(int security) {
        return security == AccessPoint.SECURITY_NONE || security == AccessPoint.SECURITY_OWE;
    }

    /**
     * Returns {@code true} if the provided NetworkCapabilities indicate a captive portal network.
     */
    public static boolean canSignIntoNetwork(NetworkCapabilities capabilities) {
        return (capabilities != null
                && capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_CAPTIVE_PORTAL));
    }

    /**
     * Attempts to connect to a specified access point
     * @param listener for callbacks on success or failure of connection attempt (can be null)
     */
    public static void connectToAccessPoint(Context context, String ssid, int security,
            String password, boolean hidden, @Nullable WifiManager.ActionListener listener) {
        WifiManager wifiManager = context.getSystemService(WifiManager.class);
        WifiConfiguration wifiConfig = getWifiConfig(ssid, security, password, hidden);
        wifiManager.connect(wifiConfig, listener);
    }

    private static WifiConfiguration getWifiConfig(String ssid, int security,
            String password, boolean hidden) {
        WifiConfiguration wifiConfig = new WifiConfiguration();
        wifiConfig.SSID = String.format("\"%s\"", ssid);
        wifiConfig.hiddenSSID = hidden;

        return finishWifiConfig(wifiConfig, security, password);
    }

    /** Similar to above, but uses AccessPoint to get additional relevant information. */
    public static WifiConfiguration getWifiConfig(@NonNull AccessPoint accessPoint,
            String password) {
        if (accessPoint == null) {
            throw new IllegalArgumentException("AccessPoint input is required.");
        }

        WifiConfiguration wifiConfig = new WifiConfiguration();
        if (!accessPoint.isSaved()) {
            wifiConfig.SSID = AccessPoint.convertToQuotedString(
                    accessPoint.getSsidStr());
        } else {
            wifiConfig.networkId = accessPoint.getConfig().networkId;
            wifiConfig.hiddenSSID = accessPoint.getConfig().hiddenSSID;
        }

        return finishWifiConfig(wifiConfig, accessPoint.getSecurity(), password);
    }

    private static WifiConfiguration finishWifiConfig(WifiConfiguration wifiConfig, int security,
            String password) {
        switch (security) {
            case AccessPoint.SECURITY_NONE:
                wifiConfig.setSecurityParams(WifiConfiguration.SECURITY_TYPE_OPEN);
                break;
            case AccessPoint.SECURITY_WEP:
                wifiConfig.setSecurityParams(WifiConfiguration.SECURITY_TYPE_WEP);
                if (!TextUtils.isEmpty(password)) {
                    int length = password.length();
                    // WEP-40, WEP-104, and 256-bit WEP (WEP-232?)
                    if ((length == 10 || length == 26 || length == 58)
                            && password.matches("[0-9A-Fa-f]*")) {
                        wifiConfig.wepKeys[0] = password;
                    } else {
                        wifiConfig.wepKeys[0] = '"' + password + '"';
                    }
                }
                break;
            case AccessPoint.SECURITY_PSK:
                wifiConfig.setSecurityParams(WifiConfiguration.SECURITY_TYPE_PSK);
                if (!TextUtils.isEmpty(password)) {
                    if (password.matches("[0-9A-Fa-f]{64}")) {
                        wifiConfig.preSharedKey = password;
                    } else {
                        wifiConfig.preSharedKey = '"' + password + '"';
                    }
                }
                break;
            case AccessPoint.SECURITY_EAP:
            case AccessPoint.SECURITY_EAP_SUITE_B:
                if (security == AccessPoint.SECURITY_EAP_SUITE_B) {
                    // allowedSuiteBCiphers will be set according to certificate type
                    wifiConfig.setSecurityParams(WifiConfiguration.SECURITY_TYPE_EAP_SUITE_B);
                } else {
                    wifiConfig.setSecurityParams(WifiConfiguration.SECURITY_TYPE_EAP);
                }
                if (!TextUtils.isEmpty(password)) {
                    wifiConfig.enterpriseConfig.setPassword(password);
                }
                break;
            case AccessPoint.SECURITY_SAE:
                wifiConfig.setSecurityParams(WifiConfiguration.SECURITY_TYPE_SAE);
                if (!TextUtils.isEmpty(password)) {
                    wifiConfig.preSharedKey = '"' + password + '"';
                }
                break;
            case AccessPoint.SECURITY_OWE:
                wifiConfig.setSecurityParams(WifiConfiguration.SECURITY_TYPE_OWE);
                break;
            default:
                throw new IllegalArgumentException("unknown security type " + security);
        }
        return wifiConfig;
    }

    /** Forget the network specified by {@code accessPoint}. */
    public static void forget(Context context, AccessPoint accessPoint) {
        WifiManager wifiManager = context.getSystemService(WifiManager.class);
        if (!accessPoint.isSaved()) {
            if (accessPoint.getNetworkInfo() != null
                    && accessPoint.getNetworkInfo().getState() != NetworkInfo.State.DISCONNECTED) {
                // Network is active but has no network ID - must be ephemeral.
                wifiManager.disableEphemeralNetwork(
                        AccessPoint.convertToQuotedString(accessPoint.getSsidStr()));
            } else {
                // Should not happen, but a monkey seems to trigger it
                LOG.e("Failed to forget invalid network " + accessPoint.getConfig());
                return;
            }
        } else {
            wifiManager.forget(accessPoint.getConfig().networkId, new WifiManager.ActionListener() {
                @Override
                public void onSuccess() {
                    LOG.d("Network successfully forgotten");
                }

                @Override
                public void onFailure(int reason) {
                    LOG.d("Could not forget network. Failure code: " + reason);
                    Toast.makeText(context, R.string.wifi_failed_forget_message,
                            Toast.LENGTH_SHORT).show();
                }
            });
        }
    }

    /** Returns {@code true} if the access point was disabled due to the wrong password. */
    public static boolean isAccessPointDisabledByWrongPassword(AccessPoint accessPoint) {
        WifiConfiguration config = accessPoint.getConfig();
        if (config == null) {
            return false;
        }
        WifiConfiguration.NetworkSelectionStatus networkStatus =
                config.getNetworkSelectionStatus();
        if (networkStatus == null
                || networkStatus.getNetworkSelectionStatus() == NETWORK_SELECTION_ENABLED) {
            return false;
        }
        return networkStatus.getNetworkSelectionDisableReason()
                == WifiConfiguration.NetworkSelectionStatus.DISABLED_BY_WRONG_PASSWORD;
    }

    private static boolean isHexString(String password) {
        return HEX_PATTERN.matcher(password).matches();
    }
}
