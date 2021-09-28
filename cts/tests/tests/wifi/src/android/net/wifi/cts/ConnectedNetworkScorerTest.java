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

package android.net.wifi.cts;

import static android.net.wifi.WifiUsabilityStatsEntry.PROBE_STATUS_FAILURE;
import static android.net.wifi.WifiUsabilityStatsEntry.PROBE_STATUS_NO_PROBE;
import static android.net.wifi.WifiUsabilityStatsEntry.PROBE_STATUS_SUCCESS;
import static android.net.wifi.WifiUsabilityStatsEntry.PROBE_STATUS_UNKNOWN;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assume.assumeTrue;

import android.app.UiAutomation;
import android.content.Context;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiUsabilityStatsEntry;
import android.os.Build;
import android.platform.test.annotations.AppModeFull;
import android.support.test.uiautomator.UiDevice;
import android.telephony.TelephonyManager;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.PollingCheck;
import com.android.compatibility.common.util.PropertyUtil;
import com.android.compatibility.common.util.ShellIdentityUtils;
import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/**
 * Tests for wifi connected network scorer interface and usability stats.
 */
@AppModeFull(reason = "Cannot get WifiManager in instant app mode")
@SmallTest
@RunWith(AndroidJUnit4.class)
public class ConnectedNetworkScorerTest extends WifiJUnit4TestBase {
    private Context mContext;
    private WifiManager mWifiManager;
    private UiDevice mUiDevice;
    private boolean mWasVerboseLoggingEnabled;

    private static final int WIFI_CONNECT_TIMEOUT_MILLIS = 30_000;
    private static final int DURATION = 10_000;
    private static final int DURATION_SCREEN_TOGGLE = 2000;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getInstrumentation().getContext();

        // skip the test if WiFi is not supported
        assumeTrue(WifiFeature.isWifiSupported(mContext));

        mWifiManager = mContext.getSystemService(WifiManager.class);
        assertThat(mWifiManager).isNotNull();

