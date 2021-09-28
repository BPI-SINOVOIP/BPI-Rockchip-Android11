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

package com.android.car.developeroptions.wifi.dpp;

import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;
import android.hardware.biometrics.BiometricPrompt;
import android.net.wifi.SoftApConfiguration;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiConfiguration.KeyMgmt;
import android.net.wifi.WifiManager;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.Looper;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.text.TextUtils;

import com.android.car.developeroptions.R;
import com.android.settingslib.wifi.AccessPoint;

import java.time.Duration;
import java.util.List;

/**
 * Here are the items shared by both WifiDppConfiguratorActivity & WifiDppEnrolleeActivity
 *
 * @see WifiQrCode
 */
public class WifiDppUtils {
    /**
     * The fragment tag specified to FragmentManager for container activities to manage fragments.
     */
    public static final String TAG_FRAGMENT_QR_CODE_SCANNER = "qr_code_scanner_fragment";

    /**
     * @see #TAG_FRAGMENT_QR_CODE_SCANNER
     */
    public static final String TAG_FRAGMENT_QR_CODE_GENERATOR = "qr_code_generator_fragment";

    /**
     * @see #TAG_FRAGMENT_QR_CODE_SCANNER
     */
    public static final String TAG_FRAGMENT_CHOOSE_SAVED_WIFI_NETWORK =
            "choose_saved_wifi_network_fragment";

    /**
     * @see #TAG_FRAGMENT_QR_CODE_SCANNER
     */
    public static final String TAG_FRAGMENT_ADD_DEVICE = "add_device_fragment";

    /** The data is from {@code com.android.settingslib.wifi.AccessPoint.securityToString} */
    public static final String EXTRA_WIFI_SECURITY = "security";

    /** The data corresponding to {@code WifiConfiguration} SSID */
    public static final String EXTRA_WIFI_SSID = "ssid";

    /** The data corresponding to {@code WifiConfiguration} preSharedKey */
    public static final String EXTRA_WIFI_PRE_SHARED_KEY = "preSharedKey";

    /** The data corresponding to {@code WifiConfiguration} hiddenSSID */
    public static final String EXTRA_WIFI_HIDDEN_SSID = "hiddenSsid";

    /** The data corresponding to {@code WifiConfiguration} networkId */
    public static final String EXTRA_WIFI_NETWORK_ID = "networkId";

    /** The data to recognize if it's a Wi-Fi hotspot for configuration */
    public static final String EXTRA_IS_HOTSPOT = "isHotspot";

    /** Used by {@link android.provider.Settings#ACTION_PROCESS_WIFI_EASY_CONNECT_URI} to
     * indicate test mode UI should be shown. Test UI does not make API calls. Value is a boolean.*/
    public static final String EXTRA_TEST = "test";

    /**
     * Default status code for Easy Connect
     */
    public static final int EASY_CONNECT_EVENT_FAILURE_NONE = 0;

    /**
     * Success status code for Easy Connect.
     */
    public static final int EASY_CONNECT_EVENT_SUCCESS = 1;

    private static final Duration VIBRATE_DURATION_QR_CODE_RECOGNITION = Duration.ofMillis(3);

    /**
     * Returns whether the device support WiFi DPP.
     */
    public static boolean isWifiDppEnabled(Context context) {
        final WifiManager manager = context.getSystemService(WifiManager.class);
        return manager.isEasyConnectSupported();
    }

    /**
     * Returns an intent to launch QR code scanner for Wi-Fi DPP enrollee.
     *
     * After enrollee success, the callee activity will return connecting WifiConfiguration by
     * putExtra {@code WifiDialogActivity.KEY_WIFI_CONFIGURATION} for
     * {@code Activity#setResult(int resultCode, Intent data)}. The calling object should check
     * if it's available before using it.
     *
     * @param ssid The data corresponding to {@code WifiConfiguration} SSID
     * @return Intent for launching QR code scanner
     */
    public static Intent getEnrolleeQrCodeScannerIntent(String ssid) {
        final Intent intent = new Intent(
                WifiDppEnrolleeActivity.ACTION_ENROLLEE_QR_CODE_SCANNER);
        if (!TextUtils.isEmpty(ssid)) {
            intent.putExtra(EXTRA_WIFI_SSID, ssid);
        }
        return intent;
    }

