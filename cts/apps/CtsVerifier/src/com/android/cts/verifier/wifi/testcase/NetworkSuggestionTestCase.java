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

import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_METERED;
import static android.net.NetworkCapabilities.TRANSPORT_WIFI;
import static android.net.wifi.WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS;

import static com.android.cts.verifier.wifi.TestUtils.SCAN_RESULT_TYPE_OPEN;
import static com.android.cts.verifier.wifi.TestUtils.SCAN_RESULT_TYPE_PSK;

import android.annotation.NonNull;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.MacAddress;
import android.net.Network;
import android.net.NetworkRequest;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiNetworkSuggestion;
import android.os.SystemClock;
import android.util.Log;
import android.util.Pair;

import com.android.cts.verifier.R;
import com.android.cts.verifier.wifi.BaseTestCase;
import com.android.cts.verifier.wifi.CallbackUtils;
import com.android.cts.verifier.wifi.TestUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

/**
 * Test cases for network suggestions {@link WifiNetworkSuggestion} added via
 * {@link WifiManager#addNetworkSuggestions(List)}.
 */
public class NetworkSuggestionTestCase extends BaseTestCase {
    private static final String TAG = "NetworkSuggestionTestCase";
    private static final boolean DBG = true;

    private static final int PERIODIC_SCAN_INTERVAL_MS = 10_000;
    private static final int CALLBACK_TIMEOUT_MS = 40_000;
    private static final int CAPABILITIES_CHANGED_FOR_METERED_TIMEOUT_MS = 80_000;

    private final Object mLock = new Object();
    private final ScheduledExecutorService mExecutorService;
    private final WifiNetworkSuggestion.Builder mNetworkSuggestionBuilder =
            new WifiNetworkSuggestion.Builder();

    private ConnectivityManager mConnectivityManager;
    private List<WifiNetworkSuggestion> mNetworkSuggestions;
    private NetworkRequest mNetworkRequest;
    private CallbackUtils.NetworkCallback mNetworkCallback;
    private ConnectionStatusListener mConnectionStatusListener;
    private BroadcastReceiver mBroadcastReceiver;
    private String mFailureReason;

    private final boolean mSetBssid;
    private final boolean mSetRequiresAppInteraction;
    private final boolean mSimulateConnectionFailure;
    private final boolean mSetMeteredPostConnection;

    public NetworkSuggestionTestCase(Context context, boolean setBssid,
            boolean setRequiresAppInteraction) {
        this(context, setBssid, setRequiresAppInteraction, false);
    }

    public NetworkSuggestionTestCase(Context context, boolean setBssid,
            boolean setRequiresAppInteraction, boolean simulateConnectionFailure) {
        this(context, setBssid, setRequiresAppInteraction, simulateConnectionFailure, false);
    }

    public NetworkSuggestionTestCase(Context context, boolean setBssid,
            boolean setRequiresAppInteraction, boolean simulateConnectionFailure,
            boolean setMeteredPostConnection) {
        super(context);
        mExecutorService = Executors.newSingleThreadScheduledExecutor();
        mSetBssid = setBssid;
        mSetRequiresAppInteraction = setRequiresAppInteraction;
        mSimulateConnectionFailure = simulateConnectionFailure;
        mSetMeteredPostConnection = setMeteredPostConnection;
    }

    // Create a network specifier based on the test type.
    private WifiNetworkSuggestion createNetworkSuggestion(@NonNull ScanResult scanResult) {
        mNetworkSuggestionBuilder.setSsid(scanResult.SSID);
        if (mSetBssid) {
            mNetworkSuggestionBuilder.setBssid(MacAddress.fromString(scanResult.BSSID));
        }
        if (mSetRequiresAppInteraction) {
            mNetworkSuggestionBuilder.setIsAppInteractionRequired(true);
        }
        if (mSimulateConnectionFailure) {
            // Use a random password to simulate connection failure.
            if (TestUtils.isScanResultForWpa2Network(scanResult)) {
                mNetworkSuggestionBuilder.setWpa2Passphrase(mTestUtils.generateRandomPassphrase());
            } else if (TestUtils.isScanResultForWpa3Network(scanResult)) {
                mNetworkSuggestionBuilder.setWpa3Passphrase(mTestUtils.generateRandomPassphrase());
            }
        } else if (!mPsk.isEmpty()) {
            if (TestUtils.isScanResultForWpa2Network(scanResult)) {
                mNetworkSuggestionBuilder.setWpa2Passphrase(mPsk);
            } else if (TestUtils.isScanResultForWpa3Network(scanResult)) {
                mNetworkSuggestionBuilder.setWpa3Passphrase(mPsk);
            }
        }
        mNetworkSuggestionBuilder.setIsMetered(false);
        return mNetworkSuggestionBuilder.build();
    }

