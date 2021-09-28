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

package com.googlecode.android_scripting.facade.wifi;

import static android.net.NetworkCapabilities.TRANSPORT_WIFI;

import static com.googlecode.android_scripting.jsonrpc.JsonBuilder.build;

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.DhcpInfo;
import android.net.MacAddress;
import android.net.Network;
import android.net.NetworkInfo;
import android.net.NetworkInfo.DetailedState;
import android.net.NetworkRequest;
import android.net.NetworkSpecifier;
import android.net.Uri;
import android.net.wifi.EasyConnectStatusCallback;
import android.net.wifi.ScanResult;
import android.net.wifi.SoftApCapability;
import android.net.wifi.SoftApConfiguration;
import android.net.wifi.SoftApInfo;
import android.net.wifi.WifiClient;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiConfiguration.AuthAlgorithm;
import android.net.wifi.WifiConfiguration.KeyMgmt;
import android.net.wifi.WifiEnterpriseConfig;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.NetworkRequestMatchCallback;
import android.net.wifi.WifiManager.NetworkRequestUserSelectionCallback;
import android.net.wifi.WifiManager.WifiLock;
import android.net.wifi.WifiNetworkSpecifier;
import android.net.wifi.WifiNetworkSuggestion;
import android.net.wifi.WpsInfo;
import android.net.wifi.hotspot2.ConfigParser;
import android.net.wifi.hotspot2.OsuProvider;
import android.net.wifi.hotspot2.PasspointConfiguration;
import android.net.wifi.hotspot2.ProvisioningCallback;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerExecutor;
import android.os.HandlerThread;
import android.os.PatternMatcher;
import android.os.connectivity.WifiActivityEnergyInfo;
import android.provider.Settings.SettingNotFoundException;
import android.text.TextUtils;
import android.util.Base64;
import android.util.SparseArray;

import com.android.internal.annotations.GuardedBy;

import com.googlecode.android_scripting.Log;
import com.googlecode.android_scripting.facade.EventFacade;
import com.googlecode.android_scripting.facade.FacadeManager;
import com.googlecode.android_scripting.jsonrpc.RpcReceiver;
import com.googlecode.android_scripting.rpc.Rpc;
import com.googlecode.android_scripting.rpc.RpcOptional;
import com.googlecode.android_scripting.rpc.RpcParameter;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectOutput;
import java.io.ObjectOutputStream;
import java.security.GeneralSecurityException;
import java.security.KeyFactory;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

/**
 * WifiManager functions.
 */
// TODO: make methods handle various wifi states properly
// e.g. wifi connection result will be null when flight mode is on
public class WifiManagerFacade extends RpcReceiver {
    private static final String mEventType = "WifiManager";
    // MIME type for passpoint config.
    private static final String TYPE_WIFICONFIG = "application/x-wifi-config";
    private static final int TIMEOUT_MILLIS = 5000;

    private final Service mService;
    private final WifiManager mWifi;
    private final ConnectivityManager mCm;
    private final EventFacade mEventFacade;

    private final IntentFilter mScanFilter;
    private final IntentFilter mStateChangeFilter;
    private final IntentFilter mTetherFilter;
    private final IntentFilter mNetworkSuggestionStateChangeFilter;
    private final WifiScanReceiver mScanResultsAvailableReceiver;
    private final WifiScanResultsReceiver mWifiScanResultsReceiver;
    private final WifiStateChangeReceiver mStateChangeReceiver;
    private final WifiNetworkSuggestionStateChangeReceiver mNetworkSuggestionStateChangeReceiver;
    private final HandlerThread mCallbackHandlerThread;
    private final Object mCallbackLock = new Object();
    private final Map<NetworkSpecifier, NetworkCallback> mNetworkCallbacks = new HashMap<>();
    private boolean mTrackingWifiStateChange;
    private boolean mTrackingTetherStateChange;
    private boolean mTrackingNetworkSuggestionStateChange;
    @GuardedBy("mCallbackLock")
    private NetworkRequestUserSelectionCallback mNetworkRequestUserSelectionCallback;
    private final SparseArray<SoftApCallbackImp> mSoftapCallbacks;

