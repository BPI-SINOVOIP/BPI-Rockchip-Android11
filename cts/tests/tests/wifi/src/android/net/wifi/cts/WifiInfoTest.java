/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.net.wifi.cts;

import static com.google.common.truth.Truth.assertThat;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.wifi.ScanResult;
import android.net.wifi.SupplicantState;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.WifiLock;
import android.platform.test.annotations.AppModeFull;
import android.test.AndroidTestCase;

import com.android.compatibility.common.util.PollingCheck;
import com.android.compatibility.common.util.ShellIdentityUtils;
import com.android.compatibility.common.util.SystemUtil;

import java.nio.charset.StandardCharsets;
import java.util.concurrent.Callable;

@AppModeFull(reason = "Cannot get WifiManager in instant app mode")
public class WifiInfoTest extends WifiJUnit3TestBase {
    private static class MySync {
        int expectedState = STATE_NULL;
    }

    private WifiManager mWifiManager;
    private WifiLock mWifiLock;
    private static MySync mMySync;

    private static final int STATE_NULL = 0;
    private static final int STATE_WIFI_CHANGING = 1;
    private static final int STATE_WIFI_CHANGED = 2;

    private static final String TEST_SSID = "Test123";
    private static final String TEST_BSSID = "12:12:12:12:12:12";
    private static final int TEST_RSSI = -60;
    private static final int TEST_NETWORK_ID = 5;
    private static final int TEST_NETWORK_ID2 = 6;

