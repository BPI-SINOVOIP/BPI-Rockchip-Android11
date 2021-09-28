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

package com.android.cts.verifier.wifi;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.wifi.ScanResult;
import android.net.wifi.SupplicantState;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.text.TextUtils;
import android.util.Log;

import com.android.cts.verifier.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Random;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Test utility methods.
 */
public class TestUtils {
    private static final String TAG = "NetworkRequestTestCase";
    private static final boolean DBG = true;
    private static final int SCAN_TIMEOUT_MS = 30_000;

    private final Context mContext;
    protected BaseTestCase.Listener mListener;
    private final WifiManager mWifiManager;

    public TestUtils(Context context, BaseTestCase.Listener listener) {
        mContext = context;
        mListener = listener;
        mWifiManager = (WifiManager) mContext.getSystemService(Context.WIFI_SERVICE);
    }

    private boolean startScanAndWaitForResults() throws InterruptedException {
        IntentFilter intentFilter = new IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION);
        final CountDownLatch countDownLatch = new CountDownLatch(1);
        // Scan Results available broadcast receiver.
        BroadcastReceiver broadcastReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (DBG) Log.v(TAG, "Broadcast onReceive " + intent);
                if (!intent.getAction().equals(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION)) return;
                if (!intent.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false)) return;
                if (DBG) Log.v(TAG, "Scan results received");
                countDownLatch.countDown();
            }
        };
        // Register the receiver for scan results broadcast.
        mContext.registerReceiver(broadcastReceiver, intentFilter);

        // Start scan.
        if (DBG) Log.v(TAG, "Starting scan");
        mListener.onTestMsgReceived(mContext.getString(R.string.wifi_status_initiating_scan));
        if (!mWifiManager.startScan()) {
            Log.e(TAG, "Failed to start scan");
            // Unregister the receiver for scan results broadcast.
            mContext.unregisterReceiver(broadcastReceiver);
            return false;
        }
        // Wait for scan results.
        if (DBG) Log.v(TAG, "Wait for scan results");
        if (!countDownLatch.await(SCAN_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            Log.e(TAG, "No new scan results available");
            // Unregister the receiver for scan results broadcast.
            mContext.unregisterReceiver(broadcastReceiver);
            return false;
        }

        // Unregister the receiver for scan results broadcast.
        mContext.unregisterReceiver(broadcastReceiver);
        return true;
    }

    public static final int SCAN_RESULT_TYPE_OPEN = 0;
    public static final int SCAN_RESULT_TYPE_PSK = 1;

    @IntDef(prefix = { "SCAN_RESULT_TYPE_" }, value = {
            SCAN_RESULT_TYPE_OPEN,
            SCAN_RESULT_TYPE_PSK,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScanResultType {}

    /**
     * Helper to check if the scan result corresponds to an open network.
     */
    public static boolean isScanResultForOpenNetwork(@NonNull ScanResult scanResult) {
        String capabilities = scanResult.capabilities;
        return !capabilities.contains("PSK") && !capabilities.contains("EAP")
                && !capabilities.contains("WEP") && !capabilities.contains("SAE")
                && !capabilities.contains("SUITE-B-192") && !capabilities.contains("OWE");
    }

    /**
     * Helper to check if the scan result corresponds to a WPA2 PSK network.
     */
    public static boolean isScanResultForWpa2Network(@NonNull ScanResult scanResult) {
        String capabilities = scanResult.capabilities;
        return capabilities.contains("PSK");
    }

    /**
     * Helper to check if the scan result corresponds to a WPA3 PSK network.
     */
    public static boolean isScanResultForWpa3Network(@NonNull ScanResult scanResult) {
        String capabilities = scanResult.capabilities;
        return capabilities.contains("SAE");
    }

    private static boolean doesScanResultMatchType(
            ScanResult scanResult, @ScanResultType int type) {
        switch(type) {
            case SCAN_RESULT_TYPE_OPEN:
                return isScanResultForOpenNetwork(scanResult);
            case SCAN_RESULT_TYPE_PSK:
                return isScanResultForWpa2Network(scanResult)
                        || isScanResultForWpa3Network(scanResult);
            default:
                return false;
        }
    }

    /**
     * Helper method to start a scan and find any type of networks in the scan results returned by
     * the device.
     * @return ScanResult instance corresponding to an network of type if one exists, null if the
     * scan failed or if there are networks found.
     */
    public @Nullable ScanResult startScanAndFindAnyMatchingNetworkInResults(
            String ssid, @ScanResultType int type)
            throws InterruptedException {
        // Start scan and wait for new results.
        if (!startScanAndWaitForResults()) {
            Log.e(TAG,"Failed to initiate a new scan. Using cached results from device");
        }
        // Filter results to find an open network.
        List<ScanResult> scanResults = mWifiManager.getScanResults();
        for (ScanResult scanResult : scanResults) {
            if (TextUtils.equals(ssid, scanResult.SSID)
                    && !TextUtils.isEmpty(scanResult.BSSID)
                    && doesScanResultMatchType(scanResult, type)) {
                if (DBG) Log.v(TAG, "Found network " + scanResult);
                return scanResult;
            }
        }
        Log.e(TAG, "No matching network found in scan results");
        return null;
    }

    /**
     * Helper method to check if a scan result with the specified SSID & BSSID matches the scan
     * results returned by the device.
     *
     * @param ssid SSID of the network.
     * @param bssid BSSID of network.
     * @return true if there is a match, false otherwise.
     */
    public boolean findNetworkInScanResultsResults(@NonNull String ssid, @NonNull String bssid) {
        List<ScanResult> scanResults = mWifiManager.getScanResults();
        for (ScanResult scanResult : scanResults) {
            if (TextUtils.equals(scanResult.SSID, ssid)
                    && TextUtils.equals(scanResult.BSSID, bssid)) {
                if (DBG) Log.v(TAG, "Found network " + scanResult);
                return true;
            }
        }
        return false;
    }

    /**
     * Checks whether the device is connected.
     *
     * @param ssid If ssid is specified, then check where the device is connected to a network
     *             with the specified SSID.
     * @param bssid If bssid is specified, then check where the device is connected to a network
     *             with the specified BSSID.
     * @return true if the device is connected to a network with the specified params, false
     * otherwise.
     */
    public boolean isConnected(@Nullable String ssid, @Nullable String bssid) {
        WifiInfo wifiInfo = mWifiManager.getConnectionInfo();
        if (wifiInfo == null) {
            Log.e(TAG, "Failed to get WifiInfo");
            return false;
        }
        if (wifiInfo.getSupplicantState() != SupplicantState.COMPLETED) return false;
        if (ssid != null && !wifiInfo.getSSID().equals(ssid)) return false;
        if (bssid != null && !wifiInfo.getBSSID().equals(bssid)) return false;
        return true;
    }

    /**
     * Generate random passphrase to use for tests.
     * @return
     */
    public String generateRandomPassphrase() {
        return new Random().ints('a', 'z' + 1)
                .limit(45)
                .collect(StringBuilder::new, StringBuilder::appendCodePoint, StringBuilder::append)
                .toString();
    }
}
