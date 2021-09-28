/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.server.wifi;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalAnswers.answerVoid;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.atMost;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.database.ContentObserver;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkAgent;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.wifi.IScoreUpdateObserver;
import android.net.wifi.IWifiConnectedNetworkScorer;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiInfo;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.os.test.TestLooper;

import androidx.test.filters.SmallTest;

import com.android.wifi.resources.R;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.io.PrintWriter;

/**
 * Unit tests for {@link com.android.server.wifi.WifiScoreReport}.
 */
@SmallTest
public class WifiScoreReportTest extends WifiBaseTest {
    class FakeClock extends Clock {
        long mWallClockMillis = 1500000000000L;
        int mStepMillis = 1001;

        @Override
        public long getWallClockMillis() {
            mWallClockMillis += mStepMillis;
            return mWallClockMillis;
        }
    }

    private static final int TEST_NETWORK_ID = 860370;
    private static final int TEST_SESSION_ID = 8603703; // last digit is a check digit

    FakeClock mClock;
    WifiConfiguration mWifiConfiguration;
    WifiScoreReport mWifiScoreReport;
    ScanDetailCache mScanDetailCache;
    WifiInfo mWifiInfo;
    ScoringParams mScoringParams;
    NetworkAgent mNetworkAgent;
    WifiThreadRunner mWifiThreadRunner;
    @Mock Context mContext;
    @Mock Resources mResources;
    @Mock WifiMetrics mWifiMetrics;
    @Mock PrintWriter mPrintWriter;
    @Mock IBinder mAppBinder;
    @Mock IWifiConnectedNetworkScorer mWifiConnectedNetworkScorer;
    @Mock WifiNative mWifiNative;
    @Mock BssidBlocklistMonitor mBssidBlocklistMonitor;
    @Mock Network mNetwork;
    @Mock DeviceConfigFacade mDeviceConfigFacade;
    @Mock Looper mWifiLooper;
    @Mock FrameworkFacade mFrameworkFacade;
    private TestLooper mLooper;

    public class WifiConnectedNetworkScorerImpl extends IWifiConnectedNetworkScorer.Stub {
        public IScoreUpdateObserver mScoreUpdateObserver;
        public int mSessionId = -1;

        @Override
        public void onStart(int sessionId) {
            mSessionId = sessionId;
        }
        @Override
        public void onStop(int sessionId) {
        }
        @Override
        public void onSetScoreUpdateObserver(IScoreUpdateObserver observerImpl) {
            mScoreUpdateObserver = observerImpl;
        }
    }

    // NetworkAgent is abstract, so a subclass is necessary
    private static class TestNetworkAgent extends NetworkAgent {
        TestNetworkAgent(Context context) {
            super(new TestLooper().getLooper(), context, "TestNetworkAgent",
                    mock(NetworkInfo.class), new NetworkCapabilities(), new LinkProperties(), 0);
        }
        @Override protected void unwanted() { }
    }

    /**
     * Sets up resource values for testing
     *
     * See frameworks/base/core/res/res/values/config.xml
     */
    private void setUpResources(Resources resources) {
        when(resources.getInteger(
                R.integer.config_wifi_framework_wifi_score_bad_rssi_threshold_5GHz))
            .thenReturn(-82);
        when(resources.getInteger(
                R.integer.config_wifi_framework_wifi_score_entry_rssi_threshold_5GHz))
            .thenReturn(-77);
        when(resources.getInteger(
                R.integer.config_wifi_framework_wifi_score_low_rssi_threshold_5GHz))
            .thenReturn(-70);
        when(resources.getInteger(
                R.integer.config_wifi_framework_wifi_score_good_rssi_threshold_5GHz))
            .thenReturn(-57);
        when(resources.getInteger(
                R.integer.config_wifi_framework_wifi_score_bad_rssi_threshold_24GHz))
            .thenReturn(-85);
        when(resources.getInteger(
                R.integer.config_wifi_framework_wifi_score_entry_rssi_threshold_24GHz))
            .thenReturn(-80);
        when(resources.getInteger(
                R.integer.config_wifi_framework_wifi_score_low_rssi_threshold_24GHz))
            .thenReturn(-73);
        when(resources.getInteger(
                R.integer.config_wifi_framework_wifi_score_good_rssi_threshold_24GHz))
            .thenReturn(-60);
        when(resources.getInteger(
                R.integer.config_wifiFrameworkScoreBadRssiThreshold6ghz))
            .thenReturn(-82);
        when(resources.getInteger(
                R.integer.config_wifiFrameworkScoreEntryRssiThreshold6ghz))
            .thenReturn(-77);
        when(resources.getInteger(
                R.integer.config_wifiFrameworkScoreLowRssiThreshold6ghz))
            .thenReturn(-70);
        when(resources.getInteger(
                R.integer.config_wifiFrameworkScoreGoodRssiThreshold6ghz))
            .thenReturn(-57);
        when(resources.getInteger(
                R.integer.config_wifiFrameworkMinPacketPerSecondHighTraffic))
            .thenReturn(100);
        when(resources.getBoolean(
                R.bool.config_wifiMinConfirmationDurationSendNetworkScoreEnabled))
            .thenReturn(false);
    }

