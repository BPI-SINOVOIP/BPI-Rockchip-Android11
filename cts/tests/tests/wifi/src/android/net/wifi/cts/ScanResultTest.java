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
import static com.google.common.truth.Truth.assertWithMessage;

import java.nio.ByteBuffer;
import java.util.List;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.wifi.ScanResult;
import android.net.wifi.ScanResult.InformationElement;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.WifiLock;
import android.platform.test.annotations.AppModeFull;
import android.test.AndroidTestCase;

import com.android.compatibility.common.util.PollingCheck;
import com.android.compatibility.common.util.ShellIdentityUtils;
import com.android.compatibility.common.util.SystemUtil;

@AppModeFull(reason = "Cannot get WifiManager in instant app mode")
public class ScanResultTest extends WifiJUnit3TestBase {
    private static class MySync {
        int expectedState = STATE_NULL;
    }

    private WifiManager mWifiManager;
    private WifiLock mWifiLock;
    private static MySync mMySync;
    private boolean mWasVerboseLoggingEnabled;
    private boolean mWasScanThrottleEnabled;

    private static final int STATE_NULL = 0;
    private static final int STATE_WIFI_CHANGING = 1;
    private static final int STATE_WIFI_CHANGED = 2;
    private static final int STATE_START_SCAN = 3;
    private static final int STATE_SCAN_RESULTS_AVAILABLE = 4;
    private static final int STATE_SCAN_FAILURE = 5;

    private static final String TAG = "WifiInfoTest";
    private static final int TIMEOUT_MSEC = 6000;
    private static final int WAIT_MSEC = 60;
    private static final int ENABLE_WAIT_MSEC = 10000;
    private static final int SCAN_WAIT_MSEC = 10000;
    private static final int SCAN_MAX_RETRY_COUNT = 6;
    private static final int SCAN_FIND_BSSID_MAX_RETRY_COUNT = 5;
    private static final long SCAN_FIND_BSSID_WAIT_MSEC = 5_000L;
    private static final int WIFI_CONNECT_TIMEOUT_MILLIS = 30_000;

    private static final String TEST_SSID = "TEST_SSID";
    public static final String TEST_BSSID = "04:ac:fe:45:34:10";
    public static final String TEST_CAPS = "CCMP";
    public static final int TEST_LEVEL = -56;
    public static final int TEST_FREQUENCY = 2412;
    public static final long TEST_TIMESTAMP = 4660L;