    private static final String TAG = "WifiInfoTest";
    private static final int TIMEOUT_MSEC = 6000;
    private static final int WAIT_MSEC = 60;
    private static final int DURATION = 10000;
    private static final int WIFI_CONNECT_TIMEOUT_MILLIS = 30_000;
    private IntentFilter mIntentFilter;
    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();
            if (action.equals(WifiManager.WIFI_STATE_CHANGED_ACTION)) {
                synchronized (mMySync) {
                    mMySync.expectedState = STATE_WIFI_CHANGED;
                    mMySync.notify();
                }
            }
        }
    };

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        mMySync = new MySync();
        mIntentFilter = new IntentFilter();
        mIntentFilter.addAction(WifiManager.WIFI_STATE_CHANGED_ACTION);

        mContext.registerReceiver(mReceiver, mIntentFilter);
        mWifiManager = (WifiManager) getContext().getSystemService(Context.WIFI_SERVICE);
        assertThat(mWifiManager).isNotNull();
        mWifiLock = mWifiManager.createWifiLock(TAG);
        mWifiLock.acquire();

        // enable Wifi
        if (!mWifiManager.isWifiEnabled()) setWifiEnabled(true);
        PollingCheck.check("Wifi not enabled", DURATION, () -> mWifiManager.isWifiEnabled());

        mMySync.expectedState = STATE_NULL;
    }

    @Override
    protected void tearDown() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            super.tearDown();
            return;
        }
        mWifiLock.release();
        mContext.unregisterReceiver(mReceiver);
        if (!mWifiManager.isWifiEnabled())
            setWifiEnabled(true);
        Thread.sleep(DURATION);
        super.tearDown();
    }

    private void setWifiEnabled(boolean enable) throws Exception {
        synchronized (mMySync) {
            mMySync.expectedState = STATE_WIFI_CHANGING;
            if (enable) {
                SystemUtil.runShellCommand("svc wifi enable");
            } else {
                SystemUtil.runShellCommand("svc wifi disable");
            }
            long timeout = System.currentTimeMillis() + TIMEOUT_MSEC;
            while (System.currentTimeMillis() < timeout
                    && mMySync.expectedState == STATE_WIFI_CHANGING)
                mMySync.wait(WAIT_MSEC);
        }
    }

    public void testWifiInfoProperties() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }

        // ensure Wifi is connected
        ShellIdentityUtils.invokeWithShellPermissions(() -> mWifiManager.reconnect());
        PollingCheck.check(
                "Wifi not connected - Please ensure there is a saved network in range of this "
                        + "device",
                WIFI_CONNECT_TIMEOUT_MILLIS,
                () -> mWifiManager.getConnectionInfo().getNetworkId() != -1);

        // this test case should in Wifi environment
        WifiInfo wifiInfo = mWifiManager.getConnectionInfo();

        testWifiInfoPropertiesWhileConnected(wifiInfo);

        setWifiEnabled(false);

        PollingCheck.check("getNetworkId not -1", 20000,
                () -> mWifiManager.getConnectionInfo().getNetworkId() == -1);

        PollingCheck.check("getWifiState not disabled", 20000,
                () -> mWifiManager.getWifiState() == WifiManager.WIFI_STATE_DISABLED);
    }

    private void testWifiInfoPropertiesWhileConnected(WifiInfo wifiInfo) {
        assertThat(wifiInfo).isNotNull();
        assertThat(wifiInfo.toString()).isNotNull();
        SupplicantState.isValidState(wifiInfo.getSupplicantState());
        WifiInfo.getDetailedStateOf(SupplicantState.DISCONNECTED);
        String ssid = wifiInfo.getSSID();
        if (!ssid.startsWith("0x") && !ssid.equals(WifiManager.UNKNOWN_SSID)) {
            // Non-hex string should be quoted
            assertThat(ssid).startsWith("\"");
            assertThat(ssid).endsWith("\"");
        }

        assertThat(wifiInfo.getBSSID()).isNotNull();
        assertThat(wifiInfo.getFrequency()).isGreaterThan(0);
        assertThat(wifiInfo.getMacAddress()).isNotNull();

        wifiInfo.getRssi();
        wifiInfo.getIpAddress();
        wifiInfo.getHiddenSSID();
        wifiInfo.getScore();

        // null for saved networks
        assertThat(wifiInfo.getRequestingPackageName()).isNull();
        assertThat(wifiInfo.getPasspointFqdn()).isNull();
        assertThat(wifiInfo.getPasspointProviderFriendlyName()).isNull();

        // false for saved networks
        assertThat(wifiInfo.isEphemeral()).isFalse();
        assertThat(wifiInfo.isOsuAp()).isFalse();
        assertThat(wifiInfo.isPasspointAp()).isFalse();

        assertThat(wifiInfo.getWifiStandard()).isAnyOf(
                ScanResult.WIFI_STANDARD_UNKNOWN,
                ScanResult.WIFI_STANDARD_LEGACY,
                ScanResult.WIFI_STANDARD_11N,
                ScanResult.WIFI_STANDARD_11AC,
                ScanResult.WIFI_STANDARD_11AX
        );

        assertThat(wifiInfo.getLostTxPacketsPerSecond()).isAtLeast(0.0);
        assertThat(wifiInfo.getRetriedTxPacketsPerSecond()).isAtLeast(0.0);
        assertThat(wifiInfo.getSuccessfulRxPacketsPerSecond()).isAtLeast(0.0);
        assertThat(wifiInfo.getSuccessfulTxPacketsPerSecond()).isAtLeast(0.0);

        // Can be -1 if link speed is unknown
        assertThat(wifiInfo.getLinkSpeed()).isAtLeast(-1);
        assertThat(wifiInfo.getTxLinkSpeedMbps()).isAtLeast(-1);
        assertThat(wifiInfo.getRxLinkSpeedMbps()).isAtLeast(-1);
        assertThat(wifiInfo.getMaxSupportedTxLinkSpeedMbps()).isAtLeast(-1);
        assertThat(wifiInfo.getMaxSupportedRxLinkSpeedMbps()).isAtLeast(-1);
    }

    /**
     * Test that the WifiInfo Builder returns the same values that was set, and that
     * calling build multiple times returns different instances.
     */
    public void testWifiInfoBuilder() throws Exception {
        WifiInfo.Builder builder = new WifiInfo.Builder()
                .setSsid(TEST_SSID.getBytes(StandardCharsets.UTF_8))
                .setBssid(TEST_BSSID)
                .setRssi(TEST_RSSI)
                .setNetworkId(TEST_NETWORK_ID);

        WifiInfo info1 = builder.build();

        assertThat(info1.getSSID()).isEqualTo("\"" + TEST_SSID + "\"");
        assertThat(info1.getBSSID()).isEqualTo(TEST_BSSID);
        assertThat(info1.getRssi()).isEqualTo(TEST_RSSI);
        assertThat(info1.getNetworkId()).isEqualTo(TEST_NETWORK_ID);

        WifiInfo info2 = builder
                .setNetworkId(TEST_NETWORK_ID2)
                .build();

        // different instances
        assertThat(info1).isNotSameAs(info2);

        // assert that info1 didn't change
        assertThat(info1.getSSID()).isEqualTo("\"" + TEST_SSID + "\"");
        assertThat(info1.getBSSID()).isEqualTo(TEST_BSSID);
        assertThat(info1.getRssi()).isEqualTo(TEST_RSSI);
        assertThat(info1.getNetworkId()).isEqualTo(TEST_NETWORK_ID);

        // assert that info2 changed
        assertThat(info2.getSSID()).isEqualTo("\"" + TEST_SSID + "\"");
        assertThat(info2.getBSSID()).isEqualTo(TEST_BSSID);
        assertThat(info2.getRssi()).isEqualTo(TEST_RSSI);
        assertThat(info2.getNetworkId()).isEqualTo(TEST_NETWORK_ID2);
    }
}