        // turn on verbose logging for tests
        mWasVerboseLoggingEnabled = ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.isVerboseLoggingEnabled());
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.setVerboseLoggingEnabled(true));

        // enable Wifi
        if (!mWifiManager.isWifiEnabled()) setWifiEnabled(true);
        PollingCheck.check("Wifi not enabled", DURATION, () -> mWifiManager.isWifiEnabled());

        // turn screen on
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        turnScreenOn();

        // check we have >= 1 saved network
        List<WifiConfiguration> savedNetworks = ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.getConfiguredNetworks());
        assertWithMessage("Need at least one saved network").that(savedNetworks).isNotEmpty();

        // ensure Wifi is connected
        ShellIdentityUtils.invokeWithShellPermissions(() -> mWifiManager.reconnect());
        PollingCheck.check(
                "Wifi not connected",
                WIFI_CONNECT_TIMEOUT_MILLIS,
                () -> mWifiManager.getConnectionInfo().getNetworkId() != -1);
    }

    @After
    public void tearDown() throws Exception {
        if (!WifiFeature.isWifiSupported(mContext)) return;
        if (!mWifiManager.isWifiEnabled()) setWifiEnabled(true);
        turnScreenOff();
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.setVerboseLoggingEnabled(mWasVerboseLoggingEnabled));
    }

    private void setWifiEnabled(boolean enable) throws Exception {
        // now trigger the change using shell commands.
        SystemUtil.runShellCommand("svc wifi " + (enable ? "enable" : "disable"));
    }

    private void turnScreenOn() throws Exception {
        mUiDevice.executeShellCommand("input keyevent KEYCODE_WAKEUP");
        mUiDevice.executeShellCommand("wm dismiss-keyguard");
        // Since the screen on/off intent is ordered, they will not be sent right now.
        Thread.sleep(DURATION_SCREEN_TOGGLE);
    }

    private void turnScreenOff() throws Exception {
        mUiDevice.executeShellCommand("input keyevent KEYCODE_SLEEP");
    }

    private static class TestUsabilityStatsListener implements
            WifiManager.OnWifiUsabilityStatsListener {
        private final CountDownLatch mCountDownLatch;
        public int seqNum;
        public boolean isSameBssidAndFre;
        public WifiUsabilityStatsEntry statsEntry;

        TestUsabilityStatsListener(CountDownLatch countDownLatch) {
            mCountDownLatch = countDownLatch;
        }

        @Override
        public void onWifiUsabilityStats(int seqNum, boolean isSameBssidAndFreq,
                WifiUsabilityStatsEntry statsEntry) {
            this.seqNum = seqNum;
            this.isSameBssidAndFre = isSameBssidAndFreq;
            this.statsEntry = statsEntry;
            mCountDownLatch.countDown();
        }
    }

    /**
     * Tests the {@link android.net.wifi.WifiUsabilityStatsEntry} retrieved from
     * {@link WifiManager.OnWifiUsabilityStatsListener}.
     */
    @Test
    public void testWifiUsabilityStatsEntry() throws Exception {
        // Usability stats collection only supported by vendor version Q and above.
        if (!PropertyUtil.isVendorApiLevelAtLeast(Build.VERSION_CODES.Q)) {
            return;
        }
        CountDownLatch countDownLatch = new CountDownLatch(1);
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        TestUsabilityStatsListener usabilityStatsListener =
                new TestUsabilityStatsListener(countDownLatch);
        try {
            uiAutomation.adoptShellPermissionIdentity();
            mWifiManager.addOnWifiUsabilityStatsListener(
                    Executors.newSingleThreadExecutor(), usabilityStatsListener);
            // Wait for new usability stats (while connected & screen on this is triggered
            // by platform periodically).
            assertThat(countDownLatch.await(DURATION, TimeUnit.MILLISECONDS)).isTrue();

            assertThat(usabilityStatsListener.statsEntry).isNotNull();
            WifiUsabilityStatsEntry statsEntry = usabilityStatsListener.statsEntry;

            assertThat(statsEntry.getTimeStampMillis()).isGreaterThan(0L);
            assertThat(statsEntry.getRssi()).isLessThan(0);
            assertThat(statsEntry.getLinkSpeedMbps()).isAtLeast(0);
            assertThat(statsEntry.getTotalTxSuccess()).isAtLeast(0L);
            assertThat(statsEntry.getTotalTxRetries()).isAtLeast(0L);
            assertThat(statsEntry.getTotalTxBad()).isAtLeast(0L);
            assertThat(statsEntry.getTotalRxSuccess()).isAtLeast(0L);
            if (mWifiManager.isEnhancedPowerReportingSupported()) {
                assertThat(statsEntry.getTotalRadioOnTimeMillis()).isAtLeast(0L);
                assertThat(statsEntry.getTotalRadioTxTimeMillis()).isAtLeast(0L);
                assertThat(statsEntry.getTotalRadioRxTimeMillis()).isAtLeast(0L);
                assertThat(statsEntry.getTotalScanTimeMillis()).isAtLeast(0L);
                assertThat(statsEntry.getTotalNanScanTimeMillis()).isAtLeast(0L);
                assertThat(statsEntry.getTotalBackgroundScanTimeMillis()).isAtLeast(0L);
                assertThat(statsEntry.getTotalRoamScanTimeMillis()).isAtLeast(0L);
                assertThat(statsEntry.getTotalPnoScanTimeMillis()).isAtLeast(0L);
                assertThat(statsEntry.getTotalHotspot2ScanTimeMillis()).isAtLeast(0L);
                assertThat(statsEntry.getTotalCcaBusyFreqTimeMillis()).isAtLeast(0L);
                assertThat(statsEntry.getTotalRadioOnTimeMillis()).isAtLeast(0L);
                assertThat(statsEntry.getTotalRadioOnFreqTimeMillis()).isAtLeast(0L);
                assertThat(statsEntry.getTotalBeaconRx()).isAtLeast(0L);
                assertThat(statsEntry.getProbeStatusSinceLastUpdate())
                        .isAnyOf(PROBE_STATUS_SUCCESS,
                                PROBE_STATUS_FAILURE,
                                PROBE_STATUS_NO_PROBE,
                                PROBE_STATUS_UNKNOWN);
                // -1 is default value for some of these fields if they're not available.
                assertThat(statsEntry.getProbeElapsedTimeSinceLastUpdateMillis()).isAtLeast(-1);
                assertThat(statsEntry.getProbeMcsRateSinceLastUpdate()).isAtLeast(-1);
                assertThat(statsEntry.getRxLinkSpeedMbps()).isAtLeast(-1);
                // no longer populated, return default value.
                assertThat(statsEntry.getCellularDataNetworkType())
                        .isAnyOf(TelephonyManager.NETWORK_TYPE_UNKNOWN,
                                TelephonyManager.NETWORK_TYPE_GPRS,
                                TelephonyManager.NETWORK_TYPE_EDGE,
                                TelephonyManager.NETWORK_TYPE_UMTS,
                                TelephonyManager.NETWORK_TYPE_CDMA,
                                TelephonyManager.NETWORK_TYPE_EVDO_0,
                                TelephonyManager.NETWORK_TYPE_EVDO_A,
                                TelephonyManager.NETWORK_TYPE_1xRTT,
                                TelephonyManager.NETWORK_TYPE_HSDPA,
                                TelephonyManager.NETWORK_TYPE_HSUPA,
                                TelephonyManager.NETWORK_TYPE_HSPA,
                                TelephonyManager.NETWORK_TYPE_IDEN,
                                TelephonyManager.NETWORK_TYPE_EVDO_B,
                                TelephonyManager.NETWORK_TYPE_LTE,
                                TelephonyManager.NETWORK_TYPE_EHRPD,
                                TelephonyManager.NETWORK_TYPE_HSPAP,
                                TelephonyManager.NETWORK_TYPE_GSM,
                                TelephonyManager.NETWORK_TYPE_TD_SCDMA,
                                TelephonyManager.NETWORK_TYPE_IWLAN,
                                TelephonyManager.NETWORK_TYPE_NR);
                assertThat(statsEntry.getCellularSignalStrengthDbm()).isAtMost(0);
                assertThat(statsEntry.getCellularSignalStrengthDb()).isAtMost(0);
                assertThat(statsEntry.isSameRegisteredCell()).isFalse();
            }
        } finally {
            mWifiManager.removeOnWifiUsabilityStatsListener(usabilityStatsListener);
            uiAutomation.dropShellPermissionIdentity();
        }
    }

    /**
     * Tests the {@link android.net.wifi.WifiManager#updateWifiUsabilityScore(int, int, int)}
     */
    @Test
    public void testUpdateWifiUsabilityScore() throws Exception {
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        try {
            uiAutomation.adoptShellPermissionIdentity();
            // update scoring with dummy values.
            mWifiManager.updateWifiUsabilityScore(0, 50, 50);
        } finally {
            uiAutomation.dropShellPermissionIdentity();
        }
    }

    private static class TestConnectedNetworkScorer implements
            WifiManager.WifiConnectedNetworkScorer {
        private CountDownLatch mCountDownLatch;
        public int startSessionId;
        public int stopSessionId;
        public WifiManager.ScoreUpdateObserver scoreUpdateObserver;

        TestConnectedNetworkScorer(CountDownLatch countDownLatch) {
            mCountDownLatch = countDownLatch;
        }

        @Override
        public void onStart(int sessionId) {
            synchronized (mCountDownLatch) {
                this.startSessionId = sessionId;
                mCountDownLatch.countDown();
            }
        }

        @Override
        public void onStop(int sessionId) {
            synchronized (mCountDownLatch) {
                this.stopSessionId = sessionId;
                mCountDownLatch.countDown();
            }
        }

        @Override
        public void onSetScoreUpdateObserver(WifiManager.ScoreUpdateObserver observerImpl) {
            this.scoreUpdateObserver = observerImpl;
        }

        public void resetCountDownLatch(CountDownLatch countDownLatch) {
            synchronized (mCountDownLatch) {
                mCountDownLatch = countDownLatch;
            }
        }
    }

    /**
     * Tests the {@link android.net.wifi.WifiConnectedNetworkScorer} interface.
     *
     * Note: We could write more interesting test cases (if the device has a mobile connection), but
     * that would make the test flaky. The default network/route selection on the device is not just
     * controlled by the wifi scorer input, but also based on params which are controlled by
     * other parts of the platform (likely in connectivity service) and hence will behave
     * differently on OEM devices.
     */
    @Test
    public void testSetWifiConnectedNetworkScorer() throws Exception {
        CountDownLatch countDownLatchScorer = new CountDownLatch(1);
        CountDownLatch countDownLatchUsabilityStats = new CountDownLatch(1);
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        TestConnectedNetworkScorer connectedNetworkScorer =
                new TestConnectedNetworkScorer(countDownLatchScorer);
        TestUsabilityStatsListener usabilityStatsListener =
                new TestUsabilityStatsListener(countDownLatchUsabilityStats);
        boolean disconnected = false;
        try {
            uiAutomation.adoptShellPermissionIdentity();
            // Clear any external scorer already active on the device.
            mWifiManager.clearWifiConnectedNetworkScorer();
            Thread.sleep(500);

            mWifiManager.setWifiConnectedNetworkScorer(
                    Executors.newSingleThreadExecutor(), connectedNetworkScorer);
            // Since we're already connected, wait for onStart to be invoked.
            assertThat(countDownLatchScorer.await(DURATION, TimeUnit.MILLISECONDS)).isTrue();

            assertThat(connectedNetworkScorer.startSessionId).isAtLeast(0);
            assertThat(connectedNetworkScorer.scoreUpdateObserver).isNotNull();
            WifiManager.ScoreUpdateObserver scoreUpdateObserver =
                    connectedNetworkScorer.scoreUpdateObserver;

            // Now trigger a dummy score update.
            scoreUpdateObserver.notifyScoreUpdate(connectedNetworkScorer.startSessionId, 50);

            // Register the usability listener
            mWifiManager.addOnWifiUsabilityStatsListener(
                    Executors.newSingleThreadExecutor(), usabilityStatsListener);
            // Trigger a usability stats update.
            scoreUpdateObserver.triggerUpdateOfWifiUsabilityStats(
                    connectedNetworkScorer.startSessionId);
            // Ensure that we got the stats update callback.
            assertThat(countDownLatchUsabilityStats.await(DURATION, TimeUnit.MILLISECONDS))
                    .isTrue();
            assertThat(usabilityStatsListener.seqNum).isAtLeast(0);

            // Reset the scorer countdown latch for onStop
            countDownLatchScorer = new CountDownLatch(1);
            connectedNetworkScorer.resetCountDownLatch(countDownLatchScorer);
            // Now disconnect from the network.
            mWifiManager.disconnect();
            // Wait for it to be disconnected.
            PollingCheck.check(
                    "Wifi not disconnected",
                    DURATION,
                    () -> mWifiManager.getConnectionInfo().getNetworkId() == -1);
            disconnected = true;

            // Wait for stop to be invoked and ensure that the session id matches.
            assertThat(countDownLatchScorer.await(DURATION, TimeUnit.MILLISECONDS)).isTrue();
            assertThat(connectedNetworkScorer.stopSessionId)
                    .isEqualTo(connectedNetworkScorer.startSessionId);
        } finally {
            mWifiManager.removeOnWifiUsabilityStatsListener(usabilityStatsListener);
            mWifiManager.clearWifiConnectedNetworkScorer();

            if (disconnected) {
                mWifiManager.reconnect();
                // Wait for it to be reconnected.
                PollingCheck.check(
                        "Wifi not reconnected",
                        WIFI_CONNECT_TIMEOUT_MILLIS,
                        () -> mWifiManager.getConnectionInfo().getNetworkId() != -1);
            }

            uiAutomation.dropShellPermissionIdentity();
        }
    }
}