    private static String getPresharedKey(WifiManager wifiManager, WifiConfiguration config) {
        String preSharedKey = config.preSharedKey;

        final List<WifiConfiguration> wifiConfigs = wifiManager.getPrivilegedConfiguredNetworks();
        for (WifiConfiguration wifiConfig : wifiConfigs) {
            if (wifiConfig.networkId == config.networkId) {
                preSharedKey = wifiConfig.preSharedKey;
                break;
            }
        }

        return preSharedKey;
    }

    private static String removeFirstAndLastDoubleQuotes(String str) {
        if (TextUtils.isEmpty(str)) {
            return str;
        }

        int begin = 0;
        int end = str.length() - 1;
        if (str.charAt(begin) == '\"') {
            begin++;
        }
        if (str.charAt(end) == '\"') {
            end--;
        }
        return str.substring(begin, end+1);
    }

    static String getSecurityString(WifiConfiguration config) {
        if (config.allowedKeyManagement.get(KeyMgmt.SAE)) {
            return WifiQrCode.SECURITY_SAE;
        }
        if (config.allowedKeyManagement.get(KeyMgmt.OWE)) {
            return WifiQrCode.SECURITY_NO_PASSWORD;
        }
        if (config.allowedKeyManagement.get(KeyMgmt.WPA_PSK) ||
                config.allowedKeyManagement.get(KeyMgmt.WPA2_PSK)) {
            return WifiQrCode.SECURITY_WPA_PSK;
        }
        return (config.wepKeys[0] == null) ?
                WifiQrCode.SECURITY_NO_PASSWORD : WifiQrCode.SECURITY_WEP;
    }

    private static String getSecurityString(SoftApConfiguration config) {
        switch (config.getSecurityType()) {
            case SoftApConfiguration.SECURITY_TYPE_WPA3_SAE:
            // TODO: add support for SoftApConfiguration.SECURITY_TYPE_WPA3_SAE_TRANSITION
                return WifiQrCode.SECURITY_SAE;
            case SoftApConfiguration.SECURITY_TYPE_WPA2_PSK:
                return WifiQrCode.SECURITY_WPA_PSK;
            case SoftApConfiguration.SECURITY_TYPE_OPEN:
            default:
                return WifiQrCode.SECURITY_NO_PASSWORD;
        }
    }

    /**
     * Returns an intent to launch QR code generator. It may return null if the security is not
     * supported by QR code generator.
     *
     * Do not use this method for Wi-Fi hotspot network, use
     * {@code getHotspotConfiguratorIntentOrNull} instead.
     *
     * @param context     The context to use for the content resolver
     * @param wifiManager An instance of {@link WifiManager}
     * @param accessPoint An instance of {@link AccessPoint}
     * @return Intent for launching QR code generator
     */
    public static Intent getConfiguratorQrCodeGeneratorIntentOrNull(Context context,
            WifiManager wifiManager, AccessPoint accessPoint) {
        final Intent intent = new Intent(context, WifiDppConfiguratorActivity.class);
        if (isSupportConfiguratorQrCodeGenerator(context, accessPoint)) {
            intent.setAction(WifiDppConfiguratorActivity.ACTION_CONFIGURATOR_QR_CODE_GENERATOR);
        } else {
            return null;
        }

        final WifiConfiguration wifiConfiguration = accessPoint.getConfig();
        setConfiguratorIntentExtra(intent, wifiManager, wifiConfiguration);

        return intent;
    }

    /**
     * Returns an intent to launch QR code scanner. It may return null if the security is not
     * supported by QR code scanner.
     *
     * @param context     The context to use for the content resolver
     * @param wifiManager An instance of {@link WifiManager}
     * @param accessPoint An instance of {@link AccessPoint}
     * @return Intent for launching QR code scanner
     */
    public static Intent getConfiguratorQrCodeScannerIntentOrNull(Context context,
            WifiManager wifiManager, AccessPoint accessPoint) {
        final Intent intent = new Intent(context, WifiDppConfiguratorActivity.class);
        if (isSupportConfiguratorQrCodeScanner(context, accessPoint)) {
            intent.setAction(WifiDppConfiguratorActivity.ACTION_CONFIGURATOR_QR_CODE_SCANNER);
        } else {
            return null;
        }

        final WifiConfiguration wifiConfiguration = accessPoint.getConfig();
        setConfiguratorIntentExtra(intent, wifiManager, wifiConfiguration);

        if (wifiConfiguration.networkId == WifiConfiguration.INVALID_NETWORK_ID) {
            throw new IllegalArgumentException("Invalid network ID");
        } else {
            intent.putExtra(EXTRA_WIFI_NETWORK_ID, wifiConfiguration.networkId);
        }

        return intent;
    }