    private final BroadcastReceiver mTetherStateReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (WifiManager.WIFI_AP_STATE_CHANGED_ACTION.equals(action)) {
                Log.d("Wifi AP state changed.");
                int state = intent.getIntExtra(WifiManager.EXTRA_WIFI_AP_STATE,
                        WifiManager.WIFI_AP_STATE_FAILED);
                if (state == WifiManager.WIFI_AP_STATE_ENABLED) {
                    mEventFacade.postEvent("WifiManagerApEnabled", null);
                } else if (state == WifiManager.WIFI_AP_STATE_DISABLED) {
                    mEventFacade.postEvent("WifiManagerApDisabled", null);
                }
            } else if (ConnectivityManager.ACTION_TETHER_STATE_CHANGED.equals(action)) {
                Log.d("Tether state changed.");
                ArrayList<String> available = intent.getStringArrayListExtra(
                        ConnectivityManager.EXTRA_AVAILABLE_TETHER);
                ArrayList<String> active = intent.getStringArrayListExtra(
                        ConnectivityManager.EXTRA_ACTIVE_TETHER);
                ArrayList<String> errored = intent.getStringArrayListExtra(
                        ConnectivityManager.EXTRA_ERRORED_TETHER);
                Bundle msg = new Bundle();
                msg.putStringArrayList("AVAILABLE_TETHER", available);
                msg.putStringArrayList("ACTIVE_TETHER", active);
                msg.putStringArrayList("ERRORED_TETHER", errored);
                mEventFacade.postEvent("TetherStateChanged", msg);
            }
        }
    };

    private final NetworkRequestMatchCallback mNetworkRequestMatchCallback =
            new NetworkRequestMatchCallback() {
                private static final String EVENT_TAG = mEventType + "NetworkRequestMatchCallback";

                @Override
                public void onUserSelectionCallbackRegistration(
                        NetworkRequestUserSelectionCallback userSelectionCallback) {
                    synchronized (mCallbackLock) {
                        mNetworkRequestUserSelectionCallback = userSelectionCallback;
                    }
                }

                @Override
                public void onAbort() {
                    mEventFacade.postEvent(EVENT_TAG + "OnAbort", null);
                }

                @Override
                public void onMatch(List<ScanResult> scanResults) {
                    mEventFacade.postEvent(EVENT_TAG + "OnMatch", scanResults);
                }

                @Override
                public void onUserSelectionConnectSuccess(WifiConfiguration wifiConfiguration) {
                    mEventFacade.postEvent(EVENT_TAG + "OnUserSelectionConnectSuccess",
                            wifiConfiguration);
                }

                @Override
                public void onUserSelectionConnectFailure(WifiConfiguration wifiConfiguration) {
                    mEventFacade.postEvent(EVENT_TAG + "OnUserSelectionConnectFailure",
                            wifiConfiguration);
                }
            };

    private final class NetworkCallbackImpl extends NetworkCallback {
        private static final String EVENT_TAG = mEventType + "NetworkCallback";

        @Override
        public void onAvailable(Network network) {
            mEventFacade.postEvent(EVENT_TAG + "OnAvailable", mWifi.getConnectionInfo());
        }

        @Override
        public void onUnavailable() {
            mEventFacade.postEvent(EVENT_TAG + "OnUnavailable", null);
        }

        @Override
        public void onLost(Network network) {
            mEventFacade.postEvent(EVENT_TAG + "OnLost", null);
        }
    };

    private static class SoftApCallbackImp implements WifiManager.SoftApCallback {
        // A monotonic increasing counter for softap callback ids.
        private static int sCount = 0;

        private final int mId;
        private final EventFacade mEventFacade;
        private final String mEventStr;

        SoftApCallbackImp(EventFacade eventFacade) {
            sCount++;
            mId = sCount;
            mEventFacade = eventFacade;
            mEventStr = mEventType + "SoftApCallback-" + mId + "-";
        }

        @Override
        public void onStateChanged(int state, int failureReason) {
            Bundle msg = new Bundle();
            msg.putInt("State", state);
            msg.putInt("FailureReason", failureReason);
            mEventFacade.postEvent(mEventStr + "OnStateChanged", msg);
        }

        @Override
        public void onConnectedClientsChanged(List<WifiClient> clients) {
            ArrayList<String> macAddresses = new ArrayList<>();
            clients.forEach(x -> macAddresses.add(x.getMacAddress().toString()));
            Bundle msg = new Bundle();
            msg.putInt("NumClients", clients.size());
            msg.putStringArrayList("MacAddresses", macAddresses);
            mEventFacade.postEvent(mEventStr + "OnNumClientsChanged", msg);
            mEventFacade.postEvent(mEventStr + "OnConnectedClientsChanged", clients);
        }

        @Override
        public void onInfoChanged(SoftApInfo softApInfo) {
            mEventFacade.postEvent(mEventStr + "OnInfoChanged", softApInfo);
        }

        @Override
        public void onCapabilityChanged(SoftApCapability softApCapability) {
            mEventFacade.postEvent(mEventStr + "OnCapabilityChanged", softApCapability);
        }

        @Override
        public void onBlockedClientConnecting(WifiClient client, int blockedReason) {
            Bundle msg = new Bundle();
            msg.putString("WifiClient", client.getMacAddress().toString());
            msg.putInt("BlockedReason", blockedReason);
            mEventFacade.postEvent(mEventStr + "OnBlockedClientConnecting", msg);
        }
    };

    private WifiLock mLock = null;
    private boolean mIsConnected = false;

    public WifiManagerFacade(FacadeManager manager) {
        super(manager);
        mService = manager.getService();
        mWifi = (WifiManager) mService.getSystemService(Context.WIFI_SERVICE);
        mCm = (ConnectivityManager) mService.getSystemService(Context.CONNECTIVITY_SERVICE);
        mEventFacade = manager.getReceiver(EventFacade.class);
        mCallbackHandlerThread = new HandlerThread("WifiManagerFacade");

        mScanFilter = new IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION);
        mStateChangeFilter = new IntentFilter(WifiManager.NETWORK_STATE_CHANGED_ACTION);
        mStateChangeFilter.addAction(WifiManager.SUPPLICANT_STATE_CHANGED_ACTION);
        mStateChangeFilter.addAction(WifiManager.SUPPLICANT_CONNECTION_CHANGE_ACTION);
        mStateChangeFilter.addAction(WifiManager.WIFI_STATE_CHANGED_ACTION);
        mStateChangeFilter.setPriority(IntentFilter.SYSTEM_HIGH_PRIORITY - 1);

        mTetherFilter = new IntentFilter(WifiManager.WIFI_AP_STATE_CHANGED_ACTION);
        mTetherFilter.addAction(ConnectivityManager.ACTION_TETHER_STATE_CHANGED);

        mNetworkSuggestionStateChangeFilter = new IntentFilter(
                WifiManager.ACTION_WIFI_NETWORK_SUGGESTION_POST_CONNECTION);

        mScanResultsAvailableReceiver = new WifiScanReceiver(mEventFacade);
        mWifiScanResultsReceiver = new WifiScanResultsReceiver(mEventFacade);
        mStateChangeReceiver = new WifiStateChangeReceiver();
        mNetworkSuggestionStateChangeReceiver = new WifiNetworkSuggestionStateChangeReceiver();
        mTrackingWifiStateChange = false;
        mTrackingTetherStateChange = false;
        mTrackingNetworkSuggestionStateChange = false;
        mCallbackHandlerThread.start();
        mSoftapCallbacks = new SparseArray<>();
    }

    private void makeLock(int wifiMode) {
        if (mLock == null) {
            mLock = mWifi.createWifiLock(wifiMode, "sl4a");
            mLock.acquire();
        }
    }

    /**
     * Handle Broadcast receiver for Scan Result
     *
     * @parm eventFacade Object of EventFacade
     */
    class WifiScanReceiver extends BroadcastReceiver {
        private final EventFacade mEventFacade;

        WifiScanReceiver(EventFacade eventFacade) {
            mEventFacade = eventFacade;
        }

        @Override
        public void onReceive(Context c, Intent intent) {
            String action = intent.getAction();
            if (action.equals(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION)) {
                if (!intent.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false)) {
                    Log.w("Wifi connection scan failed, ignoring.");
                    mEventFacade.postEvent(mEventType + "ScanFailure", null);
                } else {
                    Bundle mResults = new Bundle();
                    Log.d("Wifi connection scan finished, results available.");
                    mResults.putLong("Timestamp", System.currentTimeMillis() / 1000);
                    mEventFacade.postEvent(mEventType + "ScanResultsAvailable", mResults);
                }
                mService.unregisterReceiver(mScanResultsAvailableReceiver);
            }
        }
    }

    class WifiScanResultsReceiver extends WifiManager.ScanResultsCallback {
        private final EventFacade mEventFacade;

        WifiScanResultsReceiver(EventFacade eventFacade) {
            mEventFacade = eventFacade;
        }
        @Override
        public void onScanResultsAvailable() {
            Bundle mResults = new Bundle();
            Log.d("Wifi connection scan finished, results available.");
            mResults.putLong("Timestamp", System.currentTimeMillis() / 1000);
            mEventFacade.postEvent(mEventType + "ScanResultsCallbackOnSuccess", mResults);
            mWifi.unregisterScanResultsCallback(mWifiScanResultsReceiver);
        }
    }

    class WifiActionListener implements WifiManager.ActionListener {
        private final EventFacade mEventFacade;
        private final String TAG;

        public WifiActionListener(EventFacade eventFacade, String tag) {
            mEventFacade = eventFacade;
            this.TAG = tag;
        }

        @Override
        public void onSuccess() {
            Log.d("WifiActionListener  onSuccess called for " + mEventType + TAG + "OnSuccess");
            mEventFacade.postEvent(mEventType + TAG + "OnSuccess", null);
        }

        @Override
        public void onFailure(int reason) {
            Log.d("WifiActionListener  onFailure called for" + mEventType);
            Bundle msg = new Bundle();
            msg.putInt("reason", reason);
            mEventFacade.postEvent(mEventType + TAG + "OnFailure", msg);
        }
    }

    public class WifiStateChangeReceiver extends BroadcastReceiver {
        String mCachedWifiInfo = "";

        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action.equals(WifiManager.NETWORK_STATE_CHANGED_ACTION)) {
                Log.d("Wifi network state changed.");
                NetworkInfo nInfo = intent.getParcelableExtra(WifiManager.EXTRA_NETWORK_INFO);
                Log.d("NetworkInfo " + nInfo);
                // If network info is of type wifi, send wifi events.
                if (nInfo.getType() == ConnectivityManager.TYPE_WIFI) {
                    if (nInfo.getDetailedState().equals(DetailedState.CONNECTED)) {
                        WifiInfo wInfo = mWifi.getConnectionInfo();
                        String bssid = wInfo.getBSSID();
                        if (bssid != null && !mCachedWifiInfo.equals(wInfo.toString())) {
                            Log.d("WifiNetworkConnected");
                            mEventFacade.postEvent("WifiNetworkConnected", wInfo);
                        }
                        mCachedWifiInfo = wInfo.toString();
                    } else {
                        if (nInfo.getDetailedState().equals(DetailedState.DISCONNECTED)) {
                            if (!mCachedWifiInfo.equals("")) {
                                mCachedWifiInfo = "";
                                mEventFacade.postEvent("WifiNetworkDisconnected", null);
                            }
                        }
                    }
                }
            } else if (action.equals(WifiManager.SUPPLICANT_CONNECTION_CHANGE_ACTION)) {
                Log.d("Supplicant connection state changed.");
                mIsConnected = intent
                        .getBooleanExtra(WifiManager.EXTRA_SUPPLICANT_CONNECTED, false);
                Bundle msg = new Bundle();
                msg.putBoolean("Connected", mIsConnected);
                mEventFacade.postEvent("SupplicantConnectionChanged", msg);
            } else if (action.equals(WifiManager.WIFI_STATE_CHANGED_ACTION)) {
                int state = intent.getIntExtra(
                        WifiManager.EXTRA_WIFI_STATE, WifiManager.WIFI_STATE_DISABLED);
                Log.d("Wifi state changed to " + state);
                boolean enabled;
                if (state == WifiManager.WIFI_STATE_DISABLED) {
                    enabled = false;
                } else if (state == WifiManager.WIFI_STATE_ENABLED) {
                    enabled = true;
                } else {
                    // we only care about enabled/disabled.
                    Log.v("Ignoring intermediate wifi state change event...");
                    return;
                }
                Bundle msg = new Bundle();
                msg.putBoolean("enabled", enabled);
                mEventFacade.postEvent("WifiStateChanged", msg);
            }
        }
    }

    public class WifiNetworkSuggestionStateChangeReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action.equals(WifiManager.ACTION_WIFI_NETWORK_SUGGESTION_POST_CONNECTION)) {
                WifiNetworkSuggestion networkSuggestion =
                        intent.getParcelableExtra(WifiManager.EXTRA_NETWORK_SUGGESTION);
                mEventFacade.postEvent(
                        "WifiNetworkSuggestionPostConnection",
                        networkSuggestion.wifiConfiguration.SSID);
            }
        }
    }

    public class WifiWpsCallback extends WifiManager.WpsCallback {
        private static final String tag = "WifiWps";

        @Override
        public void onStarted(String pin) {
            Bundle msg = new Bundle();
            msg.putString("pin", pin);
            mEventFacade.postEvent(tag + "OnStarted", msg);
        }

        @Override
        public void onSucceeded() {
            Log.d("Wps op succeeded");
            mEventFacade.postEvent(tag + "OnSucceeded", null);
        }

        @Override
        public void onFailed(int reason) {
            Bundle msg = new Bundle();
            msg.putInt("reason", reason);
            mEventFacade.postEvent(tag + "OnFailed", msg);
        }
    }

    private void applyingkeyMgmt(WifiConfiguration config, ScanResult result) {
        if (result.capabilities.contains("WEP")) {
            config.allowedKeyManagement.set(KeyMgmt.NONE);
            config.allowedAuthAlgorithms.set(AuthAlgorithm.OPEN);
            config.allowedAuthAlgorithms.set(AuthAlgorithm.SHARED);
        } else if (result.capabilities.contains("PSK")) {
            config.allowedKeyManagement.set(KeyMgmt.WPA_PSK);
        } else if (result.capabilities.contains("EAP")) {
            // this is probably wrong, as we don't have a way to enter the enterprise config
            config.allowedKeyManagement.set(KeyMgmt.WPA_EAP);
            config.allowedKeyManagement.set(KeyMgmt.IEEE8021X);
        } else {
            config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
        }
    }

    private WifiConfiguration genWifiConfig(JSONObject j) throws JSONException {
        if (j == null) {
            return null;
        }
        WifiConfiguration config = new WifiConfiguration();
        if (j.has("SSID")) {
            config.SSID = "\"" + j.getString("SSID") + "\"";
        } else if (j.has("ssid")) {
            config.SSID = "\"" + j.getString("ssid") + "\"";
        }
        if (j.has("password")) {
            String security;

            // Check if new security type SAE (WPA3) is present. Default to PSK
            if (j.has("security")) {
                if (TextUtils.equals(j.getString("security"), "SAE")) {
                    config.allowedKeyManagement.set(KeyMgmt.SAE);
                    config.requirePmf = true;
                } else {
                    config.allowedKeyManagement.set(KeyMgmt.WPA_PSK);
                }
            } else {
                config.allowedKeyManagement.set(KeyMgmt.WPA_PSK);
            }
            config.preSharedKey = "\"" + j.getString("password") + "\"";
        } else if (j.has("preSharedKey")) {
            config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_PSK);
            config.preSharedKey = j.getString("preSharedKey");
        } else {
            if (j.has("security")) {
                if (TextUtils.equals(j.getString("security"), "OWE")) {
                    config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.OWE);
                    config.requirePmf = true;
                } else {
                    config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
                }
            } else {
                config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
            }
        }
        if (j.has("BSSID")) {
            config.BSSID = j.getString("BSSID");
        }
        if (j.has("hiddenSSID")) {
            config.hiddenSSID = j.getBoolean("hiddenSSID");
        }
        if (j.has("priority")) {
            config.priority = j.getInt("priority");
        }
        if (j.has("apBand")) {
            config.apBand = j.getInt("apBand");
        }
        if (j.has("wepKeys")) {
            // Looks like we only support static WEP.
            config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
            config.allowedAuthAlgorithms.set(AuthAlgorithm.OPEN);
            config.allowedAuthAlgorithms.set(AuthAlgorithm.SHARED);
            JSONArray keys = j.getJSONArray("wepKeys");
            String[] wepKeys = new String[keys.length()];
            for (int i = 0; i < keys.length(); i++) {
                wepKeys[i] = keys.getString(i);
            }
            config.wepKeys = wepKeys;
        }
        if (j.has("wepTxKeyIndex")) {
            config.wepTxKeyIndex = j.getInt("wepTxKeyIndex");
        }
        if (j.has("meteredOverride")) {
            config.meteredOverride = j.getInt("meteredOverride");
        }
        if (j.has("macRand")) {
            config.macRandomizationSetting = j.getInt("macRand");
        }
        if (j.has("carrierId")) {
            config.carrierId = j.getInt("carrierId");
        }
        return config;
    }

    private WifiEnterpriseConfig genWifiEnterpriseConfig(JSONObject j) throws JSONException,
            GeneralSecurityException {
        WifiEnterpriseConfig eConfig = new WifiEnterpriseConfig();
        if (j.has(WifiEnterpriseConfig.EAP_KEY)) {
            int eap = j.getInt(WifiEnterpriseConfig.EAP_KEY);
            eConfig.setEapMethod(eap);
        }
        if (j.has(WifiEnterpriseConfig.PHASE2_KEY)) {
            int p2Method = j.getInt(WifiEnterpriseConfig.PHASE2_KEY);
            eConfig.setPhase2Method(p2Method);
        }
        if (j.has(WifiEnterpriseConfig.CA_CERT_KEY)) {
            String certStr = j.getString(WifiEnterpriseConfig.CA_CERT_KEY);
            Log.v("CA Cert String is " + certStr);
            eConfig.setCaCertificate(strToX509Cert(certStr));
        }
        if (j.has(WifiEnterpriseConfig.CLIENT_CERT_KEY)
                && j.has(WifiEnterpriseConfig.PRIVATE_KEY_ID_KEY)) {
            String certStr = j.getString(WifiEnterpriseConfig.CLIENT_CERT_KEY);
            String keyStr = j.getString(WifiEnterpriseConfig.PRIVATE_KEY_ID_KEY);
            Log.v("Client Cert String is " + certStr);
            Log.v("Client Key String is " + keyStr);
            X509Certificate cert = strToX509Cert(certStr);
            PrivateKey privKey = strToPrivateKey(keyStr);
            Log.v("Cert is " + cert);
            Log.v("Private Key is " + privKey);
            eConfig.setClientKeyEntry(privKey, cert);
        }
        if (j.has(WifiEnterpriseConfig.IDENTITY_KEY)) {
            String identity = j.getString(WifiEnterpriseConfig.IDENTITY_KEY);
            Log.v("Setting identity to " + identity);
            eConfig.setIdentity(identity);
        }
        if (j.has(WifiEnterpriseConfig.PASSWORD_KEY)) {
            String pwd = j.getString(WifiEnterpriseConfig.PASSWORD_KEY);
            Log.v("Setting password to " + pwd);
            eConfig.setPassword(pwd);
        }
        if (j.has(WifiEnterpriseConfig.ALTSUBJECT_MATCH_KEY)) {
            String altSub = j.getString(WifiEnterpriseConfig.ALTSUBJECT_MATCH_KEY);
            Log.v("Setting Alt Subject to " + altSub);
            eConfig.setAltSubjectMatch(altSub);
        }
        if (j.has(WifiEnterpriseConfig.DOM_SUFFIX_MATCH_KEY)) {
            String domSuffix = j.getString(WifiEnterpriseConfig.DOM_SUFFIX_MATCH_KEY);
            Log.v("Setting Domain Suffix Match to " + domSuffix);
            eConfig.setDomainSuffixMatch(domSuffix);
        }
        if (j.has(WifiEnterpriseConfig.REALM_KEY)) {
            String realm = j.getString(WifiEnterpriseConfig.REALM_KEY);
            Log.v("Setting Domain Suffix Match to " + realm);
            eConfig.setRealm(realm);
        }
        if (j.has(WifiEnterpriseConfig.OCSP)) {
            int ocsp = j.getInt(WifiEnterpriseConfig.OCSP);
            Log.v("Setting OCSP to " + ocsp);
            eConfig.setOcsp(ocsp);
        }
        return eConfig;
    }

    private WifiConfiguration genWifiConfigWithEnterpriseConfig(JSONObject j) throws JSONException,
            GeneralSecurityException {
        if (j == null) {
            return null;
        }
        WifiConfiguration config = new WifiConfiguration();
        config.allowedKeyManagement.set(KeyMgmt.WPA_EAP);
        config.allowedKeyManagement.set(KeyMgmt.IEEE8021X);

        if (j.has("security")) {
            if (TextUtils.equals(j.getString("security"), "SUITE_B_192")) {
                config.allowedKeyManagement.set(KeyMgmt.SUITE_B_192);
                config.requirePmf = true;
            }
        }

        if (j.has("SSID")) {
            config.SSID = "\"" + j.getString("SSID") + "\"";
        } else if (j.has("ssid")) {
            config.SSID = "\"" + j.getString("ssid") + "\"";
        }
        if (j.has("FQDN")) {
            config.FQDN = j.getString("FQDN");
        }
        if (j.has("providerFriendlyName")) {
            config.providerFriendlyName = j.getString("providerFriendlyName");
        }
        if (j.has("roamingConsortiumIds")) {
            JSONArray ids = j.getJSONArray("roamingConsortiumIds");
            long[] rIds = new long[ids.length()];
            for (int i = 0; i < ids.length(); i++) {
                rIds[i] = ids.getLong(i);
            }
            config.roamingConsortiumIds = rIds;
        }
        if (j.has("carrierId")) {
            config.carrierId = j.getInt("carrierId");
        }
        config.enterpriseConfig = genWifiEnterpriseConfig(j);
        return config;
    }

    private NetworkSpecifier genWifiNetworkSpecifier(JSONObject j) throws JSONException,
            GeneralSecurityException {
        if (j == null) {
            return null;
        }
        WifiNetworkSpecifier.Builder builder = new WifiNetworkSpecifier.Builder();
        if (j.has("SSID")) {
            builder = builder.setSsid(j.getString("SSID"));
        } else if (j.has("ssidPattern")) {
            builder = builder.setSsidPattern(
                    new PatternMatcher(j.getString("ssidPattern"),
                            PatternMatcher.PATTERN_ADVANCED_GLOB));
        }
        if (j.has("BSSID")) {
            builder = builder.setBssid(MacAddress.fromString(j.getString("BSSID")));
        } else if (j.has("bssidPattern")) {
            builder = builder.setBssidPattern(
                    MacAddress.fromString(j.getJSONArray("bssidPattern").getString(0)),
                    MacAddress.fromString(j.getJSONArray("bssidPattern").getString(1)));
        }
        if (j.has("hiddenSSID")) {
            builder = builder.setIsHiddenSsid(j.getBoolean("hiddenSSID"));
        }
        if (j.has("isEnhancedOpen")) {
            builder = builder.setIsEnhancedOpen(j.getBoolean("isEnhancedOpen"));
        }
        boolean isWpa3 = false;
        if (j.has("isWpa3") && j.getBoolean("isWpa3")) {
            isWpa3 = true;
        }
        if (j.has("password") && !j.has(WifiEnterpriseConfig.EAP_KEY)) {
            if (!isWpa3) {
                builder = builder.setWpa2Passphrase(j.getString("password"));
            } else {
                builder = builder.setWpa3Passphrase(j.getString("password"));
            }
        }
        if (j.has(WifiEnterpriseConfig.EAP_KEY)) {
            if (!isWpa3) {
                builder = builder.setWpa2EnterpriseConfig(genWifiEnterpriseConfig(j));
            } else {
                builder = builder.setWpa3EnterpriseConfig(genWifiEnterpriseConfig(j));
            }
        }
        return builder.build();
    }

    private WifiNetworkSuggestion genWifiNetworkSuggestion(JSONObject j) throws JSONException,
            GeneralSecurityException, IOException {
        if (j == null) {
            return null;
        }
        WifiNetworkSuggestion.Builder builder = new WifiNetworkSuggestion.Builder();
        if (j.has("isAppInteractionRequired")) {
            builder = builder.setIsAppInteractionRequired(j.getBoolean("isAppInteractionRequired"));
        }
        if (j.has("isUserInteractionRequired")) {
            builder = builder.setIsUserInteractionRequired(
                    j.getBoolean("isUserInteractionRequired"));
        }
        if (j.has("isMetered")) {
            builder = builder.setIsMetered(j.getBoolean("isMetered"));
        }
        if (j.has("priority")) {
            builder = builder.setPriority(j.getInt("priority"));
        }
        if (j.has("carrierId")) {
            builder.setCarrierId(j.getInt("carrierId"));
        }
        if (j.has("enableAutojoin")) {
            builder.setIsInitialAutojoinEnabled(j.getBoolean("enableAutojoin"));
        }
        if (j.has("untrusted")) {
            builder.setUntrusted(j.getBoolean("untrusted"));
        }
        if (j.has("profile")) {
            builder = builder.setPasspointConfig(genWifiPasspointConfig(j));
        } else {
            if (j.has("SSID")) {
                builder = builder.setSsid(j.getString("SSID"));
            }
            if (j.has("BSSID")) {
                builder = builder.setBssid(MacAddress.fromString(j.getString("BSSID")));
            }
            if (j.has("hiddenSSID")) {
                builder = builder.setIsHiddenSsid(j.getBoolean("hiddenSSID"));
            }
            if (j.has("isEnhancedOpen")) {
                builder = builder.setIsEnhancedOpen(j.getBoolean("isEnhancedOpen"));
            }
            boolean isWpa3 = false;
            if (j.has("isWpa3") && j.getBoolean("isWpa3")) {
                isWpa3 = true;
            }
            if (j.has("password") && !j.has(WifiEnterpriseConfig.EAP_KEY)) {
                if (!isWpa3) {
                    builder = builder.setWpa2Passphrase(j.getString("password"));
                } else {
                    builder = builder.setWpa3Passphrase(j.getString("password"));
                }
            }
            if (j.has(WifiEnterpriseConfig.EAP_KEY)) {
                if (!isWpa3) {
                    builder = builder.setWpa2EnterpriseConfig(genWifiEnterpriseConfig(j));
                } else {
                    builder = builder.setWpa3EnterpriseConfig(genWifiEnterpriseConfig(j));
                }
            }
        }

        return builder.build();
    }

    private List<WifiNetworkSuggestion> genWifiNetworkSuggestions(
            JSONArray jsonNetworkSuggestionsArray) throws JSONException, GeneralSecurityException,
            IOException {
        if (jsonNetworkSuggestionsArray == null) {
            return null;
        }
        List<WifiNetworkSuggestion> networkSuggestions = new ArrayList<>();
        for (int i = 0; i < jsonNetworkSuggestionsArray.length(); i++) {
            networkSuggestions.add(
                    genWifiNetworkSuggestion(jsonNetworkSuggestionsArray.getJSONObject(i)));
        }
        return networkSuggestions;
    }

    private boolean matchScanResult(ScanResult result, String id) {
        if (result.BSSID.equals(id) || result.SSID.equals(id)) {
            return true;
        }
        return false;
    }

    private WpsInfo parseWpsInfo(String infoStr) throws JSONException {
        if (infoStr == null) {
            return null;
        }
        JSONObject j = new JSONObject(infoStr);
        WpsInfo info = new WpsInfo();
        if (j.has("setup")) {
            info.setup = j.getInt("setup");
        }
        if (j.has("BSSID")) {
            info.BSSID = j.getString("BSSID");
        }
        if (j.has("pin")) {
            info.pin = j.getString("pin");
        }
        return info;
    }

    private byte[] base64StrToBytes(String input) {
        return Base64.decode(input, Base64.DEFAULT);
    }

    private X509Certificate strToX509Cert(String certStr) throws CertificateException {
        byte[] certBytes = base64StrToBytes(certStr);
        InputStream certStream = new ByteArrayInputStream(certBytes);
        CertificateFactory cf = CertificateFactory.getInstance("X509");
        return (X509Certificate) cf.generateCertificate(certStream);
    }

    private PrivateKey strToPrivateKey(String key) throws NoSuchAlgorithmException,
            InvalidKeySpecException {
        byte[] keyBytes = base64StrToBytes(key);
        PKCS8EncodedKeySpec keySpec = new PKCS8EncodedKeySpec(keyBytes);
        KeyFactory fact = KeyFactory.getInstance("RSA");
        PrivateKey priv = fact.generatePrivate(keySpec);
        return priv;
    }

    private PublicKey strToPublicKey(String key) throws NoSuchAlgorithmException,
            InvalidKeySpecException {
        byte[] keyBytes = base64StrToBytes(key);
        X509EncodedKeySpec keySpec = new X509EncodedKeySpec(keyBytes);
        KeyFactory fact = KeyFactory.getInstance("RSA");
        PublicKey pub = fact.generatePublic(keySpec);
        return pub;
    }

    private WifiConfiguration wifiConfigurationFromScanResult(ScanResult result) {
        if (result == null)
            return null;
        WifiConfiguration config = new WifiConfiguration();
        config.SSID = "\"" + result.SSID + "\"";
        applyingkeyMgmt(config, result);
        config.BSSID = result.BSSID;
        return config;
    }

    @Rpc(description = "test.")
    public String wifiTest(
            @RpcParameter(name = "certString") String certString) throws CertificateException, IOException {
        // TODO(angli): Make this work. Convert a X509Certificate back to a string.
        X509Certificate caCert = strToX509Cert(certString);
        caCert.getEncoded();
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        ObjectOutput out = new ObjectOutputStream(bos);
        out.writeObject(caCert);
        byte[] data = bos.toByteArray();
        bos.close();
        return Base64.encodeToString(data, Base64.DEFAULT);
    }

    @Rpc(description = "Add a network.")
    @Deprecated
    public Integer wifiAddNetwork(@RpcParameter(name = "wifiConfig") JSONObject wifiConfig)
            throws JSONException {
        return wifiaddOrUpdateNetwork(wifiConfig);
    }

    @Rpc(description = "Add or update a network.")
    public Integer wifiaddOrUpdateNetwork(@RpcParameter(name = "wifiConfig") JSONObject wifiConfig)
            throws JSONException {
        return mWifi.addNetwork(genWifiConfig(wifiConfig));
    }

    @Rpc(description = "Cancel Wi-fi Protected Setup.")
    public void wifiCancelWps() throws JSONException {
        WifiWpsCallback listener = new WifiWpsCallback();
        mWifi.cancelWps(listener);
    }

    @Rpc(description = "Checks Wifi state.", returns = "True if Wifi is enabled.")
    public Boolean wifiCheckState() {
        return mWifi.getWifiState() == WifiManager.WIFI_STATE_ENABLED;
    }

    /**
    * @deprecated Use {@link #wifiConnectByConfig(config)} instead.
    */
    @Rpc(description = "Connects to the network with the given configuration")
    @Deprecated
    public Boolean wifiConnect(@RpcParameter(name = "config") JSONObject config)
            throws JSONException {
        try {
            wifiConnectByConfig(config);
        } catch (GeneralSecurityException e) {
            String msg = "Caught GeneralSecurityException with the provided"
                         + "configuration";
            throw new RuntimeException(msg);
        }
        return true;
    }

    /**
    * @deprecated Use {@link #wifiConnectByConfig(config)} instead.
    */
    @Rpc(description = "Connects to the network with the given configuration")
    @Deprecated
    public Boolean wifiEnterpriseConnect(@RpcParameter(name = "config")
            JSONObject config) throws JSONException, GeneralSecurityException {
        try {
            wifiConnectByConfig(config);
        } catch (GeneralSecurityException e) {
            throw e;
        }
        return true;
    }

    /**
     * Connects to a wifi network using configuration.
     * @param config JSONObject Dictionary of wifi connection parameters
     * @throws JSONException
     * @throws GeneralSecurityException
     */
    @Rpc(description = "Connects to the network with the given configuration")
    public void wifiConnectByConfig(@RpcParameter(name = "config") JSONObject config)
            throws JSONException, GeneralSecurityException {
        WifiConfiguration wifiConfig;
        WifiActionListener listener;
        // Check if this is 802.1x or 802.11x config.
        if (config.has(WifiEnterpriseConfig.EAP_KEY)) {
            wifiConfig = genWifiConfigWithEnterpriseConfig(config);
        } else {
            wifiConfig = genWifiConfig(config);
        }
        listener = new WifiActionListener(mEventFacade,
                WifiConstants.WIFI_CONNECT_BY_CONFIG_CALLBACK);
        mWifi.connect(wifiConfig, listener);
    }

    /**
    * Gets the Wi-Fi factory MAC addresses.
    * @return An array of String represnting Wi-Fi MAC addresses,
    *         Or an empty Srting if failed.
    */
    @Rpc(description = "Gets the Wi-Fi factory MAC addresses", returns = "An array of String, representing the MAC address")
    public String[] wifigetFactorymacAddresses(){
        return mWifi.getFactoryMacAddresses();
    }

    @Rpc(description = "Gets the randomized MAC address", returns = "A MAC address or null")
    public MacAddress wifigetRandomizedMacAddress(@RpcParameter(name = "config") JSONObject config)
            throws JSONException{
        List<WifiConfiguration> configList = mWifi.getConfiguredNetworks();
        for(WifiConfiguration WifiConfig : configList){
            String ssid = WifiConfig.SSID;
            ssid = ssid.replace("\"", "");
            if (ssid.equals(config.getString("SSID"))){
                return WifiConfig.getRandomizedMacAddress();
            }
        }
        Log.d("Did not find a matching object in wifiManager.");
        return null;
    }
    /**
     * Generate a Passpoint configuration from JSON config.
     * @param config JSON config containing base64 encoded Passpoint profile
     */
    @Rpc(description = "Generate Passpoint configuration", returns = "PasspointConfiguration object")
    public PasspointConfiguration genWifiPasspointConfig(@RpcParameter(
            name = "config") JSONObject config)
            throws JSONException,CertificateException, IOException {
        String profileStr = "";
        if (config == null) {
            return null;
        }
        if (config.has("profile")) {
            profileStr =  config.getString("profile");
        }
        return ConfigParser.parsePasspointConfig(TYPE_WIFICONFIG,
                profileStr.getBytes());
    }

    /**
     * Add or update a Passpoint configuration.
     * @param config base64 encoded message containing Passpoint profile
     * @throws JSONException
     */
    @Rpc(description = "Add or update a Passpoint configuration")
    public void addUpdatePasspointConfig(@RpcParameter(
            name = "config") JSONObject config)
            throws JSONException,CertificateException, IOException {
        PasspointConfiguration passpointConfig = genWifiPasspointConfig(config);
        mWifi.addOrUpdatePasspointConfiguration(passpointConfig);
    }

    /**
     * Remove a Passpoint configuration.
     * @param fqdn The FQDN of the passpoint configuration to be removed
     * @return true on success; false otherwise
     */
    @Rpc(description = "Remove a Passpoint configuration")
    public void removePasspointConfig(
            @RpcParameter(name = "fqdn") String fqdn) {
        mWifi.removePasspointConfiguration(fqdn);
    }

    /**
     * Get list of Passpoint configurations.
     * @return A list of FQDNs of the Passpoint configurations
     */
    @Rpc(description = "Return the list of installed Passpoint configurations", returns = "A list of Passpoint configurations")
    public List<String> getPasspointConfigs() {
        List<String> fqdnList = new ArrayList<String>();
        for(PasspointConfiguration passpoint :
            mWifi.getPasspointConfigurations()) {
            fqdnList.add(passpoint.getHomeSp().getFqdn());
        }
        return fqdnList;
    }

    private class ProvisioningCallbackFacade extends ProvisioningCallback {
        private final EventFacade mEventFacade;

        ProvisioningCallbackFacade(EventFacade eventFacade) {
            mEventFacade = eventFacade;
        }

        @Override
        public void onProvisioningFailure(int status) {
            Log.v("Provisioning Failure " + status);
            Bundle msg = new Bundle();
            msg.putString("tag", "failure");
            msg.putInt("reason", status);
            mEventFacade.postEvent("onProvisioningCallback", msg);
        }

        @Override
        public void onProvisioningStatus(int status) {
            Log.v("Provisioning status " + status);
            Bundle msg = new Bundle();
            msg.putString("tag", "status");
            msg.putInt("status", status);
            mEventFacade.postEvent("onProvisioningCallback", msg);
        }

        @Override
        public void onProvisioningComplete() {
            Log.v("Provisioning Complete");
            Bundle msg = new Bundle();
            msg.putString("tag", "success");
            mEventFacade.postEvent("onProvisioningCallback", msg);
        }
    }

    private OsuProvider buildTestOsuProvider(JSONObject config) {
        String osuServiceDescription = "Google Passpoint Test Service";
        List<Integer> osuMethodList =
                Arrays.asList(OsuProvider.METHOD_SOAP_XML_SPP);

        try {
            if (!config.has("osuSSID")) {
                Log.e("missing osuSSID from the config");
                return null;
            }
            String osuSsid = config.getString("osuSSID");

            if (!config.has("osuUri")) {
                Log.e("missing osuUri from the config");
                return null;
            }
            Uri osuServerUri = Uri.parse(config.getString("osuUri"));

            Log.v("OSU Server URI " + osuServerUri.toString());
            if (!config.has("osuFriendlyName")) {
                Log.e("missing osuFriendlyName from the config");
                return null;
            }
            String osuFriendlyName = config.getString("osuFriendlyName");

            if (config.has("description")) {
                osuServiceDescription = config.getString("description");
            }
            Map<String, String> osuFriendlyNames = new HashMap<>();
            osuFriendlyNames.put("eng", osuFriendlyName);
            return new OsuProvider(osuSsid, osuFriendlyNames, osuServiceDescription,
                    osuServerUri, null, osuMethodList);
        } catch (JSONException e) {
            Log.e("JSON Parsing error: " + e);
            return null;
        }
    }

    /**
     * Start subscription provisioning
     */
    @Rpc(description = "Starts subscription provisioning flow")
    public void startSubscriptionProvisioning(
            @RpcParameter(name = "configJson") JSONObject configJson) {
        ProvisioningCallback callback = new ProvisioningCallbackFacade(mEventFacade);
        mWifi.startSubscriptionProvisioning(buildTestOsuProvider(configJson),
                mService.getMainExecutor(), callback);
    }

    /**
     * Connects to a wifi network using networkId.
     * @param networkId the network identity for the network in the supplicant
     */
    @Rpc(description = "Connects to the network with the given networkId")
    public void wifiConnectByNetworkId(
            @RpcParameter(name = "networkId") Integer networkId) {
        WifiActionListener listener;
        listener = new WifiActionListener(mEventFacade,
                WifiConstants.WIFI_CONNECT_BY_NETID_CALLBACK);
        mWifi.connect(networkId, listener);
    }

    @Rpc(description = "Disconnects from the currently active access point.", returns = "True if the operation succeeded.")
    public Boolean wifiDisconnect() {
        return mWifi.disconnect();
    }

    @Rpc(description = "Enable/disable autojoin scan and switch network when connected.")
    public Boolean wifiSetEnableAutoJoinWhenAssociated(@RpcParameter(name = "enable") Boolean enable) {
        return mWifi.setEnableAutoJoinWhenAssociated(enable);
    }

    @Rpc(description = "Enable a configured network. Initiate a connection if disableOthers is true", returns = "True if the operation succeeded.")
    public Boolean wifiEnableNetwork(@RpcParameter(name = "netId") Integer netId,
            @RpcParameter(name = "disableOthers") Boolean disableOthers) {
        return mWifi.enableNetwork(netId, disableOthers);
    }

    @Rpc(description = "Enable WiFi verbose logging.")
    public void wifiEnableVerboseLogging(@RpcParameter(name = "level") Integer level) {
        mWifi.setVerboseLoggingEnabled(level > 0);
    }

    @Rpc(description = "Resets all WifiManager settings.")
    public void wifiFactoryReset() {
        mWifi.factoryReset();
    }

    /**
     * Forget a wifi network by networkId.
     *
     * @param networkId Id of wifi network
     */
    @Rpc(description = "Forget a wifi network by networkId")
    public void wifiForgetNetwork(@RpcParameter(name = "wifiSSID") Integer networkId) {
        WifiActionListener listener = new WifiActionListener(mEventFacade,
                WifiConstants.WIFI_FORGET_NETWORK_CALLBACK);
        mWifi.forget(networkId, listener);
    }

    /**
     * User disconnect network.
     *
     * @param ssid SSID of wifi network
     */
    @Rpc(description = "Disconnect a wifi network by SSID")
    public void wifiUserDisconnectNetwork(@RpcParameter(name = "ssid") String ssid) {
        mWifi.disableEphemeralNetwork("\"" + ssid + "\"");
        mWifi.disconnect();
    }

    /**
     * User disconnect passpoint network.
     *
     * @param fqdn FQDN of the passpoint network
     */
    @Rpc(description = "Disconnect a wifi network by FQDN")
    public void wifiUserDisconnectPasspointNetwork(@RpcParameter(name = "fqdn") String fqdn) {
        mWifi.disableEphemeralNetwork(fqdn);
        mWifi.disconnect();
    }

    /**
     * Get SoftAp Configuration with SoftApConfiguration.
     */
    @Rpc(description = "Gets the Wi-Fi AP Configuration.")
    public SoftApConfiguration wifiGetApConfiguration() {
        return mWifi.getSoftApConfiguration();
    }

    /**
     * Get SoftAp Configuration with WifiConfiguration.
     *
     * Used to test deprecated API to check backward compatible
     */
    @Rpc(description = "Gets the Wi-Fi AP Configuration with WifiConfiguration.")
    public WifiConfiguration wifiGetApConfigurationWithWifiConfiguration() {
        return mWifi.getWifiApConfiguration();
    }

    @Rpc(description = "Return a list of all the configured wifi networks.")
    public List<WifiConfiguration> wifiGetConfiguredNetworks() {
        return mWifi.getConfiguredNetworks();
    }

    @Rpc(description = "Returns information about the currently active access point.")
    public WifiInfo wifiGetConnectionInfo() {
        return mWifi.getConnectionInfo();
    }

    @Rpc(description = "Returns wifi activity and energy usage info.")
    public WifiActivityEnergyInfo wifiGetControllerActivityEnergyInfo() {
        WifiActivityEnergyInfo[] mutable = {null};
        CountDownLatch latch = new CountDownLatch(1);
        mWifi.getWifiActivityEnergyInfoAsync(new Executor() {
            @Override
            public void execute(Runnable runnable) {
                runnable.run();
            }
        }, info -> {
            mutable[0] = info;
            latch.countDown();
        });
        boolean completedSuccessfully = false;
        try {
            completedSuccessfully = latch.await(TIMEOUT_MILLIS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            Log.w("Interrupted while awaiting for WifiManager.getWifiActivityEnergyInfoAsync()");
        }
        if (completedSuccessfully) {
            return mutable[0];
        } else {
            Log.w("WifiManager.getWifiActivityEnergyInfoAsync() timed out after "
                    + TIMEOUT_MILLIS + " milliseconds");
            return null;
        }
    }

    @Rpc(description = "Get the country code used by WiFi.")
    public String wifiGetCountryCode() {
        return mWifi.getCountryCode();
    }

    @Rpc(description = "Get the current network.")
    public Network wifiGetCurrentNetwork() {
        return mWifi.getCurrentNetwork();
    }

    @Rpc(description = "Get the info from last successful DHCP request.")
    public DhcpInfo wifiGetDhcpInfo() {
        return mWifi.getDhcpInfo();
    }

    @Rpc(description = "Get setting for Framework layer autojoin enable status.")
    public Boolean wifiGetEnableAutoJoinWhenAssociated() {
        return mWifi.getEnableAutoJoinWhenAssociated();
    }

    @Rpc(description = "Get privileged configured networks.")
    public List<WifiConfiguration> wifiGetPrivilegedConfiguredNetworks() {
        return mWifi.getPrivilegedConfiguredNetworks();
    }

    @Rpc(description = "Returns the list of access points found during the most recent Wifi scan.")
    public List<ScanResult> wifiGetScanResults() {
        return mWifi.getScanResults();
    }

    @Rpc(description = "Get the current level of WiFi verbose logging.")
    public Integer wifiGetVerboseLoggingLevel() {
        return mWifi.isVerboseLoggingEnabled() ? 1 : 0;
    }

    @Rpc(description = "true if this adapter supports 5 GHz band.")
    public Boolean wifiIs5GHzBandSupported() {
        return mWifi.is5GHzBandSupported();
    }

    @Rpc(description = "true if this adapter supports multiple simultaneous connections.")
    public Boolean wifiIsAdditionalStaSupported() {
        return mWifi.isAdditionalStaSupported();
    }

    @Rpc(description = "Return true if WiFi is enabled.")
    public Boolean wifiGetisWifiEnabled() {
        return mWifi.isWifiEnabled();
    }

    @Rpc(description = "Return whether Wi-Fi AP is enabled or disabled.")
    public Boolean wifiIsApEnabled() {
        return mWifi.isWifiApEnabled();
    }

    @Rpc(description = "Check if Device-to-AP RTT is supported.")
    public Boolean wifiIsDeviceToApRttSupported() {
        return mWifi.isDeviceToApRttSupported();
    }

    @Rpc(description = "Check if Device-to-device RTT is supported.")
    public Boolean wifiIsDeviceToDeviceRttSupported() {
        return mWifi.isDeviceToDeviceRttSupported();
    }

    /**
     * @return true if chipset supports 5GHz band and false otherwise.
     */
    @Rpc(description = "Check if the chipset supports 5GHz frequency band.")
    public Boolean is5GhzBandSupported() {
        return mWifi.is5GHzBandSupported();
    }

    /**
     * @return true if chipset supports 6GHz band and false otherwise.
     */
    @Rpc(description = "Check if the chipset supports 6GHz frequency band.")
    public Boolean is6GhzBandSupported() {
        return mWifi.is6GHzBandSupported();
    }

    @Rpc(description = "Check if this adapter supports advanced power/performance counters.")
    public Boolean wifiIsEnhancedPowerReportingSupported() {
        return mWifi.isEnhancedPowerReportingSupported();
    }

    @Rpc(description = "Check if multicast is enabled.")
    public Boolean wifiIsMulticastEnabled() {
        return mWifi.isMulticastEnabled();
    }

    @Rpc(description = "true if this adapter supports Wi-Fi Aware APIs.")
    public Boolean wifiIsAwareSupported() {
        return mWifi.isWifiAwareSupported();
    }

    @Rpc(description = "true if this adapter supports Off Channel Tunnel Directed Link Setup.")
    public Boolean wifiIsOffChannelTdlsSupported() {
        return mWifi.isOffChannelTdlsSupported();
    }

    @Rpc(description = "true if this adapter supports WifiP2pManager (Wi-Fi Direct).")
    public Boolean wifiIsP2pSupported() {
        return mWifi.isP2pSupported();
    }

    @Rpc(description = "true if this adapter supports passpoint.")
    public Boolean wifiIsPasspointSupported() {
        return mWifi.isPasspointSupported();
    }

    @Rpc(description = "true if this adapter supports portable Wi-Fi hotspot.")
    public Boolean wifiIsPortableHotspotSupported() {
        return mWifi.isPortableHotspotSupported();
    }

    @Rpc(description = "true if this adapter supports offloaded connectivity scan.")
    public Boolean wifiIsPreferredNetworkOffloadSupported() {
        return mWifi.isPreferredNetworkOffloadSupported();
    }

    @Rpc(description = "Check if wifi scanner is supported on this device.")
    public Boolean wifiIsScannerSupported() {
        return mWifi.isWifiScannerSupported();
    }

    @Rpc(description = "Check if tdls is supported on this device.")
    public Boolean wifiIsTdlsSupported() {
        return mWifi.isTdlsSupported();
    }

    /**
     * @return true if this device supports WPA3-Personal SAE
     */
    @Rpc(description = "Check if WPA3-Personal SAE is supported on this device.")
    public Boolean wifiIsWpa3SaeSupported() {
        return mWifi.isWpa3SaeSupported();
    }
    /**
     * @return true if this device supports WPA3-Enterprise Suite-B-192
     */
    @Rpc(description = "Check if WPA3-Enterprise Suite-B-192 is supported on this device.")
    public Boolean wifiIsWpa3SuiteBSupported() {
        return mWifi.isWpa3SuiteBSupported();
    }
    /**
     * @return true if this device supports Wi-Fi Enhanced Open (OWE)
     */
    @Rpc(description = "Check if Enhanced Open (OWE) is supported on this device.")
    public Boolean wifiIsEnhancedOpenSupported() {
        return mWifi.isEnhancedOpenSupported();
    }
    /**
     * @return true if this device supports Wi-Fi Device Provisioning Protocol (Easy-connect)
     */
    @Rpc(description = "Check if Easy Connect (DPP) is supported on this device.")
    public Boolean wifiIsEasyConnectSupported() {
        return mWifi.isEasyConnectSupported();
    }

    @Rpc(description = "Acquires a full Wifi lock.")
    public void wifiLockAcquireFull() {
        makeLock(WifiManager.WIFI_MODE_FULL);
    }

    @Rpc(description = "Acquires a scan only Wifi lock.")
    public void wifiLockAcquireScanOnly() {
        makeLock(WifiManager.WIFI_MODE_SCAN_ONLY);
    }

    @Rpc(description = "Releases a previously acquired Wifi lock.")
    public void wifiLockRelease() {
        if (mLock != null) {
            mLock.release();
            mLock = null;
        }
    }

    @Rpc(description = "Reassociates with the currently active access point.", returns = "True if the operation succeeded.")
    public Boolean wifiReassociate() {
        return mWifi.reassociate();
    }

    @Rpc(description = "Reconnects to the currently active access point.", returns = "True if the operation succeeded.")
    public Boolean wifiReconnect() {
        return mWifi.reconnect();
    }

    @Rpc(description = "Remove a configured network.", returns = "True if the operation succeeded.")
    public Boolean wifiRemoveNetwork(@RpcParameter(name = "netId") Integer netId) {
        return mWifi.removeNetwork(netId);
    }

    private SoftApConfiguration createSoftApConfiguration(JSONObject configJson)
            throws JSONException {
        if (configJson == null) {
            return null;
        }
        SoftApConfiguration.Builder configBuilder = new SoftApConfiguration.Builder();
        if (configJson.has("SSID")) {
            configBuilder.setSsid(configJson.getString("SSID"));
        }
        if (configJson.has("password")) {
            String pwd = configJson.getString("password");
            // Check if new security type SAE (WPA3) is present. Default to PSK
            if (configJson.has("security")) {
                String securityType = configJson.getString("security");
                if (TextUtils.equals(securityType, "WPA2_PSK")) {
                    configBuilder.setPassphrase(pwd, SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
                } else if (TextUtils.equals(securityType, "WPA3_SAE_TRANSITION")) {
                    configBuilder.setPassphrase(pwd,
                            SoftApConfiguration.SECURITY_TYPE_WPA3_SAE_TRANSITION);
                } else if (TextUtils.equals(securityType, "WPA3_SAE")) {
                    configBuilder.setPassphrase(pwd, SoftApConfiguration.SECURITY_TYPE_WPA3_SAE);
                }
            } else {
                configBuilder.setPassphrase(pwd, SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
            }
        }
        if (configJson.has("BSSID")) {
            configBuilder.setBssid(MacAddress.fromString(configJson.getString("BSSID")));
        }
        if (configJson.has("hiddenSSID")) {
            configBuilder.setHiddenSsid(configJson.getBoolean("hiddenSSID"));
        }
        if (configJson.has("apBand")) {
            configBuilder.setBand(configJson.getInt("apBand"));
        }
        if (configJson.has("apChannel") && configJson.has("apBand")) {
            configBuilder.setChannel(configJson.getInt("apChannel"), configJson.getInt("apBand"));
        }

        if (configJson.has("MaxNumberOfClients")) {
            configBuilder.setMaxNumberOfClients(configJson.getInt("MaxNumberOfClients"));
        }

        if (configJson.has("ShutdownTimeoutMillis")) {
            configBuilder.setShutdownTimeoutMillis(configJson.getLong("ShutdownTimeoutMillis"));
        }

        if (configJson.has("AutoShutdownEnabled")) {
            configBuilder.setAutoShutdownEnabled(configJson.getBoolean("AutoShutdownEnabled"));
        }

        if (configJson.has("ClientControlByUserEnabled")) {
            configBuilder.setClientControlByUserEnabled(
                    configJson.getBoolean("ClientControlByUserEnabled"));
        }

        List allowedClientList = new ArrayList<>();
        if (configJson.has("AllowedClientList")) {
            JSONArray allowedList = configJson.getJSONArray("AllowedClientList");
            for (int i = 0; i < allowedList.length(); i++) {
                allowedClientList.add(MacAddress.fromString(allowedList.getString(i)));
            }
        }

        List blockedClientList = new ArrayList<>();
        if (configJson.has("BlockedClientList")) {
            JSONArray blockedList = configJson.getJSONArray("BlockedClientList");
            for (int j = 0; j < blockedList.length(); j++) {
                blockedClientList.add(MacAddress.fromString(blockedList.getString(j)));
            }

        }
        configBuilder.setAllowedClientList(allowedClientList);
        configBuilder.setBlockedClientList(blockedClientList);
        return configBuilder.build();
    }

    private WifiConfiguration createSoftApWifiConfiguration(JSONObject configJson)
            throws JSONException {
        WifiConfiguration config = genWifiConfig(configJson);
        // Need to strip of extra quotation marks for SSID and password.
        String ssid = config.SSID;
        if (ssid != null) {
            config.SSID = ssid.substring(1, ssid.length() - 1);
        }

        config.allowedKeyManagement.clear();
        String pwd = config.preSharedKey;
        if (pwd != null) {
            config.allowedKeyManagement.set(KeyMgmt.WPA2_PSK);
            config.preSharedKey = pwd.substring(1, pwd.length() - 1);
        } else {
            config.allowedKeyManagement.set(KeyMgmt.NONE);
        }
        return config;
    }

    /**
     * Set SoftAp Configuration with SoftApConfiguration.
     */
    @Rpc(description = "Set configuration for soft AP.")
    public Boolean wifiSetWifiApConfiguration(
            @RpcParameter(name = "configJson") JSONObject configJson) throws JSONException {
        return mWifi.setSoftApConfiguration(createSoftApConfiguration(configJson));
    }

    /**
     * Set SoftAp Configuration with WifiConfiguration.
     *
     * Used to test deprecated API to check backward compatible.
     */
    @Rpc(description = "Set configuration for soft AP with WifiConfig.")
    public Boolean wifiSetWifiApConfigurationWithWifiConfiguration(
            @RpcParameter(name = "configJson") JSONObject configJson) throws JSONException {
        return mWifi.setWifiApConfiguration(createSoftApWifiConfiguration(configJson));
    }

    /**
     * Register softap callback.
     *
     * @return the id associated with the {@link SoftApCallbackImp}
     * used for registering callback.
     */
    @Rpc(description = "Register softap callback function.",
            returns = "Id of the callback associated with registering.")
    public Integer registerSoftApCallback() {
        SoftApCallbackImp softApCallback = new SoftApCallbackImp(mEventFacade);
        mSoftapCallbacks.put(softApCallback.mId, softApCallback);
        mWifi.registerSoftApCallback(
                new HandlerExecutor(new Handler(mCallbackHandlerThread.getLooper())),
                softApCallback);
        return softApCallback.mId;
    }

    /**
     * Unregister softap callback role for the {@link SoftApCallbackImp} identified by the given
     * {@code callbackId}.
     *
     * @param callbackId the id associated with the {@link SoftApCallbackImp}
     * used for registering callback.
     *
     */
    @Rpc(description = "Unregister softap callback function.")
    public void unregisterSoftApCallback(@RpcParameter(name = "callbackId") Integer callbackId) {
        mWifi.unregisterSoftApCallback(mSoftapCallbacks.get(callbackId));
        mSoftapCallbacks.delete(callbackId);
    }

    @Rpc(description = "Enable/disable tdls with a mac address.")
    public void wifiSetTdlsEnabledWithMacAddress(
            @RpcParameter(name = "remoteMacAddress") String remoteMacAddress,
            @RpcParameter(name = "enable") Boolean enable) {
        mWifi.setTdlsEnabledWithMacAddress(remoteMacAddress, enable);
    }

    @Rpc(description = "Starts a scan for Wifi access points.", returns = "True if the scan was initiated successfully.")
    public Boolean wifiStartScan() {
        mService.registerReceiver(mScanResultsAvailableReceiver, mScanFilter);
        return mWifi.startScan();
    }

    @Rpc(description = "Starts a scan for Wifi access points with scanResultCallback.",
            returns = "True if the scan was initiated successfully.")
    public Boolean wifiStartScanWithListener() {
        mWifi.registerScanResultsCallback(mService.getMainExecutor(), mWifiScanResultsReceiver);
        return mWifi.startScan();
    }

    @Rpc(description = "Start Wi-fi Protected Setup.")
    public void wifiStartWps(
            @RpcParameter(name = "config", description = "A json string with fields \"setup\", \"BSSID\", and \"pin\"") String config)
                    throws JSONException {
        WpsInfo info = parseWpsInfo(config);
        WifiWpsCallback listener = new WifiWpsCallback();
        Log.d("Starting wps with: " + info);
        mWifi.startWps(info, listener);
    }

    @Rpc(description = "Start listening for wifi state change related broadcasts.")
    public void wifiStartTrackingStateChange() {
        mService.registerReceiver(mStateChangeReceiver, mStateChangeFilter);
        mTrackingWifiStateChange = true;
    }

    @Rpc(description = "Stop listening for wifi state change related broadcasts.")
    public void wifiStopTrackingStateChange() {
        if (mTrackingWifiStateChange == true) {
            mService.unregisterReceiver(mStateChangeReceiver);
            mTrackingWifiStateChange = false;
        }
    }

    @Rpc(description = "Start listening for tether state change related broadcasts.")
    public void wifiStartTrackingTetherStateChange() {
        mService.registerReceiver(mTetherStateReceiver, mTetherFilter);
        mTrackingTetherStateChange = true;
    }

    @Rpc(description = "Stop listening for wifi state change related broadcasts.")
    public void wifiStopTrackingTetherStateChange() {
        if (mTrackingTetherStateChange == true) {
            mService.unregisterReceiver(mTetherStateReceiver);
            mTrackingTetherStateChange = false;
        }
    }

    @Rpc(description = "Start listening for network suggestion change related broadcasts.")
    public void wifiStartTrackingNetworkSuggestionStateChange() {
        mService.registerReceiver(
                mNetworkSuggestionStateChangeReceiver, mNetworkSuggestionStateChangeFilter);
        mTrackingNetworkSuggestionStateChange = true;
    }

    @Rpc(description = "Stop listening for network suggestion change related broadcasts.")
    public void wifiStopTrackingNetworkSuggestionStateChange() {
        if (mTrackingNetworkSuggestionStateChange) {
            mService.unregisterReceiver(mNetworkSuggestionStateChangeReceiver);
            mTrackingNetworkSuggestionStateChange = false;
        }
    }

    @Rpc(description = "Toggle Wifi on and off.", returns = "True if Wifi is enabled.")
    public Boolean wifiToggleState(@RpcParameter(name = "enabled") @RpcOptional Boolean enabled) {
        if (enabled == null) {
            enabled = !wifiCheckState();
        }
        mWifi.setWifiEnabled(enabled);
        return enabled;
    }

    @Rpc(description = "Toggle Wifi scan always available on and off.", returns = "True if Wifi scan is always available.")
    public Boolean wifiToggleScanAlwaysAvailable(
            @RpcParameter(name = "enabled") @RpcOptional Boolean enabled)
                    throws SettingNotFoundException {
        boolean isSet = (enabled == null) ? !mWifi.isScanAlwaysAvailable() : enabled;
        mWifi.setScanAlwaysAvailable(isSet);
        return isSet;
    }

    @Rpc(description = "Enable/disable WifiConnectivityManager.")
    public void wifiEnableWifiConnectivityManager(
            @RpcParameter(name = "enable") Boolean enable) {
        mWifi.allowAutojoinGlobal(enable);
    }

    private void wifiRequestNetworkWithSpecifierInternal(NetworkSpecifier wns, int timeoutInMs)
            throws GeneralSecurityException {
        NetworkRequest networkRequest = new NetworkRequest.Builder()
                .addTransportType(TRANSPORT_WIFI)
                .setNetworkSpecifier(wns)
                .build();
        NetworkCallback networkCallback = new NetworkCallbackImpl();
        if (timeoutInMs != 0) {
            mCm.requestNetwork(networkRequest, networkCallback,
                    new Handler(mCallbackHandlerThread.getLooper()), timeoutInMs);
        } else {
            mCm.requestNetwork(networkRequest, networkCallback,
                    new Handler(mCallbackHandlerThread.getLooper()));
        }
        // Store the callback for release later.
        mNetworkCallbacks.put(wns, networkCallback);
    }

    /**
     * Initiates a network request {@link NetworkRequest} using {@link WifiNetworkSpecifier}.
     *
     * @param wifiNetworkSpecifier JSONObject Dictionary of wifi network specifier parameters
     * @throws JSONException
     * @throws GeneralSecurityException
     */
    @Rpc(description = "Initiates a network request using the provided network specifier")
    public void wifiRequestNetworkWithSpecifier(
            @RpcParameter(name = "wifiNetworkSpecifier") JSONObject wifiNetworkSpecifier)
            throws JSONException, GeneralSecurityException {
        wifiRequestNetworkWithSpecifierInternal(genWifiNetworkSpecifier(wifiNetworkSpecifier), 0);
    }

    /**
     * Initiates a network request {@link NetworkRequest} using {@link WifiNetworkSpecifier}.
     *
     * @param wifiNetworkSpecifier JSONObject Dictionary of wifi network specifier parameters
     * @param timeoutInMs Timeout for the request.
     * @throws JSONException
     * @throws GeneralSecurityException
     */
    @Rpc(description = "Initiates a network request using the provided network specifier")
    public void wifiRequestNetworkWithSpecifierWithTimeout(
            @RpcParameter(name = "wifiNetworkSpecifier") JSONObject wifiNetworkSpecifier,
            @RpcParameter(name = "timeout") Integer timeoutInMs)
            throws JSONException, GeneralSecurityException {
        wifiRequestNetworkWithSpecifierInternal(
                genWifiNetworkSpecifier(wifiNetworkSpecifier), timeoutInMs);
    }

    /**
     * Releases network request using {@link WifiNetworkSpecifier}.
     *
     * @throws JSONException
     * @throws GeneralSecurityException
     */
    @Rpc(description = "Releases network request corresponding to the network specifier")
    public void wifiReleaseNetwork(
            @RpcParameter(name = "wifiNetworkSpecifier") JSONObject wifiNetworkSpecifier)
            throws JSONException, GeneralSecurityException {
        NetworkSpecifier wns = genWifiNetworkSpecifier(wifiNetworkSpecifier);
        NetworkCallback networkCallback = mNetworkCallbacks.remove(wns);
        if (networkCallback == null) {
            throw new IllegalArgumentException("network callback is null");
        }
        mCm.unregisterNetworkCallback(networkCallback);
    }

    /**
     * Releases all pending network requests.
     *
     * @throws JSONException
     * @throws GeneralSecurityException
     */
    @Rpc(description = "Releases all pending network requests")
    public void wifiReleaseNetworkAll() {
        Iterator<Map.Entry<NetworkSpecifier, NetworkCallback>> it =
                mNetworkCallbacks.entrySet().iterator();
        while (it.hasNext()) {
            Map.Entry<NetworkSpecifier, NetworkCallback> entry = it.next();
            NetworkCallback networkCallback = entry.getValue();
            it.remove();
            mCm.unregisterNetworkCallback(networkCallback);
        }
    }

    /**
     * Register network request match callback to simulate the UI flow.
     *
     * @throws JSONException
     * @throws GeneralSecurityException
     */
    @Rpc(description = "Register network request match callback")
    public void wifiRegisterNetworkRequestMatchCallback()
            throws JSONException, GeneralSecurityException {
        // Listen for UI interaction callbacks
        mWifi.registerNetworkRequestMatchCallback(
                new HandlerExecutor(new Handler(mCallbackHandlerThread.getLooper())),
                mNetworkRequestMatchCallback);
    }

    /**
     * Triggers connect to a specific wifi network.
     *
     * @param jsonConfig JSONObject Dictionary of wifi connection parameters
     * @throws JSONException
     * @throws GeneralSecurityException
     */
    @Rpc(description = "Connects to the specified network for the ongoing network request")
    public void wifiSendUserSelectionForNetworkRequestMatch(
            @RpcParameter(name = "jsonConfig") JSONObject jsonConfig)
            throws JSONException, GeneralSecurityException {
        synchronized (mCallbackLock) {
            if (mNetworkRequestUserSelectionCallback == null) {
                throw new IllegalStateException("user callback is null");
            }
            // Copy the SSID for user selection.
            WifiConfiguration config = new WifiConfiguration();
            if (jsonConfig.has("SSID")) {
                config.SSID = "\"" + jsonConfig.getString("SSID") + "\"";
            }
            mNetworkRequestUserSelectionCallback.select(config);
        }
    }

    /**
     * Rejects network request.
     *
     * @throws JSONException
     * @throws GeneralSecurityException
     */
    @Rpc(description = "Rejects ongoing network request")
    public void wifiSendUserRejectionForNetworkRequestMatch()
            throws JSONException, GeneralSecurityException {
        synchronized (mCallbackLock) {
            if (mNetworkRequestUserSelectionCallback == null) {
                throw new IllegalStateException("user callback is null");
            }
            mNetworkRequestUserSelectionCallback.reject();
        }
    }

    /**
     * Add network suggestions.

     * @param wifiNetworkSuggestions Array of JSONObject Dictionary of wifi network suggestion
     *                               parameters
     * @throws JSONException
     * @throws GeneralSecurityException
     */
    @Rpc(description = "Add network suggestions to the platform")
    public boolean wifiAddNetworkSuggestions(
            @RpcParameter(name = "wifiNetworkSuggestions") JSONArray wifiNetworkSuggestions)
            throws JSONException, GeneralSecurityException, IOException {
        return mWifi.addNetworkSuggestions(genWifiNetworkSuggestions(wifiNetworkSuggestions))
                == WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS;
    }

    /**
     * Remove network suggestions.

     * @param wifiNetworkSuggestions Array of JSONObject Dictionary of wifi network suggestion
     *                               parameters
     * @throws JSONException
     * @throws GeneralSecurityException
     */
    @Rpc(description = "Remove network suggestions from the platform")
    public boolean wifiRemoveNetworkSuggestions(
            @RpcParameter(name = "wifiNetworkSuggestions") JSONArray wifiNetworkSuggestions)
            throws JSONException, GeneralSecurityException, IOException {
        return mWifi.removeNetworkSuggestions(genWifiNetworkSuggestions(wifiNetworkSuggestions))
                == WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS;
    }

    @Override
    public void shutdown() {
        wifiReleaseNetworkAll();
        wifiLockRelease();
        if (mTrackingWifiStateChange == true) {
            wifiStopTrackingStateChange();
        }
        if (mTrackingTetherStateChange == true) {
            wifiStopTrackingTetherStateChange();
        }
    }

    private class EasyConnectCallback extends EasyConnectStatusCallback {
        private static final String EASY_CONNECT_CALLBACK_TAG = "onDppCallback";

        @Override
        public void onEnrolleeSuccess(int newWifiConfigurationId) {
            Bundle msg = new Bundle();
            msg.putString("Type", "onEnrolleeSuccess");
            msg.putInt("NetworkId", newWifiConfigurationId);
            Log.d("Posting event: onEnrolleeSuccess");
            mEventFacade.postEvent(EASY_CONNECT_CALLBACK_TAG, msg);
        }

        @Override
        public void onConfiguratorSuccess(int code) {
            Bundle msg = new Bundle();
            msg.putString("Type", "onConfiguratorSuccess");
            msg.putInt("Status", code);
            Log.d("Posting event: onConfiguratorSuccess");
            mEventFacade.postEvent(EASY_CONNECT_CALLBACK_TAG, msg);
        }

        @Override
        public void onFailure(int code, String ssid, SparseArray<int[]> channelList,
                int[] bandList) {
            Bundle msg = new Bundle();
            msg.putString("Type", "onFailure");
            msg.putInt("Status", code);
            Log.d("Posting event: onFailure");
            if (ssid != null) {
                Log.d("onFailure SSID: " + ssid);
                msg.putString("onFailureSsid", ssid);
            } else {
                msg.putString("onFailureSsid", "");
            }
            if (channelList != null) {
                Log.d("onFailure list of tried channels: " + channelList);
                int key;
                int index = 0;
                JSONObject formattedChannelList = new JSONObject();

                // Build a JSON array of classes, with an array of channels for each class.
                do {
                    try {
                        key = channelList.keyAt(index);
                    } catch (java.lang.ArrayIndexOutOfBoundsException e) {
                        break;
                    }
                    try {
                        JSONArray channelsInClassArray = new JSONArray();

                        int[] output = channelList.get(key);
                        for (int i = 0; i < output.length; i++) {
                            channelsInClassArray.put(output[i]);
                        }
                        formattedChannelList.put(Integer.toString(key),
                                build(channelsInClassArray));
                    } catch (org.json.JSONException e) {
                        msg.putString("onFailureChannelList", "");
                        break;
                    }
                    index++;
                } while (true);

                msg.putString("onFailureChannelList", formattedChannelList.toString());
            } else {
                msg.putString("onFailureChannelList", "");
            }

            if (bandList != null) {
                // Build a JSON array of bands represented as operating classes
                Log.d("onFailure list of supported bands: " + bandList);
                JSONArray formattedBandList = new JSONArray();
                for (int i = 0; i < bandList.length; i++) {
                    formattedBandList.put(bandList[i]);
                }
                msg.putString("onFailureBandList", formattedBandList.toString());
            } else {
                msg.putString("onFailureBandList", "");
            }
            mEventFacade.postEvent(EASY_CONNECT_CALLBACK_TAG, msg);
        }

        @Override
        public void onProgress(int code) {
            Bundle msg = new Bundle();
            msg.putString("Type", "onProgress");
            msg.putInt("Status", code);
            Log.d("Posting event: onProgress");
            mEventFacade.postEvent(EASY_CONNECT_CALLBACK_TAG, msg);
        }
    }

    /**
     * Start Easy Connect (DPP) in Initiator-Configurator role: Send Wi-Fi configuration to a peer
     *
     * @param enrolleeUri Peer URI
     * @param selectedNetworkId Wi-Fi configuration ID
     */
    @Rpc(description = "Easy Connect Initiator-Configurator: Send Wi-Fi configuration to peer")
    public void startEasyConnectAsConfiguratorInitiator(@RpcParameter(name = "enrolleeUri") String
            enrolleeUri, @RpcParameter(name = "selectedNetworkId") Integer selectedNetworkId,
            @RpcParameter(name = "netRole") String netRole)
            throws JSONException {
        EasyConnectCallback dppStatusCallback = new EasyConnectCallback();
        int netRoleInternal;

        if (netRole.equals("ap")) {
            netRoleInternal = WifiManager.EASY_CONNECT_NETWORK_ROLE_AP;
        } else {
            netRoleInternal = WifiManager.EASY_CONNECT_NETWORK_ROLE_STA;
        }

        // Start Easy Connect
        mWifi.startEasyConnectAsConfiguratorInitiator(enrolleeUri, selectedNetworkId,
                netRoleInternal, mService.getMainExecutor(), dppStatusCallback);
    }

    /**
     * Start Easy Connect (DPP) in Initiator-Enrollee role: Receive Wi-Fi configuration from a peer
     *
     * @param configuratorUri
     */
    @Rpc(description = "Easy Connect Initiator-Enrollee: Receive Wi-Fi configuration from peer")
    public void startEasyConnectAsEnrolleeInitiator(@RpcParameter(name = "configuratorUri") String
            configuratorUri) {
        EasyConnectCallback dppStatusCallback = new EasyConnectCallback();

        // Start Easy Connect
        mWifi.startEasyConnectAsEnrolleeInitiator(configuratorUri, mService.getMainExecutor(),
                dppStatusCallback);
    }

    /**
     * Stop Easy Connect (DPP) session
     *
     */
    @Rpc(description = "Stop Easy Connect session")
    public void stopEasyConnectSession() {
        // Stop Easy Connect
        mWifi.stopEasyConnectSession();
    }

    /**
     * Enable/Disable auto join for target network
     */
    @Rpc(description = "Set network auto join enable/disable")
    public void wifiEnableAutojoin(@RpcParameter(name = "netId") Integer netId,
            @RpcParameter(name = "enableAutojoin") Boolean enableAutojoin) {
        mWifi.allowAutojoin(netId, enableAutojoin);
    }

    /**
     * Enable/Disable auto join for target Passpoint network
     */
    @Rpc(description = "Set passpoint network auto join enable/disable")
    public void wifiEnableAutojoinPasspoint(@RpcParameter(name = "FQDN") String fqdn,
            @RpcParameter(name = "enableAutojoin") Boolean enableAutojoin) {
        mWifi.allowAutojoinPasspoint(fqdn, enableAutojoin);
    }
}
