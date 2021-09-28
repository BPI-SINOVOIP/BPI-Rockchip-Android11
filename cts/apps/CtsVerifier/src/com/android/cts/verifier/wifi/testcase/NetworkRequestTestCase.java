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

package com.android.cts.verifier.wifi.testcase;

import static android.net.NetworkCapabilities.NET_CAPABILITY_INTERNET;
import static android.net.NetworkCapabilities.TRANSPORT_WIFI;

import static com.android.cts.verifier.wifi.TestUtils.SCAN_RESULT_TYPE_OPEN;
import static com.android.cts.verifier.wifi.TestUtils.SCAN_RESULT_TYPE_PSK;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.MacAddress;
import android.net.Network;
import android.net.NetworkRequest;
import android.net.NetworkSpecifier;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiNetworkSpecifier;
import android.os.PatternMatcher;
import android.util.Log;
import android.util.Pair;

import com.android.cts.verifier.R;
import com.android.cts.verifier.wifi.BaseTestCase;
import com.android.cts.verifier.wifi.CallbackUtils;
import com.android.cts.verifier.wifi.TestUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Test case for all {@link NetworkRequest} requests with specifier built using
 * {@link WifiNetworkSpecifier.Builder#build()}.
 */
public class NetworkRequestTestCase extends BaseTestCase {
    private static final String TAG = "NetworkRequestTestCase";
    private static final boolean DBG = true;

    private static final String UNAVAILABLE_SSID = "blahblahblah";
    private static final String UNAVAILABLE_BSSID = "02:00:00:00:00:00";
    private static final int NETWORK_REQUEST_TIMEOUT_MS = 30_000;
    private static final int CALLBACK_TIMEOUT_MS = 40_000;

    public static final int NETWORK_SPECIFIER_SPECIFIC_SSID_BSSID = 0;
    public static final int NETWORK_SPECIFIER_PATTERN_SSID_BSSID = 1;
    public static final int NETWORK_SPECIFIER_UNAVAILABLE_SSID_BSSID = 2;
    public static final int NETWORK_SPECIFIER_INVALID_CREDENTIAL = 3;

    @IntDef(prefix = { "NETWORK_SPECIFIER_" }, value = {
            NETWORK_SPECIFIER_SPECIFIC_SSID_BSSID,
            NETWORK_SPECIFIER_PATTERN_SSID_BSSID,
            NETWORK_SPECIFIER_UNAVAILABLE_SSID_BSSID,
            NETWORK_SPECIFIER_INVALID_CREDENTIAL
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NetworkSpecifierType{}

    private final Object mLock = new Object();
    private final @NetworkSpecifierType int mNetworkSpecifierType;

    private ConnectivityManager mConnectivityManager;
    private NetworkRequest mNetworkRequest;
    private CallbackUtils.NetworkCallback mNetworkCallback;
    private String mFailureReason;

    public NetworkRequestTestCase(Context context, @NetworkSpecifierType int networkSpecifierType) {
        super(context);
        mNetworkSpecifierType = networkSpecifierType;
    }

    // Create a network specifier based on the test type.
    private NetworkSpecifier createNetworkSpecifier(@NonNull ScanResult scanResult)
            throws InterruptedException {
        WifiNetworkSpecifier.Builder configBuilder = new WifiNetworkSpecifier.Builder();
        switch (mNetworkSpecifierType) {
            case NETWORK_SPECIFIER_SPECIFIC_SSID_BSSID:
                configBuilder.setSsid(scanResult.SSID);
                configBuilder.setBssid(MacAddress.fromString(scanResult.BSSID));
                if (!mPsk.isEmpty()) {
                    if (TestUtils.isScanResultForWpa2Network(scanResult)) {
                        configBuilder.setWpa2Passphrase(mPsk);
                    } else if (TestUtils.isScanResultForWpa3Network(scanResult)) {
                        configBuilder.setWpa3Passphrase(mPsk);
                    }
                }
                break;
            case NETWORK_SPECIFIER_PATTERN_SSID_BSSID:
                String ssidPrefix = scanResult.SSID.substring(0, scanResult.SSID.length() - 1);
                MacAddress bssidMask = MacAddress.fromString("ff:ff:ff:ff:ff:00");
                configBuilder.setSsidPattern(
                        new PatternMatcher(ssidPrefix, PatternMatcher.PATTERN_PREFIX));
                configBuilder.setBssidPattern(MacAddress.fromString(scanResult.BSSID), bssidMask);
                if (!mPsk.isEmpty()) {
                    if (TestUtils.isScanResultForWpa2Network(scanResult)) {
                        configBuilder.setWpa2Passphrase(mPsk);
                    } else if (TestUtils.isScanResultForWpa3Network(scanResult)) {
                        configBuilder.setWpa3Passphrase(mPsk);
                    }
                }
                break;
            case NETWORK_SPECIFIER_UNAVAILABLE_SSID_BSSID:
                String ssid = UNAVAILABLE_SSID;
                MacAddress bssid = MacAddress.fromString(UNAVAILABLE_BSSID);
                if (mTestUtils.findNetworkInScanResultsResults(ssid, bssid.toString())) {
                    Log.e(TAG, "The specifiers chosen match a network in scan results."
                            + "Test will fail");
                    return null;
                }
                configBuilder.setSsid(UNAVAILABLE_SSID);
                configBuilder.setBssid(bssid);
                if (!mPsk.isEmpty()) {
                    if (TestUtils.isScanResultForWpa2Network(scanResult)) {
                        configBuilder.setWpa2Passphrase(mPsk);
                    } else if (TestUtils.isScanResultForWpa3Network(scanResult)) {
                        configBuilder.setWpa3Passphrase(mPsk);
                    }
                }
                break;
            case NETWORK_SPECIFIER_INVALID_CREDENTIAL:
                configBuilder.setSsid(scanResult.SSID);
                configBuilder.setBssid(MacAddress.fromString(scanResult.BSSID));
                // Use a random password to simulate connection failure.
                if (TestUtils.isScanResultForWpa2Network(scanResult)) {
                    configBuilder.setWpa2Passphrase(mTestUtils.generateRandomPassphrase());
                } else if (TestUtils.isScanResultForWpa3Network(scanResult)) {
                    configBuilder.setWpa3Passphrase(mTestUtils.generateRandomPassphrase());
                }
                break;
            default:
                throw new IllegalStateException("Unknown specifier type specifier");
        }
        return configBuilder.build();
    }


    private void setFailureReason(String reason) {
        synchronized (mLock) {
            mFailureReason = reason;
        }
    }

    @Override
    protected boolean executeTest() throws InterruptedException {
        if (mNetworkSpecifierType == NETWORK_SPECIFIER_INVALID_CREDENTIAL && mPsk.isEmpty()) {
            setFailureReason(mContext.getString(R.string.wifi_status_need_psk));
            return false;
        }
        // Step: Scan and find the network around.
        if (DBG) Log.v(TAG, "Scan and find the network: " + mSsid);
        ScanResult testNetwork = mTestUtils.startScanAndFindAnyMatchingNetworkInResults(
                mSsid, mPsk.isEmpty() ? SCAN_RESULT_TYPE_OPEN : SCAN_RESULT_TYPE_PSK);
        if (testNetwork == null) {
            setFailureReason(mContext.getString(R.string.wifi_status_scan_failure));
            return false;
        }

        // Step: Create a specifier for the chosen open network depending on the type of test.
        NetworkSpecifier wns = createNetworkSpecifier(testNetwork);
        if (wns == null) return false;

        // Step: Create a network request with specifier.
        mNetworkRequest = new NetworkRequest.Builder()
                .addTransportType(TRANSPORT_WIFI)
                .setNetworkSpecifier(wns)
                .removeCapability(NET_CAPABILITY_INTERNET)
                .build();

        // Step: Send the network request
        if (DBG) Log.v(TAG, "Request network using " + mNetworkRequest);
        mNetworkCallback = new CallbackUtils.NetworkCallback(CALLBACK_TIMEOUT_MS);
        mListener.onTestMsgReceived(
                mContext.getString(R.string.wifi_status_initiating_network_request));
        mConnectivityManager.requestNetwork(mNetworkRequest, mNetworkCallback,
                NETWORK_REQUEST_TIMEOUT_MS);

        // Step: Wait for the network available/unavailable callback.
        if (mNetworkSpecifierType == NETWORK_SPECIFIER_UNAVAILABLE_SSID_BSSID
                || mNetworkSpecifierType == NETWORK_SPECIFIER_INVALID_CREDENTIAL) {
            if (mNetworkSpecifierType == NETWORK_SPECIFIER_UNAVAILABLE_SSID_BSSID) {
                mListener.onTestMsgReceived(
                        mContext.getString(R.string.wifi_status_network_wait_for_unavailable));
            } else {
                mListener.onTestMsgReceived(
                        mContext.getString(R.string
                                .wifi_status_network_wait_for_unavailable_invalid_credential));
            }
            if (DBG) Log.v(TAG, "Waiting for network unavailable callback");
            boolean cbStatusForUnavailable = mNetworkCallback.waitForUnavailable();
            if (!cbStatusForUnavailable) {
                Log.e(TAG, "Failed to get network unavailable callback");
                setFailureReason(mContext.getString(R.string.wifi_status_network_cb_timeout));
                return false;
            }
            mListener.onTestMsgReceived(
                    mContext.getString(R.string.wifi_status_network_unavailable));
            // All done!
            return true;
        }
        mListener.onTestMsgReceived(
                mContext.getString(R.string.wifi_status_network_wait_for_available));
        if (DBG) Log.v(TAG, "Waiting for network available callback");
        Pair<Boolean, Network> cbStatusForAvailable = mNetworkCallback.waitForAvailable();
        if (!cbStatusForAvailable.first) {
            Log.e(TAG, "Failed to get network available callback");
            setFailureReason(mContext.getString(R.string.wifi_status_network_cb_timeout));
            return false;
        }
        mListener.onTestMsgReceived(
                mContext.getString(R.string.wifi_status_network_available));

        mListener.onTestMsgReceived(
                mContext.getString(R.string.wifi_status_network_wait_for_lost));
        // Step 6: Ensure we don't disconnect from the network as long as the request is alive.
        if (DBG) Log.v(TAG, "Ensuring network lost callback is not invoked");
        boolean cbStatusForLost = mNetworkCallback.waitForLost();
        if (cbStatusForLost) {
            Log.e(TAG, "Disconnected from the network even though the request is active");
            setFailureReason(mContext.getString(R.string.wifi_status_network_lost));
            return false;
        }
        // All done!
        return true;
    }

    @Override
    protected String getFailureReason() {
        synchronized (mLock) {
            return mFailureReason;
        }
    }

    @Override
    protected void setUp() {
        super.setUp();
        mConnectivityManager = ConnectivityManager.from(mContext);
    }

    @Override
    protected void tearDown() {
        if (mNetworkCallback != null) {
            mConnectivityManager.unregisterNetworkCallback(mNetworkCallback);
        }
        super.tearDown();
    }
}