    private void setFailureReason(String reason) {
        synchronized (mLock) {
            mFailureReason = reason;
        }
    }

    private static class ConnectionStatusListener implements
            WifiManager.SuggestionConnectionStatusListener {
        private final CountDownLatch mCountDownLatch;
        public WifiNetworkSuggestion wifiNetworkSuggestion = null;
        public int failureReason = -1;

        ConnectionStatusListener(CountDownLatch countDownLatch) {
            mCountDownLatch = countDownLatch;
        }

        @Override
        public void onConnectionStatus(
                WifiNetworkSuggestion wifiNetworkSuggestion, int failureReason) {
            this.wifiNetworkSuggestion = wifiNetworkSuggestion;
            this.failureReason = failureReason;
            mCountDownLatch.countDown();
        }
    }

    // TODO(b/150890482): Capabilities changed callback can occur multiple times (for ex: RSSI
    // change) & the sufficiency checks may result in ths change taking longer to take effect.
    // This method accounts for both of these situations.
    private boolean waitForNetworkToBeMetered() throws InterruptedException {
        long startTimeMillis = SystemClock.elapsedRealtime();
        while (SystemClock.elapsedRealtime()
                < startTimeMillis + CAPABILITIES_CHANGED_FOR_METERED_TIMEOUT_MS) {
            // Network marked metered.
            if (!mNetworkCallback.getNetworkCapabilities()
                    .hasCapability(NET_CAPABILITY_NOT_METERED)) {
                return true;
            } else {
                Log.w(TAG, "Network meteredness check failed. "
                        + mNetworkCallback.getNetworkCapabilities());
            }
            // Wait for the suggestion to be marked metered now.
            if (!mNetworkCallback.waitForCapabilitiesChanged()) {
                Log.w(TAG, "Network capabilities did not change");
            }
        }
        return false;
    }

    @Override
    protected boolean executeTest() throws InterruptedException {
        if (mSimulateConnectionFailure && mPsk.isEmpty()) {
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

        // Step (Optional): Register for the post connection broadcast.
        final CountDownLatch countDownLatchForPostConnectionBcast = new CountDownLatch(1);
        IntentFilter intentFilter =
                new IntentFilter(WifiManager.ACTION_WIFI_NETWORK_SUGGESTION_POST_CONNECTION);
        // Post connection broadcast receiver.
        mBroadcastReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (DBG) Log.v(TAG, "Broadcast onReceive " + intent);
                if (!intent.getAction().equals(
                        WifiManager.ACTION_WIFI_NETWORK_SUGGESTION_POST_CONNECTION)) {
                    return;
                }
                if (DBG) Log.v(TAG, "Post connection broadcast received");
                countDownLatchForPostConnectionBcast.countDown();
            }
        };
        // Register the receiver for post connection broadcast.
        mContext.registerReceiver(mBroadcastReceiver, intentFilter);
        final CountDownLatch countDownLatchForConnectionStatusListener = new CountDownLatch(1);
        mConnectionStatusListener =
                new ConnectionStatusListener(countDownLatchForConnectionStatusListener);
        mWifiManager.addSuggestionConnectionStatusListener(
                Executors.newSingleThreadExecutor(), mConnectionStatusListener);

        // Step: Register network callback to wait for connection state.
        mNetworkRequest = new NetworkRequest.Builder()
                .addTransportType(TRANSPORT_WIFI)
                .build();
        mNetworkCallback = new CallbackUtils.NetworkCallback(CALLBACK_TIMEOUT_MS);
        mConnectivityManager.registerNetworkCallback(mNetworkRequest, mNetworkCallback);

        // Step: Create a suggestion for the chosen open network depending on the type of test.
        WifiNetworkSuggestion networkSuggestion = createNetworkSuggestion(testNetwork);
        mNetworkSuggestions = Arrays.asList(networkSuggestion);