    /**
     * Returns an intent to launch QR code generator for the Wi-Fi hotspot. It may return null if
     * the security is not supported by QR code generator.
     *
     * @param context The context to use for the content resolver
     * @param softApConfiguration {@link WifiConfiguration} of the Wi-Fi hotspot
     * @return Intent for launching QR code generator
     */
    public static Intent getHotspotConfiguratorIntentOrNull(Context context,
            SoftApConfiguration softApConfiguration) {
        final Intent intent = new Intent(context, WifiDppConfiguratorActivity.class);
        if (isSupportHotspotConfiguratorQrCodeGenerator(softApConfiguration)) {
            intent.setAction(WifiDppConfiguratorActivity.ACTION_CONFIGURATOR_QR_CODE_GENERATOR);
        } else {
            return null;
        }

        setConfiguratorIntentExtra(intent, softApConfiguration);

        intent.putExtra(EXTRA_WIFI_NETWORK_ID, WifiConfiguration.INVALID_NETWORK_ID);
        intent.putExtra(EXTRA_IS_HOTSPOT, true);

        return intent;
    }

    /**
     * Set all extra except {@code EXTRA_WIFI_NETWORK_ID} for the intent to
     * launch configurator activity later.
     *
     * @param intent the target to set extra
     * @param wifiManager an instance of {@code WifiManager}
     * @param wifiConfiguration the Wi-Fi network for launching configurator activity
     */
    private static void setConfiguratorIntentExtra(Intent intent, WifiManager wifiManager,
            WifiConfiguration wifiConfiguration) {
        final String ssid = removeFirstAndLastDoubleQuotes(wifiConfiguration.SSID);
        final String security = getSecurityString(wifiConfiguration);
        String preSharedKey = wifiConfiguration.preSharedKey;

        if (preSharedKey != null) {
            // When the value of this key is read, the actual key is not returned, just a "*".
            // Call privileged system API to obtain actual key.
            preSharedKey = removeFirstAndLastDoubleQuotes(getPresharedKey(wifiManager,
                    wifiConfiguration));
        }

        if (!TextUtils.isEmpty(ssid)) {
            intent.putExtra(EXTRA_WIFI_SSID, ssid);
        }
        if (!TextUtils.isEmpty(security)) {
            intent.putExtra(EXTRA_WIFI_SECURITY, security);
        }
        if (!TextUtils.isEmpty(preSharedKey)) {
            intent.putExtra(EXTRA_WIFI_PRE_SHARED_KEY, preSharedKey);
        }
    }

    /**
     * Set all extra except {@code EXTRA_WIFI_NETWORK_ID} for the intent to
     * launch configurator activity later.
     *
     * @param intent the target to set extra
     * @param softApConfig the Wi-Fi network for launching configurator activity
     */
    private static void setConfiguratorIntentExtra(
            Intent intent, SoftApConfiguration softApConfig) {
        final String ssid = softApConfig.getSsid();
        final String security = getSecurityString(softApConfig);
        String preSharedKey = softApConfig.getPassphrase();

        if (!TextUtils.isEmpty(ssid)) {
            intent.putExtra(EXTRA_WIFI_SSID, ssid);
        }
        if (!TextUtils.isEmpty(security)) {
            intent.putExtra(EXTRA_WIFI_SECURITY, security);
        }
        if (!TextUtils.isEmpty(preSharedKey)) {
            intent.putExtra(EXTRA_WIFI_PRE_SHARED_KEY, preSharedKey);
        }
    }