    /**
     * Sets up for unit test
     */
    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        setUpResources(mResources);
        mWifiInfo = new WifiInfo();
        mWifiInfo.setFrequency(2412);
        mLooper = new TestLooper();
        when(mContext.getResources()).thenReturn(mResources);
        final ConnectivityManager cm = mock(ConnectivityManager.class);
        when(mContext.getSystemService(Context.CONNECTIVITY_SERVICE)).thenReturn(cm);
        when(cm.registerNetworkAgent(any(), any(), any(), any(), anyInt(), any(), anyInt()))
                .thenReturn(mNetwork);
        when(mNetwork.getNetId()).thenReturn(0);
        mNetworkAgent = spy(new TestNetworkAgent(mContext));
        mClock = new FakeClock();
        mScoringParams = new ScoringParams();
        mWifiThreadRunner = new WifiThreadRunner(new Handler(mLooper.getLooper()));
        when(mFrameworkFacade.getIntegerSetting(any(Context.class),
                eq(WifiScoreReport.SETTINGS_SECURE_ADAPTIVE_CONNECTIVITY_ENABLED), eq(1)))
                .thenReturn(1);
        mWifiScoreReport = new WifiScoreReport(mScoringParams, mClock, mWifiMetrics, mWifiInfo,
                mWifiNative, mBssidBlocklistMonitor, mWifiThreadRunner,
                mDeviceConfigFacade, mContext, mWifiLooper, mFrameworkFacade);
        mWifiScoreReport.setNetworkAgent(mNetworkAgent);
        mWifiScoreReport.initialize();
        when(mDeviceConfigFacade.getMinConfirmationDurationSendLowScoreMs()).thenReturn(
                DeviceConfigFacade.DEFAULT_MIN_CONFIRMATION_DURATION_SEND_LOW_SCORE_MS);
        when(mDeviceConfigFacade.getMinConfirmationDurationSendHighScoreMs()).thenReturn(
                DeviceConfigFacade.DEFAULT_MIN_CONFIRMATION_DURATION_SEND_HIGH_SCORE_MS);
        when(mDeviceConfigFacade.getRssiThresholdNotSendLowScoreToCsDbm()).thenReturn(
                DeviceConfigFacade.DEFAULT_RSSI_THRESHOLD_NOT_SEND_LOW_SCORE_TO_CS_DBM);
    }

    /**
     * Cleans up after test
     */
    @After
    public void tearDown() throws Exception {
        mResources = null;
        mWifiScoreReport = null;
        mWifiMetrics = null;
    }

    /**
     * Test for score reporting
     *
     * The score should be sent to both the NetworkAgent and the
     * WifiMetrics
     */
    @Test
    public void calculateAndReportScoreSucceeds() throws Exception {
        mWifiInfo.setRssi(-77);
        mWifiScoreReport.calculateAndReportScore();
        verify(mNetworkAgent).sendNetworkScore(anyInt());
        verify(mWifiMetrics).incrementWifiScoreCount(anyInt());
    }

    /**
     * Test for no score report if rssi is invalid
     *
     * The score should be sent to neither the NetworkAgent nor the
     * WifiMetrics
     */
    @Test
    public void calculateAndReportScoreDoesNotReportWhenRssiIsNotValid() throws Exception {
        mWifiInfo.setRssi(WifiInfo.INVALID_RSSI);
        mWifiScoreReport.calculateAndReportScore();
        verify(mNetworkAgent, never()).sendNetworkScore(anyInt());
        verify(mWifiMetrics, never()).incrementWifiScoreCount(anyInt());
    }

    /**
     * Test for operation with null NetworkAgent
     *
     * Expect to not die, and to calculate the score and report to metrics.
     */
    @Test
    public void networkAgentMayBeNull() throws Exception {
        mWifiInfo.setRssi(-33);
        mWifiScoreReport.enableVerboseLogging(true);
        mWifiScoreReport.setNetworkAgent(null);
        mWifiScoreReport.calculateAndReportScore();
        verify(mWifiMetrics).incrementWifiScoreCount(anyInt());
    }

    /**
     * Exercise the rates with low RSSI
     *
     * The setup has a low (not bad) RSSI, and data movement (txSuccessRate) above
     * the threshold.
     *
     * Expect a score above threshold.
     */
    @Test
    public void allowLowRssiIfDataIsMoving() throws Exception {
        mWifiInfo.setRssi(-80);
        mWifiInfo.setLinkSpeed(6); // Mbps
        mWifiInfo.setSuccessfulTxPacketsPerSecond(5.1); // proportional to pps
        mWifiInfo.setSuccessfulRxPacketsPerSecond(5.1);
        for (int i = 0; i < 10; i++) {
            mWifiScoreReport.calculateAndReportScore();
        }
        int score = mWifiInfo.getScore();
        assertTrue(score > ConnectedScore.WIFI_TRANSITION_SCORE);
    }

    /**
     * Bad RSSI without data moving should allow handoff
     *
     * The setup has a bad RSSI, and the txSuccessRate is below threshold; several
     * scoring iterations are performed.
     *
     * Expect the score to drop below the handoff threshold.
     */
    @Test
    public void giveUpOnBadRssiWhenDataIsNotMoving() throws Exception {
        mWifiInfo.setRssi(-100);
        mWifiInfo.setLinkSpeed(6); // Mbps
        mWifiInfo.setFrequency(5220);
        mWifiScoreReport.enableVerboseLogging(true);
        mWifiInfo.setSuccessfulTxPacketsPerSecond(0.1);
        mWifiInfo.setSuccessfulRxPacketsPerSecond(0.1);
        for (int i = 0; i < 10; i++) {
            mWifiScoreReport.calculateAndReportScore();
        }
        int score = mWifiInfo.getScore();
        assertTrue(score < ConnectedScore.WIFI_TRANSITION_SCORE);
        verify(mNetworkAgent, atLeast(1)).sendNetworkScore(score);
    }

    /**
     * When the score ramps down to the exit theshold, let go.
     */
    @Test
    public void giveUpOnBadRssiAggressively() throws Exception {
        String oops = "giveUpOnBadRssiAggressively";
        mWifiInfo.setFrequency(5220);
        for (int rssi = -60; rssi >= -83; rssi -= 1) {
            mWifiInfo.setRssi(rssi);
            oops += " " + mClock.mWallClockMillis + "," + rssi;
            mWifiScoreReport.calculateAndReportScore();
            oops += ":" + mWifiInfo.getScore();
        }
        int score = mWifiInfo.getScore();
        verify(mNetworkAgent, atLeast(1)).sendNetworkScore(score);
        assertTrue(oops, score < ConnectedScore.WIFI_TRANSITION_SCORE);
    }

    /**
     * RSSI that falls rapidly but does not cross entry threshold should not cause handoff
     *
     * Expect the score to not drop below the handoff threshold.
     */
    @Test
    public void stayOnIfRssiDoesNotGetBelowEntryThreshold() throws Exception {
        String oops = "didNotStickLanding";
        int minScore = 100;
        mWifiInfo.setLinkSpeed(6); // Mbps
        mWifiInfo.setFrequency(5220);
        mWifiScoreReport.enableVerboseLogging(true);
        mWifiInfo.setSuccessfulTxPacketsPerSecond(0.1);
        mWifiInfo.setSuccessfulRxPacketsPerSecond(0.1);
        assertTrue(mScoringParams.update("rssi5=-83:-80:-66:-55"));
        for (int r = -30; r >= -100; r -= 1) {
            int rssi = Math.max(r, -80);
            mWifiInfo.setRssi(rssi);
            oops += " " + mClock.mWallClockMillis + "," + rssi;
            mWifiScoreReport.calculateAndReportScore();
            oops += ":" + mWifiInfo.getScore();
            if (mWifiInfo.getScore() < minScore) minScore = mWifiInfo.getScore();
        }
        assertTrue(oops, minScore > ConnectedScore.WIFI_TRANSITION_SCORE);
    }

    /**
     * Don't breach if the success rates are great
     *
     * Ramp the RSSI down, but maintain a high packet throughput
     *
     * Expect score to stay above above threshold.
     */
    @Test
    public void allowTerribleRssiIfDataIsMovingWell() throws Exception {
        mWifiInfo.setSuccessfulTxPacketsPerSecond(
                mScoringParams.getYippeeSkippyPacketsPerSecond() + 0.1);
        mWifiInfo.setSuccessfulRxPacketsPerSecond(
                mScoringParams.getYippeeSkippyPacketsPerSecond() + 0.1);
        assertTrue(mWifiInfo.getSuccessfulTxPacketsPerSecond() > 10);
        mWifiInfo.setFrequency(5220);
        for (int r = -30; r >= -120; r -= 2) {
            mWifiInfo.setRssi(r);
            mWifiScoreReport.calculateAndReportScore();
            assertTrue(mWifiInfo.getScore() > ConnectedScore.WIFI_TRANSITION_SCORE);
        }
        // If the throughput dips, we should let go
        mWifiInfo.setSuccessfulRxPacketsPerSecond(
                mScoringParams.getYippeeSkippyPacketsPerSecond() - 0.1);
        mWifiScoreReport.calculateAndReportScore();
        assertTrue(mWifiInfo.getScore() < ConnectedScore.WIFI_TRANSITION_SCORE);
        // And even if throughput improves again, once we have decided to let go, disregard
        // the good rates.
        mWifiInfo.setSuccessfulRxPacketsPerSecond(
                mScoringParams.getYippeeSkippyPacketsPerSecond() + 0.1);
        mWifiScoreReport.calculateAndReportScore();
        assertTrue(mWifiInfo.getScore() < ConnectedScore.WIFI_TRANSITION_SCORE);
    }

    /**
     * Never ask for nud check when nud=0
     */
    @Test
    public void neverAskForNudCheckWhenNudKnobIsZero() throws Exception {
        assertTrue(mScoringParams.update("nud=0"));
        assertEquals(0, mScoringParams.getNudKnob());
        mWifiInfo.setFrequency(5220);
        for (int rssi = -30; rssi >= -120; rssi -= 1) {
            mWifiInfo.setRssi(rssi);
            mWifiScoreReport.calculateAndReportScore();
            assertFalse(mWifiScoreReport.shouldCheckIpLayer());
        }
    }

    /**
     * Eventually ask for nud check when nud=1
     */
    @Test
    public void eventuallyAskForNudCheckWhenNudKnobIsOne() throws Exception {
        String oops = "nud=1";
        long lastAskedMillis = 0; // Check that we don't send too soon
        int asks = 0; // Keep track of how many time we asked
        assertTrue(mScoringParams.update("nud=1"));
        assertEquals(1, mScoringParams.getNudKnob());
        mWifiInfo.setFrequency(5220);
        for (int rssi = -40; rssi >= -120; rssi -= 1) {
            mWifiInfo.setRssi(rssi);
            mWifiScoreReport.calculateAndReportScore();
            boolean ask = mWifiScoreReport.shouldCheckIpLayer();
            if (ask) {
                assertTrue(mWifiInfo.getScore() < ConnectedScore.WIFI_TRANSITION_SCORE);
                assertTrue(oops, mClock.mWallClockMillis >= lastAskedMillis + 5000);
                lastAskedMillis = mClock.mWallClockMillis;
                oops += " " + lastAskedMillis + ":" + mWifiInfo.getScore();
                mWifiScoreReport.noteIpCheck();
                asks++;
            }
        }
        assertTrue(oops + " asks:" + asks, asks > 5 && asks < 15);
    }


    /**
     * Ask for more nud checks when nud=10
     */
    @Test
    public void askForMoreNudChecksWhenNudKnobIsBigger() throws Exception {
        String oops = "nud=10";
        long lastAskedMillis = 0; // Check that we don't send too soon
        int asks = 0; // Keep track of how many time we asked
        assertTrue(mScoringParams.update("nud=10"));
        assertEquals(10, mScoringParams.getNudKnob());
        mWifiInfo.setFrequency(5220);
        for (int rssi = -40; rssi >= -120; rssi -= 1) {
            mWifiInfo.setRssi(rssi);
            mWifiScoreReport.calculateAndReportScore();
            boolean ask = mWifiScoreReport.shouldCheckIpLayer();
            if (ask) {
                assertTrue(mWifiInfo.getScore() < ConnectedScore.WIFI_TRANSITION_SCORE);
                assertTrue(oops, mClock.mWallClockMillis >= lastAskedMillis + 5000);
                lastAskedMillis = mClock.mWallClockMillis;
                oops += " " + lastAskedMillis + ":" + mWifiInfo.getScore();
                mWifiScoreReport.noteIpCheck();
                asks++;
            }
        }
        assertTrue(oops + " asks:" + asks, asks > 12 && asks < 80);
    }

    /**
     * Test initial conditions, and after reset()
     */
    @Test
    public void exerciseReset() throws Exception {
        assertFalse(mWifiScoreReport.shouldCheckIpLayer());
        mWifiScoreReport.reset();
        assertFalse(mWifiScoreReport.shouldCheckIpLayer());
    }

    /**
     * This setup causes some reports to be generated when println
     * methods are called, to check for "concurrent" modification
     * errors.
     */
    private void setupToGenerateAReportWhenPrintlnIsCalled() {
        int[] counter = new int[1];
        doAnswer(answerVoid((String line) -> {
            if (counter[0]++ < 3) {
                mWifiScoreReport.calculateAndReportScore();
            }
        })).when(mPrintWriter).println(anyString());
    }

    /**
     * Test data logging
     */
    @Test
    public void testDataLogging() throws Exception {
        for (int i = 0; i < 10; i++) {
            mWifiInfo.setRssi(-65 + i);
            mWifiInfo.setLinkSpeed(300);
            mWifiInfo.setFrequency(5220);
            mWifiInfo.setSuccessfulTxPacketsPerSecond(0.1 + i);
            mWifiInfo.setRetriedTxPacketsRate(0.2 + i);
            mWifiInfo.setLostTxPacketsPerSecond(0.01 * i);
            mWifiInfo.setSuccessfulRxPacketsPerSecond(0.3 + i);
            mWifiScoreReport.calculateAndReportScore();
        }
        setupToGenerateAReportWhenPrintlnIsCalled();
        mWifiScoreReport.dump(null, mPrintWriter, null);
        verify(mPrintWriter, times(11)).println(anyString());
    }

    /**
     *  Test data logging limit
     *  <p>
     *  Check that only a bounded amount of data is collected for dumpsys report
     */
    @Test
    public void testDataLoggingLimit() throws Exception {
        for (int i = 0; i < 3620; i++) {
            mWifiInfo.setRssi(-65 + i % 20);
            mWifiInfo.setLinkSpeed(300);
            mWifiInfo.setFrequency(5220);
            mWifiInfo.setSuccessfulTxPacketsPerSecond(0.1 + i % 100);
            mWifiInfo.setRetriedTxPacketsRate(0.2 + i % 100);
            mWifiInfo.setLostTxPacketsPerSecond(0.0001 * i);
            mWifiInfo.setSuccessfulRxPacketsPerSecond(0.3 + i % 200);
            mWifiScoreReport.calculateAndReportScore();
        }
        mWifiScoreReport.dump(null, mPrintWriter, null);
        verify(mPrintWriter, atMost(3601)).println(anyString());
    }

    /**
     * Test for staying at below transition score for a certain period of time.
     */
    @Test
    public void stayAtBelowTransitionScoreForCertainPeriodOfTime() throws Exception {
        mWifiScoreReport.enableVerboseLogging(true);
        mWifiInfo.setFrequency(5220);

        // Reduce RSSI value to fall below the transition score
        for (int rssi = -60; rssi >= -83; rssi -= 1) {
            mWifiInfo.setRssi(rssi);
            mWifiScoreReport.calculateAndReportScore();
        }
        assertTrue(mWifiInfo.getScore() < ConnectedScore.WIFI_TRANSITION_SCORE);

        // Then, set high RSSI value to exceed the transition score
        mWifiInfo.setRssi(-50);
        // 8 seconds elapse
        for (int i = 0; i < 8; i++) {
            mWifiScoreReport.calculateAndReportScore();
        }
        assertTrue(mWifiInfo.getScore() < ConnectedScore.WIFI_TRANSITION_SCORE);

        // 9 seconds elapse
        mWifiScoreReport.calculateAndReportScore();
        assertTrue(mWifiInfo.getScore() > ConnectedScore.WIFI_TRANSITION_SCORE);
    }

    /**
     * Test for resetting the internal timer which is used to keep staying at
     * below transition score for a certain period of time.
     */
    @Test
    public void stayAtBelowTransitionScoreWithReset() throws Exception {
        mWifiScoreReport.enableVerboseLogging(true);
        mWifiInfo.setFrequency(5220);

        // Reduce RSSI value to fall below the transition score
        for (int rssi = -60; rssi >= -83; rssi -= 1) {
            mWifiInfo.setRssi(rssi);
            mWifiScoreReport.calculateAndReportScore();
        }
        assertTrue(mWifiInfo.getScore() < ConnectedScore.WIFI_TRANSITION_SCORE);

        // Then, set high RSSI value to exceed the transition score
        mWifiInfo.setRssi(-50);
        // Reset the internal timer so that no need to wait for 9 seconds
        mWifiScoreReport.reset();
        mWifiScoreReport.calculateAndReportScore();
        assertTrue(mWifiInfo.getScore() > ConnectedScore.WIFI_TRANSITION_SCORE);
    }

    /**
     * Verify that client gets ScoreChangeCallback object when client sets its scorer.
     */
    @Test
    public void testClientNotification() throws RemoteException {
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, mWifiConnectedNetworkScorer);
        // Client should get ScoreChangeCallback.
        verify(mWifiConnectedNetworkScorer).onSetScoreUpdateObserver(any());
    }

    /**
     * Verify that clear client should be handled.
     */
    @Test
    public void testClearClient() throws RemoteException {
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, mWifiConnectedNetworkScorer);
        mWifiScoreReport.clearWifiConnectedNetworkScorer();
        verify(mAppBinder).unlinkToDeath(any(), anyInt());

        mWifiScoreReport.startConnectedNetworkScorer(10);
        verify(mWifiConnectedNetworkScorer, never()).onStart(anyInt());
    }

    /**
     * Verify that WifiScoreReport adds for death notification on setting client.
     */
    @Test
    public void testAddsForBinderDeathOnSetClient() throws Exception {
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, mWifiConnectedNetworkScorer);
        verify(mAppBinder).linkToDeath(any(IBinder.DeathRecipient.class), anyInt());
    }

    /**
     * Verify that client fails to get message when scorer add failed.
     */
    @Test
    public void testAddsScorerFailureOnLinkToDeath() throws Exception {
        doThrow(new RemoteException())
                .when(mAppBinder).linkToDeath(any(IBinder.DeathRecipient.class), anyInt());
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, mWifiConnectedNetworkScorer);
        verify(mAppBinder).linkToDeath(any(IBinder.DeathRecipient.class), anyInt());

        // Client should not get any message when scorer add failed.
        verify(mWifiConnectedNetworkScorer, never()).onSetScoreUpdateObserver(any());
    }

    /**
     * Verify netId to sessionId conversion.
     */
    @Test
    public void testSessionId() throws Exception {
        assertEquals(-1, WifiScoreReport.sessionIdFromNetId(Integer.MIN_VALUE));
        assertEquals(-1, WifiScoreReport.sessionIdFromNetId(-42));
        assertEquals(-1, WifiScoreReport.sessionIdFromNetId(-1));
        assertEquals(-1, WifiScoreReport.sessionIdFromNetId(0));
        assertEquals(18, WifiScoreReport.sessionIdFromNetId(1));
        assertEquals(3339, WifiScoreReport.sessionIdFromNetId(333));
        assertEquals(TEST_SESSION_ID, WifiScoreReport.sessionIdFromNetId(TEST_NETWORK_ID));
        int dangerOfOverflow = Integer.MAX_VALUE / 10;
        assertEquals(214748364, dangerOfOverflow);
        assertEquals(2147483646, WifiScoreReport.sessionIdFromNetId(dangerOfOverflow));
        assertEquals(8, WifiScoreReport.sessionIdFromNetId(dangerOfOverflow + 1));
        assertEquals(8, WifiScoreReport.sessionIdFromNetId(Integer.MAX_VALUE));
    }

    /**
     * Verify that client gets session ID when onStart() method is called.
     */
    @Test
    public void testClientGetSessionIdOnStart() throws Exception {
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, mWifiConnectedNetworkScorer);
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);
        verify(mWifiConnectedNetworkScorer).onStart(TEST_SESSION_ID);
    }

    /**
     * Verify that onStart is called if there is already an active network when registered.
     */
    @Test
    public void testClientStartOnRegWhileActive() throws Exception {
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, mWifiConnectedNetworkScorer);
        verify(mWifiConnectedNetworkScorer).onStart(TEST_SESSION_ID);
    }

    /**
     * Verify that client gets session ID when onStop() method is called.
     */
    @Test
    public void testClientGetSessionIdOnStop() throws Exception {
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, mWifiConnectedNetworkScorer);
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);
        verify(mWifiConnectedNetworkScorer).onStart(TEST_SESSION_ID);
        mWifiScoreReport.stopConnectedNetworkScorer();
        verify(mWifiConnectedNetworkScorer).onStop(TEST_SESSION_ID);
        // After the session stops, it should not start again (without a new NetworkAgent)
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);
        verify(mWifiConnectedNetworkScorer).onStart(anyInt());
    }

    /**
     * Verify that only a single Wi-Fi connected network scorer can be registered successfully.
     */
    @Test
    public void verifyOnlyASingleScorerCanBeRegisteredSuccessively() throws Exception {
        WifiConnectedNetworkScorerImpl scorerImpl = new WifiConnectedNetworkScorerImpl();
        assertEquals(true, mWifiScoreReport.setWifiConnectedNetworkScorer(
                mAppBinder, scorerImpl));
        assertEquals(false, mWifiScoreReport.setWifiConnectedNetworkScorer(
                mAppBinder, scorerImpl));
    }

    /**
     * Verify that WifiScoreReport gets updated score when notifyScoreUpdate() is called by apps.
     */
    @Test
    public void testFrameworkGetsUpdatesScore() throws Exception {
        WifiConnectedNetworkScorerImpl scorerImpl = new WifiConnectedNetworkScorerImpl();
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, scorerImpl);
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);

        assertEquals(TEST_SESSION_ID, scorerImpl.mSessionId);

        // Invalid session ID
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(-1, 49);
        assertEquals(mWifiScoreReport.getScore(), ConnectedScore.WIFI_MAX_SCORE);

        // Incorrect session ID
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId + 10, 49);
        assertEquals(mWifiScoreReport.getScore(), ConnectedScore.WIFI_MAX_SCORE);

        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 49);
        mLooper.dispatchAll();
        verify(mNetworkAgent).sendNetworkScore(49);
        assertEquals(mWifiScoreReport.getScore(), 49);

        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 59);
        mLooper.dispatchAll();
        verify(mNetworkAgent).sendNetworkScore(59);
        assertEquals(mWifiScoreReport.getScore(), 59);
    }

    /**
     * Verify that WifiScoreReport triggers an update of WifiUsabilityStatsEntry.
     */
    @Test
    public void testFrameworkTriggersUpdateOfWifiUsabilityStats() throws Exception {
        WifiConnectedNetworkScorerImpl scorerImpl = new WifiConnectedNetworkScorerImpl();
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, scorerImpl);
        mWifiScoreReport.setInterfaceName("wlan0");
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);

        scorerImpl.mScoreUpdateObserver.triggerUpdateOfWifiUsabilityStats(scorerImpl.mSessionId);
        mLooper.dispatchAll();
        verify(mWifiNative).getWifiLinkLayerStats("wlan0");
        verify(mWifiNative).signalPoll("wlan0");
    }

    /**
     * Ask for nud when score from external scorer breaches
     */
    @Test
    public void askForNudCheckWhenExternalScoreBreaches() throws Exception {
        WifiConnectedNetworkScorerImpl scorerImpl = new WifiConnectedNetworkScorerImpl();
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, scorerImpl);
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);
        mClock.mStepMillis = 0;

        mClock.mWallClockMillis = 5001;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 49);
        mLooper.dispatchAll();
        assertTrue(mWifiScoreReport.shouldCheckIpLayer());
        mWifiScoreReport.noteIpCheck();

        mClock.mWallClockMillis = 10000;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 49);
        mLooper.dispatchAll();
        assertFalse(mWifiScoreReport.shouldCheckIpLayer());

        mClock.mWallClockMillis = 10001;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 49);
        mLooper.dispatchAll();
        assertTrue(mWifiScoreReport.shouldCheckIpLayer());
    }

    /**
     * Verify BSSID blocklist does not happen when score stays below threshold for less than the
     * minimum duration
     */
    @Test
    public void bssidBlockListDoesnotHappenWhenExitingIsLessThanMinDuration() throws Exception {
        WifiConnectedNetworkScorerImpl scorerImpl = new WifiConnectedNetworkScorerImpl();
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, scorerImpl);
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);
        mClock.mStepMillis = 0;

        mClock.mWallClockMillis = 10;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 49);
        mLooper.dispatchAll();
        mClock.mWallClockMillis = 29009;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 49);
        mLooper.dispatchAll();
        mWifiScoreReport.stopConnectedNetworkScorer();
        mLooper.dispatchAll();
        verify(mBssidBlocklistMonitor, never()).handleBssidConnectionFailure(any(), any(),
                anyInt(), anyInt());
    }

    /**
     * Verify BSSID blocklist happens when score stays below threshold for longer than the
     * minimum duration
     */
    @Test
    public void bssidBlockListHappensWhenExitingIsLongerThanMinDuration() throws Exception {
        WifiConnectedNetworkScorerImpl scorerImpl = new WifiConnectedNetworkScorerImpl();
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, scorerImpl);
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);
        mClock.mStepMillis = 0;

        mClock.mWallClockMillis = 10;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 49);
        mLooper.dispatchAll();
        mClock.mWallClockMillis = 29011;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 49);
        mLooper.dispatchAll();
        mWifiScoreReport.stopConnectedNetworkScorer();
        mLooper.dispatchAll();
        verify(mBssidBlocklistMonitor).handleBssidConnectionFailure(any(), any(),
                eq(BssidBlocklistMonitor.REASON_FRAMEWORK_DISCONNECT_CONNECTED_SCORE), anyInt());
    }

    /**
     * Verify BSSID blocklist does not happen when there is score flip flop
     */
    @Test
    public void bssidBlockListDoesnotHappenWhenExitingIsReset() throws Exception {
        WifiConnectedNetworkScorerImpl scorerImpl = new WifiConnectedNetworkScorerImpl();
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, scorerImpl);
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);
        mClock.mStepMillis = 0;

        mClock.mWallClockMillis = 10;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 49);
        mLooper.dispatchAll();
        mClock.mWallClockMillis = 15000;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 51);
        mLooper.dispatchAll();
        mClock.mWallClockMillis = 29011;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 49);
        mLooper.dispatchAll();
        mWifiScoreReport.stopConnectedNetworkScorer();
        mLooper.dispatchAll();
        verify(mBssidBlocklistMonitor, never()).handleBssidConnectionFailure(any(), any(),
                anyInt(), anyInt());
    }

    /**
     * Verify that the initial score value in WifiInfo is the max when onStart is called.
     */
    @Test
    public void testOnStartInitialScoreInWifiInfoIsMaxScore() throws Exception {
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, mWifiConnectedNetworkScorer);
        assertEquals(ConnectedScore.WIFI_MAX_SCORE, mWifiInfo.getScore());
    }

    /**
     * Verify confirmation duration is not added when it is not enabled in config overlay by default
     */
    @Test
    public void confirmationDurationIsNotAddedWhenItIsNotEnabledInConfigOverlay() throws Exception {
        WifiConnectedNetworkScorerImpl scorerImpl = new WifiConnectedNetworkScorerImpl();
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, scorerImpl);
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);
        mClock.mStepMillis = 0;

        mClock.mWallClockMillis = 10;
        mWifiInfo.setRssi(-65);
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 49);
        mLooper.dispatchAll();
        verify(mNetworkAgent).sendNetworkScore(anyInt());
    }

    /**
     * Verify confirmation duration is not added when there is no score breacht
     */
    @Test
    public void confirmationDurationIsNotAddedWhenThereIsNoScoreBreach() throws Exception {
        WifiConnectedNetworkScorerImpl scorerImpl = new WifiConnectedNetworkScorerImpl();
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, scorerImpl);
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);
        mClock.mStepMillis = 0;
        when(mContext.getResources().getBoolean(
            R.bool.config_wifiMinConfirmationDurationSendNetworkScoreEnabled)).thenReturn(true);

        mClock.mWallClockMillis = 10;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 60);
        mWifiInfo.setRssi(-70);
        mLooper.dispatchAll();
        verify(mNetworkAgent).sendNetworkScore(60);
        mClock.mWallClockMillis = 3010;
        mWifiInfo.setRssi(-65);
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 59);
        mLooper.dispatchAll();
        verify(mNetworkAgent).sendNetworkScore(59);
        mClock.mWallClockMillis = 6010;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 58);
        mLooper.dispatchAll();
        verify(mNetworkAgent).sendNetworkScore(58);
    }

    /**
     * Verify confirmation duration and RSSI check is added for reporting low score when it is
     * enabled in config overlay
     */
    @Test
    public void confirmationDurationAndRssiCheckIsAddedForSendingLowScore() throws Exception {
        WifiConnectedNetworkScorerImpl scorerImpl = new WifiConnectedNetworkScorerImpl();
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, scorerImpl);
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);
        mClock.mStepMillis = 0;
        when(mContext.getResources().getBoolean(
                R.bool.config_wifiMinConfirmationDurationSendNetworkScoreEnabled)).thenReturn(true);

        mClock.mWallClockMillis = 10;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 49);
        mLooper.dispatchAll();
        verify(mNetworkAgent, never()).sendNetworkScore(anyInt());
        mClock.mWallClockMillis = 10
                + mDeviceConfigFacade.DEFAULT_MIN_CONFIRMATION_DURATION_SEND_LOW_SCORE_MS - 1;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 48);
        mLooper.dispatchAll();
        verify(mNetworkAgent, never()).sendNetworkScore(anyInt());
        mClock.mWallClockMillis = 10
                + mDeviceConfigFacade.DEFAULT_MIN_CONFIRMATION_DURATION_SEND_LOW_SCORE_MS;
        mWifiInfo.setRssi(-65);
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 47);
        mLooper.dispatchAll();
        verify(mNetworkAgent, never()).sendNetworkScore(47);
        mClock.mWallClockMillis = 10
                + mDeviceConfigFacade.DEFAULT_MIN_CONFIRMATION_DURATION_SEND_LOW_SCORE_MS + 3000;
        mWifiInfo.setRssi(-68);
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 46);
        mLooper.dispatchAll();
        verify(mNetworkAgent).sendNetworkScore(46);
    }

    /**
     * Verify confirmation duration is not added for reporting high score with default zero value
     */
    @Test
    public void confirmationDurationIsNotAddedForSendingHighScore() throws Exception {
        WifiConnectedNetworkScorerImpl scorerImpl = new WifiConnectedNetworkScorerImpl();
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, scorerImpl);
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);
        mClock.mStepMillis = 0;
        when(mContext.getResources().getBoolean(
                R.bool.config_wifiMinConfirmationDurationSendNetworkScoreEnabled)).thenReturn(true);

        mClock.mWallClockMillis = 10;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 49);
        mLooper.dispatchAll();
        verify(mNetworkAgent, never()).sendNetworkScore(anyInt());
        mClock.mWallClockMillis = 3000;
        mWifiInfo.setRssi(-70);
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 51);
        mLooper.dispatchAll();
        verify(mNetworkAgent).sendNetworkScore(51);
        mClock.mWallClockMillis = 6000;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 52);
        mLooper.dispatchAll();
        verify(mNetworkAgent).sendNetworkScore(52);
    }

    /**
     * Verify confirmation duration is added for reporting high score with non-zero value
     */
    @Test
    public void confirmationDurationIsAddedForSendingHighScore() throws Exception {
        WifiConnectedNetworkScorerImpl scorerImpl = new WifiConnectedNetworkScorerImpl();
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, scorerImpl);
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);
        mClock.mStepMillis = 0;
        when(mContext.getResources().getBoolean(
                R.bool.config_wifiMinConfirmationDurationSendNetworkScoreEnabled)).thenReturn(true);
        when(mDeviceConfigFacade.getMinConfirmationDurationSendHighScoreMs()).thenReturn(4000);

        mClock.mWallClockMillis = 10;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 49);
        mLooper.dispatchAll();
        verify(mNetworkAgent, never()).sendNetworkScore(anyInt());
        mClock.mWallClockMillis = 3000;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 51);
        mLooper.dispatchAll();
        verify(mNetworkAgent, never()).sendNetworkScore(anyInt());
        mClock.mWallClockMillis = 6999;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 52);
        mLooper.dispatchAll();
        verify(mNetworkAgent, never()).sendNetworkScore(anyInt());
        mClock.mWallClockMillis = 7000;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 53);
        mLooper.dispatchAll();
        verify(mNetworkAgent).sendNetworkScore(53);
    }

    /**
     * Verify NUD check is not recommended and the score of 51 is sent to connectivity service
     * when adaptive connectivity is disabled for AOSP scorer.
     */
    @Test
    public void verifyNudCheckAndScoreIfToggleOffForAospScorer() throws Exception {
        mWifiInfo.setFrequency(5220);
        mWifiInfo.setRssi(-85);
        ArgumentCaptor<ContentObserver> observer = ArgumentCaptor.forClass(ContentObserver.class);
        verify(mFrameworkFacade).registerContentObserver(
                any(), any(), eq(true), observer.capture());
        when(mFrameworkFacade.getIntegerSetting(any(Context.class),
                eq(WifiScoreReport.SETTINGS_SECURE_ADAPTIVE_CONNECTIVITY_ENABLED), eq(1)))
                .thenReturn(0);
        observer.getValue().onChange(true);
        mWifiScoreReport.calculateAndReportScore();
        assertFalse(mWifiScoreReport.shouldCheckIpLayer());
        verify(mNetworkAgent).sendNetworkScore(51);
    }

    /**
     * Verify NUD check is not recommended and the score of 51 is sent to connectivity service
     * when adaptive connectivity is disabled for external Wi-Fi scorer.
     */
    @Test
    public void verifyNudCheckAndScoreIfToggleOffForExternalScorer() throws Exception {
        WifiConnectedNetworkScorerImpl scorerImpl = new WifiConnectedNetworkScorerImpl();
        // Register Client for verification.
        mWifiScoreReport.setWifiConnectedNetworkScorer(mAppBinder, scorerImpl);
        when(mNetwork.getNetId()).thenReturn(TEST_NETWORK_ID);
        mWifiScoreReport.startConnectedNetworkScorer(TEST_NETWORK_ID);
        mClock.mStepMillis = 0;
        when(mContext.getResources().getBoolean(
                R.bool.config_wifiMinConfirmationDurationSendNetworkScoreEnabled)).thenReturn(true);
        when(mDeviceConfigFacade.getMinConfirmationDurationSendHighScoreMs()).thenReturn(4000);

        ArgumentCaptor<ContentObserver> observer = ArgumentCaptor.forClass(ContentObserver.class);
        verify(mFrameworkFacade).registerContentObserver(
                any(), any(), eq(true), observer.capture());
        when(mFrameworkFacade.getIntegerSetting(any(Context.class),
                eq(WifiScoreReport.SETTINGS_SECURE_ADAPTIVE_CONNECTIVITY_ENABLED), eq(1)))
                .thenReturn(0);
        observer.getValue().onChange(true);

        mClock.mWallClockMillis = 10;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 49);
        mLooper.dispatchAll();
        verify(mNetworkAgent, never()).sendNetworkScore(anyInt());
        assertFalse(mWifiScoreReport.shouldCheckIpLayer());

        mClock.mWallClockMillis = 10
                + mDeviceConfigFacade.DEFAULT_MIN_CONFIRMATION_DURATION_SEND_LOW_SCORE_MS - 1;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 48);
        mLooper.dispatchAll();
        verify(mNetworkAgent, never()).sendNetworkScore(anyInt());
        mClock.mWallClockMillis = 10
                + mDeviceConfigFacade.DEFAULT_MIN_CONFIRMATION_DURATION_SEND_LOW_SCORE_MS;
        scorerImpl.mScoreUpdateObserver.notifyScoreUpdate(scorerImpl.mSessionId, 47);
        mLooper.dispatchAll();
        verify(mNetworkAgent).sendNetworkScore(51);
    }
}