    private IntentFilter mIntentFilter;
    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();
            if (action.equals(WifiManager.SUPPLICANT_STATE_CHANGED_ACTION)) {
                synchronized (mMySync) {
                    mMySync.expectedState = STATE_WIFI_CHANGED;
                    mMySync.notify();
                }
            } else if (action.equals(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION)) {
                synchronized (mMySync) {
                    if (intent.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false)) {
                        mMySync.expectedState = STATE_SCAN_RESULTS_AVAILABLE;
                    } else {
                        mMySync.expectedState = STATE_SCAN_FAILURE;
                    }
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
        mIntentFilter.addAction(WifiManager.NETWORK_STATE_CHANGED_ACTION);
        mIntentFilter.addAction(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION);
        mIntentFilter.addAction(WifiManager.SUPPLICANT_CONNECTION_CHANGE_ACTION);
        mIntentFilter.addAction(WifiManager.SUPPLICANT_STATE_CHANGED_ACTION);
        mIntentFilter.addAction(WifiManager.WIFI_STATE_CHANGED_ACTION);
        mIntentFilter.addAction(WifiManager.RSSI_CHANGED_ACTION);
        mIntentFilter.addAction(WifiManager.NETWORK_IDS_CHANGED_ACTION);
        mIntentFilter.addAction(WifiManager.ACTION_PICK_WIFI_NETWORK);

        mContext.registerReceiver(mReceiver, mIntentFilter);
        mWifiManager = (WifiManager) getContext().getSystemService(Context.WIFI_SERVICE);
        assertThat(mWifiManager).isNotNull();

        // turn on verbose logging for tests
        mWasVerboseLoggingEnabled = ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.isVerboseLoggingEnabled());
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.setVerboseLoggingEnabled(true));
        // Disable scan throttling for tests.
        mWasScanThrottleEnabled = ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.isScanThrottleEnabled());
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.setScanThrottleEnabled(false));

        mWifiLock = mWifiManager.createWifiLock(TAG);
        mWifiLock.acquire();

        // enable Wifi
        if (!mWifiManager.isWifiEnabled()) setWifiEnabled(true);
        PollingCheck.check("Wifi not enabled", ENABLE_WAIT_MSEC,
                () -> mWifiManager.isWifiEnabled());

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
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.setScanThrottleEnabled(mWasScanThrottleEnabled));
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.setVerboseLoggingEnabled(mWasVerboseLoggingEnabled));
        Thread.sleep(ENABLE_WAIT_MSEC);
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
            waitForBroadcast(TIMEOUT_MSEC, STATE_WIFI_CHANGED);
       }
    }

    private boolean waitForBroadcast(long timeout, int expectedState) throws Exception {
        long waitTime = System.currentTimeMillis() + timeout;
        while (System.currentTimeMillis() < waitTime
                && mMySync.expectedState != expectedState)
            mMySync.wait(WAIT_MSEC);
        return mMySync.expectedState == expectedState;
    }

    public void testScanResultProperties() {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        // this test case should in Wifi environment
        for (ScanResult scanResult : mWifiManager.getScanResults()) {
            assertThat(scanResult.toString()).isNotNull();

            for (InformationElement ie : scanResult.getInformationElements()) {
                testInformationElementCopyConstructor(ie);
                testInformationElementFields(ie);
            }

            assertThat(scanResult.getWifiStandard()).isAnyOf(
                    ScanResult.WIFI_STANDARD_UNKNOWN,
                    ScanResult.WIFI_STANDARD_LEGACY,
                    ScanResult.WIFI_STANDARD_11N,
                    ScanResult.WIFI_STANDARD_11AC,
                    ScanResult.WIFI_STANDARD_11AX
            );

            scanResult.isPasspointNetwork();
        }
    }

    private void testInformationElementCopyConstructor(InformationElement ie) {
        InformationElement copy = new InformationElement(ie);

        assertThat(copy.getId()).isEqualTo(ie.getId());
        assertThat(copy.getIdExt()).isEqualTo(ie.getIdExt());
        assertThat(copy.getBytes()).isEqualTo(ie.getBytes());
    }

    private void testInformationElementFields(InformationElement ie) {
        // id is 1 octet
        int id = ie.getId();
        assertThat(id).isAtLeast(0);
        assertThat(id).isAtMost(255);

        // idExt is 0 or 1 octet
        int idExt = ie.getIdExt();
        assertThat(idExt).isAtLeast(0);
        assertThat(idExt).isAtMost(255);

        ByteBuffer bytes = ie.getBytes();
        assertThat(bytes).isNotNull();
    }

    /* Multiple scans to ensure bssid is updated */
    private void scanAndWait() throws Exception {
        synchronized (mMySync) {
            for (int retry  = 0; retry < SCAN_MAX_RETRY_COUNT; retry++) {
                mMySync.expectedState = STATE_START_SCAN;
                mWifiManager.startScan();
                if (waitForBroadcast(SCAN_WAIT_MSEC, STATE_SCAN_RESULTS_AVAILABLE)) {
                    break;
                }
            }
        }
   }

    public void testScanResultTimeStamp() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }

        long timestamp = 0;
        String BSSID = null;

        scanAndWait();

        List<ScanResult> scanResults = mWifiManager.getScanResults();
        for (ScanResult result : scanResults) {
            BSSID = result.BSSID;
            timestamp = result.timestamp;
            assertThat(timestamp).isNotEqualTo(0);
            break;
        }

        scanAndWait();

        scanResults = mWifiManager.getScanResults();
        for (ScanResult result : scanResults) {
            if (result.BSSID.equals(BSSID)) {
                long timeDiff = (result.timestamp - timestamp) / 1000;
                assertThat(timeDiff).isGreaterThan(0L);
                assertThat(timeDiff).isLessThan(6L * SCAN_WAIT_MSEC);
            }
        }
    }

    /** Test that the copy constructor copies fields correctly. */
    public void testScanResultConstructors() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }

        ScanResult scanResult = new ScanResult();
        scanResult.SSID = TEST_SSID;
        scanResult.BSSID = TEST_BSSID;
        scanResult.capabilities = TEST_CAPS;
        scanResult.level = TEST_LEVEL;
        scanResult.frequency = TEST_FREQUENCY;
        scanResult.timestamp = TEST_TIMESTAMP;

        ScanResult scanResult2 = new ScanResult(scanResult);
        assertThat(scanResult2.SSID).isEqualTo(TEST_SSID);
        assertThat(scanResult2.BSSID).isEqualTo(TEST_BSSID);
        assertThat(scanResult2.capabilities).isEqualTo(TEST_CAPS);
        assertThat(scanResult2.level).isEqualTo(TEST_LEVEL);
        assertThat(scanResult2.frequency).isEqualTo(TEST_FREQUENCY);
        assertThat(scanResult2.timestamp).isEqualTo(TEST_TIMESTAMP);
    }

    public void testScanResultMatchesWifiInfo() throws Exception {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }

        // ensure Wifi is connected
        ShellIdentityUtils.invokeWithShellPermissions(() -> mWifiManager.reconnect());
        PollingCheck.check(
                "Wifi not connected",
                WIFI_CONNECT_TIMEOUT_MILLIS,
                () -> mWifiManager.getConnectionInfo().getNetworkId() != -1);

        final WifiInfo wifiInfo = mWifiManager.getConnectionInfo();
        assertThat(wifiInfo).isNotNull();

        ScanResult currentNetwork = null;
        for (int i = 0; i < SCAN_FIND_BSSID_MAX_RETRY_COUNT; i++) {
            scanAndWait();
            final List<ScanResult> scanResults = mWifiManager.getScanResults();
            currentNetwork = scanResults.stream().filter(r -> r.BSSID.equals(wifiInfo.getBSSID()))
                    .findAny().orElse(null);

            if (currentNetwork != null) {
                break;
            }
            Thread.sleep(SCAN_FIND_BSSID_WAIT_MSEC);
        }
        assertWithMessage("Current network not found in scan results")
                .that(currentNetwork).isNotNull();

        String wifiInfoSsidQuoted = wifiInfo.getSSID();
        String scanResultSsidUnquoted = currentNetwork.SSID;

        assertWithMessage(
                "SSID mismatch: make sure this isn't a hidden network or an SSID containing "
                        + "non-UTF-8 characters - neither is supported by this CTS test.")
                .that("\"" + scanResultSsidUnquoted + "\"")
                .isEqualTo(wifiInfoSsidQuoted);
        assertThat(currentNetwork.frequency).isEqualTo(wifiInfo.getFrequency());
    }
}