        // Step: Add a network suggestions.
        if (DBG) Log.v(TAG, "Adding suggestion");
        mListener.onTestMsgReceived(mContext.getString(R.string.wifi_status_suggestion_add));
        if (mWifiManager.addNetworkSuggestions(mNetworkSuggestions)
                != STATUS_NETWORK_SUGGESTIONS_SUCCESS) {
            setFailureReason(mContext.getString(R.string.wifi_status_suggestion_add_failure));
            return false;
        }
        if (DBG) Log.v(TAG, "Getting suggestion");
        List<WifiNetworkSuggestion> retrievedSuggestions = mWifiManager.getNetworkSuggestions();
        if (!Objects.equals(mNetworkSuggestions, retrievedSuggestions)) {
            setFailureReason(mContext.getString(R.string.wifi_status_suggestion_get_failure));
            return false;
        }

        // Step: Trigger scans periodically to trigger network selection quicker.
        if (DBG) Log.v(TAG, "Triggering scan periodically");
        mExecutorService.scheduleAtFixedRate(() -> {
            if (!mWifiManager.startScan()) {
                Log.w(TAG, "Failed to trigger scan");
            }
        }, 0, PERIODIC_SCAN_INTERVAL_MS, TimeUnit.MILLISECONDS);

        // Step: Wait for connection/unavailable.
        if (!mSimulateConnectionFailure) {
            if (DBG) Log.v(TAG, "Waiting for connection");
            mListener.onTestMsgReceived(mContext.getString(
                    R.string.wifi_status_suggestion_wait_for_connect));
            Pair<Boolean, Network> cbStatusForAvailable = mNetworkCallback.waitForAvailable();
            if (!cbStatusForAvailable.first) {
                Log.e(TAG, "Failed to get network available callback");
                setFailureReason(mContext.getString(R.string.wifi_status_network_cb_timeout));
                return false;
            }
            mListener.onTestMsgReceived(
                    mContext.getString(R.string.wifi_status_suggestion_connect));
        } else {
            if (DBG) Log.v(TAG, "Ensure no connection");
            mListener.onTestMsgReceived(mContext.getString(
                    R.string.wifi_status_suggestion_ensure_no_connect));
            Pair<Boolean, Network> cbStatusForAvailable = mNetworkCallback.waitForAvailable();
            if (cbStatusForAvailable.first) {
                Log.e(TAG, "Unexpectedly got network available callback");
                setFailureReason(mContext.getString(R.string.wifi_status_network_available_error));
                return false;
            }
            mListener.onTestMsgReceived(
                    mContext.getString(R.string.wifi_status_suggestion_not_connected));
        }

        // Step: Ensure that we connected to the suggested network (optionally, the correct BSSID).
        if (!mSimulateConnectionFailure) {
            if (!mTestUtils.isConnected("\"" + testNetwork.SSID + "\"",
                    // TODO: This might fail if there are other BSSID's for the same network & the
                    //  device decided to connect/roam to a different BSSID. We don't turn off
                    //  roaming for suggestions.
                    mSetBssid ? testNetwork.BSSID : null)) {
                Log.e(TAG, "Failed to connected to the network");
                setFailureReason(
                        mContext.getString(R.string.wifi_status_connected_to_other_network));
                return false;
            }
        }