    /**
     * Shows authentication screen to confirm credentials (pin, pattern or password) for the current
     * user of the device.
     *
     * @param context The {@code Context} used to get {@code KeyguardManager} service
     * @param successRunnable The {@code Runnable} which will be executed if the user does not setup
     *                        device security or if lock screen is unlocked
     */
    public static void showLockScreen(Context context, Runnable successRunnable) {
        final KeyguardManager keyguardManager = (KeyguardManager) context.getSystemService(
                Context.KEYGUARD_SERVICE);

        if (keyguardManager.isKeyguardSecure()) {
            final BiometricPrompt.AuthenticationCallback authenticationCallback =
                    new BiometricPrompt.AuthenticationCallback() {
                        @Override
                        public void onAuthenticationSucceeded(
                                    BiometricPrompt.AuthenticationResult result) {
                            successRunnable.run();
                        }

                        @Override
                        public void onAuthenticationError(int errorCode, CharSequence errString) {
                            //Do nothing
                        }
            };

            final BiometricPrompt.Builder builder = new BiometricPrompt.Builder(context)
                    .setTitle(context.getText(R.string.wifi_dpp_lockscreen_title));

            if (keyguardManager.isDeviceSecure()) {
                builder.setDeviceCredentialAllowed(true);
            }

            final BiometricPrompt bp = builder.build();
            final Handler handler = new Handler(Looper.getMainLooper());
            bp.authenticate(new CancellationSignal(),
                    runnable -> handler.post(runnable),
                    authenticationCallback);
        } else {
            successRunnable.run();
        }
    }

    /**
     * Checks if QR code scanner supports to config other devices with the Wi-Fi network
     *
     * @param context The context to use for {@link WifiManager#isEasyConnectSupported()}
     * @param accessPoint The {@link AccessPoint} of the Wi-Fi network
     */
    public static boolean isSupportConfiguratorQrCodeScanner(Context context,
            AccessPoint accessPoint) {
        return isSupportWifiDpp(context, accessPoint.getSecurity());
    }

    /**
     * Checks if QR code generator supports to config other devices with the Wi-Fi network
     *
     * @param context The context to use for {@code WifiManager}
     * @param accessPoint The {@link AccessPoint} of the Wi-Fi network
     */
    public static boolean isSupportConfiguratorQrCodeGenerator(Context context,
            AccessPoint accessPoint) {
        return isSupportZxing(context, accessPoint.getSecurity());
    }

    /**
     * Checks if this device supports to be configured by the Wi-Fi network of the security
     *
     * @param context The context to use for {@code WifiManager}
     * @param accesspointSecurity The security constants defined in {@link AccessPoint}
     */
    public static boolean isSupportEnrolleeQrCodeScanner(Context context,
            int accesspointSecurity) {
        return isSupportWifiDpp(context, accesspointSecurity) ||
                isSupportZxing(context, accesspointSecurity);
    }

    private static boolean isSupportHotspotConfiguratorQrCodeGenerator(
            SoftApConfiguration softApConfiguration) {
        // QR code generator produces QR code with ZXing's Wi-Fi network config format,
        // it supports PSK and WEP and non security
        // KeyMgmt.NONE is for WEP or non security
        int securityType = softApConfiguration.getSecurityType();
        return securityType == SoftApConfiguration.SECURITY_TYPE_WPA2_PSK
                || securityType == SoftApConfiguration.SECURITY_TYPE_OPEN;
    }

    private static boolean isSupportWifiDpp(Context context, int accesspointSecurity) {
        if (!isWifiDppEnabled(context)) {
            return false;
        }

        // DPP 1.0 only supports SAE and PSK.
        final WifiManager wifiManager = context.getSystemService(WifiManager.class);
        switch (accesspointSecurity) {
            case AccessPoint.SECURITY_SAE:
                if (wifiManager.isWpa3SaeSupported()) {
                    return true;
                }
                break;
            case AccessPoint.SECURITY_PSK:
                return true;
            default:
        }
        return false;
    }

    private static boolean isSupportZxing(Context context, int accesspointSecurity) {
        final WifiManager wifiManager = context.getSystemService(WifiManager.class);
        switch (accesspointSecurity) {
            case AccessPoint.SECURITY_PSK:
            case AccessPoint.SECURITY_WEP:
            case AccessPoint.SECURITY_NONE:
                return true;
            case AccessPoint.SECURITY_SAE:
                if (wifiManager.isWpa3SaeSupported()) {
                    return true;
                }
                break;
            case AccessPoint.SECURITY_OWE:
                if (wifiManager.isEnhancedOpenSupported()) {
                    return true;
                }
                break;
            default:
        }
        return false;
    }

    static void triggerVibrationForQrCodeRecognition(Context context) {
        Vibrator vibrator = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
        if (vibrator == null) {
          return;
        }
        vibrator.vibrate(VibrationEffect.createOneShot(
                VIBRATE_DURATION_QR_CODE_RECOGNITION.toMillis(),
                VibrationEffect.DEFAULT_AMPLITUDE));
    }
}