        // Step (Optional): Ensure we received the post connect broadcast.
        if (mSetRequiresAppInteraction) {
            if (DBG) Log.v(TAG, "Wait for post connection broadcast");
            mListener.onTestMsgReceived(
                    mContext.getString(
                            R.string.wifi_status_suggestion_wait_for_post_connect_bcast));
            if (!countDownLatchForPostConnectionBcast.await(
                    CALLBACK_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                Log.e(TAG, "Failed to get post connection broadcast");
                setFailureReason(mContext.getString(
                        R.string.wifi_status_suggestion_post_connect_bcast_failure));
                return false;
            }
            mListener.onTestMsgReceived(
                    mContext.getString(R.string.wifi_status_suggestion_post_connect_bcast));
        }
        // Step (Optional): Ensure we received the connection status listener.
        if (mSimulateConnectionFailure) {
            if (DBG) Log.v(TAG, "Wait for connection status listener");
            mListener.onTestMsgReceived(
                    mContext.getString(
                            R.string.wifi_status_suggestion_wait_for_connection_status));
            if (!countDownLatchForConnectionStatusListener.await(
                    CALLBACK_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                Log.e(TAG, "Failed to receive connection status");
                setFailureReason(mContext.getString(
                        R.string.wifi_status_suggestion_connection_status_failure));
                return false;
            }
            if (DBG) Log.v(TAG, "Received connection status");
            if (!Objects.equals(mConnectionStatusListener.wifiNetworkSuggestion, networkSuggestion)
                    || mConnectionStatusListener.failureReason
                    != WifiManager.STATUS_SUGGESTION_CONNECTION_FAILURE_AUTHENTICATION) {
                Log.e(TAG, "Received wrong connection status for "
                        + mConnectionStatusListener.wifiNetworkSuggestion
                        + " with reason: " + mConnectionStatusListener.failureReason);
                setFailureReason(mContext.getString(
                        R.string.wifi_status_suggestion_connection_status_failure));
                return false;
            }
            mListener.onTestMsgReceived(
                    mContext.getString(R.string.wifi_status_suggestion_connection_status));
        }

        if (mSetMeteredPostConnection) {
            // ensure that the network is not metered before change.
            if (!mNetworkCallback.getNetworkCapabilities()
                    .hasCapability(NET_CAPABILITY_NOT_METERED)) {
                Log.e(TAG, "Network meteredness check failed "
                        + mNetworkCallback.getNetworkCapabilities());
                setFailureReason(mContext.getString(
                        R.string.wifi_status_suggestion_metered_check_failed));
                return false;
            }
            if (DBG) Log.v(TAG, "Mark suggestion metered after connection");
            mListener.onTestMsgReceived(
                    mContext.getString(R.string.wifi_status_suggestion_metered_change));
            WifiNetworkSuggestion modifiedSuggestion = mNetworkSuggestionBuilder
                    .setIsMetered(true)
                    .build();
            if (mWifiManager.addNetworkSuggestions(Arrays.asList(modifiedSuggestion))
                    != STATUS_NETWORK_SUGGESTIONS_SUCCESS) {
                setFailureReason(mContext.getString(R.string.wifi_status_suggestion_add_failure));
                return false;
            }
            if (!waitForNetworkToBeMetered()) {
                Log.e(TAG, "Network was not marked metered");
                setFailureReason(mContext.getString(
                        R.string.wifi_status_suggestion_metered_check_failed));
                return false;
            }
            mListener.onTestMsgReceived(
                    mContext.getString(R.string.wifi_status_suggestion_metered_changed));
        }

        // Step: Remove the suggestions from the app.
        if (DBG) Log.v(TAG, "Removing suggestion");
        mListener.onTestMsgReceived(mContext.getString(R.string.wifi_status_suggestion_remove));
        if (mWifiManager.removeNetworkSuggestions(mNetworkSuggestions)
                != STATUS_NETWORK_SUGGESTIONS_SUCCESS) {
            setFailureReason(mContext.getString(R.string.wifi_status_suggestion_remove_failure));
            return false;
        }

        // Step: Ensure we disconnect immediately on suggestion removal.
        if (!mSimulateConnectionFailure) {
            mListener.onTestMsgReceived(
                    mContext.getString(R.string.wifi_status_suggestion_wait_for_disconnect));
            if (DBG) Log.v(TAG, "Ensuring we disconnect immediately");
            boolean cbStatusForLost = mNetworkCallback.waitForLost();
            if (!cbStatusForLost) {
                setFailureReason(
                        mContext.getString(R.string.wifi_status_suggestion_not_disconnected));
                return false;
            }
            mListener.onTestMsgReceived(
                    mContext.getString(R.string.wifi_status_suggestion_disconnected));
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
        mExecutorService.shutdownNow();
        if (mBroadcastReceiver != null) {
            mContext.unregisterReceiver(mBroadcastReceiver);
        }
        if (mConnectionStatusListener != null) {
            mWifiManager.removeSuggestionConnectionStatusListener(mConnectionStatusListener);
        }
        mWifiManager.removeNetworkSuggestions(new ArrayList<>());
        super.tearDown();
    }
}
