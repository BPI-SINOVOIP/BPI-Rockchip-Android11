/*
 * Copyright (C) 2018 The Android Open Source Project
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
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.net.wifi.WifiInfo;
import android.os.Handler;
import android.os.test.TestLooper;
import android.telephony.PhoneStateListener;
import android.telephony.TelephonyManager;

import androidx.test.filters.SmallTest;

import com.android.dx.mockito.inline.extended.ExtendedMockito;
import com.android.server.wifi.proto.WifiStatsLog;
import com.android.server.wifi.proto.nano.WifiMetricsProto.WifiIsUnusableEvent;
import com.android.wifi.resources.R;

import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.MockitoSession;
import org.mockito.quality.Strictness;

/**
 * Unit tests for {@link com.android.server.wifi.WifiDataStall}.
 */
@SmallTest
public class WifiDataStallTest extends WifiBaseTest {
    private static final int TEST_MIN_TX_BAD = 1;
    private static final int TEST_MIN_TX_SUCCESS_WITHOUT_RX = 1;
    private static final long TEST_WIFI_BYTES =
            WifiDataStall.MAX_MS_DELTA_FOR_DATA_STALL * 1000 / 8;

    @Mock Context mContext;
    MockResources mMockResources = new MockResources();
    @Mock FrameworkFacade mFrameworkFacade;
    @Mock WifiChannelUtilization mWifiChannelUtilization;
    @Mock WifiMetrics mWifiMetrics;
    WifiDataStall mWifiDataStall;
    @Mock Clock mClock;
    @Mock DeviceConfigFacade mDeviceConfigFacade;
    @Mock WifiInfo mWifiInfo;
    @Mock TelephonyManager mTelephonyManager;
    @Mock Handler mHandler;
    @Mock ThroughputPredictor mThroughputPredictor;
    @Mock WifiNative.ConnectionCapabilities mCapabilities;

    private final WifiLinkLayerStats mOldLlStats = new WifiLinkLayerStats();
    private final WifiLinkLayerStats mNewLlStats = new WifiLinkLayerStats();
    private MockitoSession mSession;
    /**
     * Sets up for unit test
     */
    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        // Ensure Looper exists
        // Required by TelephonyManager.listen()
        TestLooper looper = new TestLooper();
        when(mContext.getResources()).thenReturn(mMockResources);
        when(mContext.getSystemService(Context.TELEPHONY_SERVICE)).thenReturn(mTelephonyManager);

        mMockResources.setInteger(
                R.integer.config_wifiPollRssiIntervalMilliseconds,
                3000);
        mMockResources.setInteger(
                R.integer.config_wifiDataStallMinTxBad, TEST_MIN_TX_BAD);
        mMockResources.setInteger(
                R.integer.config_wifiDataStallMinTxSuccessWithoutRx,
                TEST_MIN_TX_SUCCESS_WITHOUT_RX);
        when(mDeviceConfigFacade.getDataStallDurationMs()).thenReturn(
                DeviceConfigFacade.DEFAULT_DATA_STALL_DURATION_MS);
        when(mDeviceConfigFacade.getDataStallTxTputThrKbps()).thenReturn(
                DeviceConfigFacade.DEFAULT_DATA_STALL_TX_TPUT_THR_KBPS);
        when(mDeviceConfigFacade.getDataStallRxTputThrKbps()).thenReturn(
                DeviceConfigFacade.DEFAULT_DATA_STALL_RX_TPUT_THR_KBPS);
        when(mDeviceConfigFacade.getDataStallTxPerThr()).thenReturn(
                DeviceConfigFacade.DEFAULT_DATA_STALL_TX_PER_THR);
        when(mDeviceConfigFacade.getDataStallCcaLevelThr()).thenReturn(
                DeviceConfigFacade.DEFAULT_DATA_STALL_CCA_LEVEL_THR);
        when(mDeviceConfigFacade.getTxTputSufficientLowThrKbps()).thenReturn(
                DeviceConfigFacade.DEFAULT_TX_TPUT_SUFFICIENT_THR_LOW_KBPS);
        when(mDeviceConfigFacade.getTxTputSufficientHighThrKbps()).thenReturn(
                DeviceConfigFacade.DEFAULT_TX_TPUT_SUFFICIENT_THR_HIGH_KBPS);
        when(mDeviceConfigFacade.getRxTputSufficientLowThrKbps()).thenReturn(
                DeviceConfigFacade.DEFAULT_RX_TPUT_SUFFICIENT_THR_LOW_KBPS);
        when(mDeviceConfigFacade.getRxTputSufficientHighThrKbps()).thenReturn(
                DeviceConfigFacade.DEFAULT_RX_TPUT_SUFFICIENT_THR_HIGH_KBPS);
        when(mDeviceConfigFacade.getTputSufficientRatioThrNum()).thenReturn(
                DeviceConfigFacade.DEFAULT_TPUT_SUFFICIENT_RATIO_THR_NUM);
        when(mDeviceConfigFacade.getTputSufficientRatioThrDen()).thenReturn(
                DeviceConfigFacade.DEFAULT_TPUT_SUFFICIENT_RATIO_THR_DEN);
        when(mDeviceConfigFacade.getTxPktPerSecondThr()).thenReturn(
                DeviceConfigFacade.DEFAULT_TX_PACKET_PER_SECOND_THR);
        when(mDeviceConfigFacade.getRxPktPerSecondThr()).thenReturn(
                DeviceConfigFacade.DEFAULT_RX_PACKET_PER_SECOND_THR);
        when(mDeviceConfigFacade.getTxLinkSpeedLowThresholdMbps()).thenReturn(
                DeviceConfigFacade.DEFAULT_TX_LINK_SPEED_LOW_THRESHOLD_MBPS);
        when(mDeviceConfigFacade.getRxLinkSpeedLowThresholdMbps()).thenReturn(
                DeviceConfigFacade.DEFAULT_RX_LINK_SPEED_LOW_THRESHOLD_MBPS);

        when(mWifiInfo.getLinkSpeed()).thenReturn(10);
        when(mWifiInfo.getRxLinkSpeedMbps()).thenReturn(10);
        when(mWifiInfo.getFrequency()).thenReturn(5850);
        when(mWifiInfo.getBSSID()).thenReturn("5G_WiFi");

        mWifiDataStall = new WifiDataStall(mFrameworkFacade, mWifiMetrics, mContext,
                mDeviceConfigFacade, mWifiChannelUtilization, mClock, mHandler,
                mThroughputPredictor);
        mOldLlStats.txmpdu_be = 1000;
        mOldLlStats.retries_be = 1000;
        mOldLlStats.lostmpdu_be = 3000;
        mOldLlStats.rxmpdu_be = 4000;
        mOldLlStats.timeStampInMs = 10000;

        mNewLlStats.txmpdu_be = 2 * mOldLlStats.txmpdu_be;
        mNewLlStats.retries_be = 10 * mOldLlStats.retries_be;
        mNewLlStats.lostmpdu_be = mOldLlStats.lostmpdu_be;
        mNewLlStats.rxmpdu_be = mOldLlStats.rxmpdu_be + 100;
        mNewLlStats.timeStampInMs = mOldLlStats.timeStampInMs
                + WifiDataStall.MAX_MS_DELTA_FOR_DATA_STALL - 1;
        when(mWifiChannelUtilization.getUtilizationRatio(anyInt())).thenReturn(10);
        when(mThroughputPredictor.predictTxThroughput(any(), anyInt(), anyInt(), anyInt()))
                .thenReturn(50);
        when(mThroughputPredictor.predictRxThroughput(any(), anyInt(), anyInt(), anyInt()))
                .thenReturn(150);
        mWifiDataStall.init();
        mWifiDataStall.setConnectionCapabilities(mCapabilities);
        setUpWifiBytes(1, 1);
    }

    private void setUpWifiBytes(long txBytes, long rxBytes) {
        lenient().when(mFrameworkFacade.getTotalTxBytes()).thenReturn(txBytes);
        lenient().when(mFrameworkFacade.getTotalRxBytes()).thenReturn(rxBytes);
        lenient().when(mFrameworkFacade.getMobileTxBytes()).thenReturn((long) 0);
        lenient().when(mFrameworkFacade.getMobileRxBytes()).thenReturn((long) 0);
    }

    /**
     * Verify that LinkLayerStats for WifiIsUnusableEvent is correctly updated
     */
    private void verifyUpdateWifiIsUnusableLinkLayerStats() {
        verify(mWifiMetrics).updateWifiIsUnusableLinkLayerStats(
                mNewLlStats.txmpdu_be - mOldLlStats.txmpdu_be,
                mNewLlStats.retries_be - mOldLlStats.retries_be,
                mNewLlStats.lostmpdu_be - mOldLlStats.lostmpdu_be,
                mNewLlStats.rxmpdu_be - mOldLlStats.rxmpdu_be,
                mNewLlStats.timeStampInMs - mOldLlStats.timeStampInMs);
    }

    private PhoneStateListener mockPhoneStateListener() {
        PhoneStateListener dataConnectionStateListener = null;
        /* Capture the PhoneStateListener */
        ArgumentCaptor<PhoneStateListener> phoneStateListenerCaptor =
                ArgumentCaptor.forClass(PhoneStateListener.class);
        verify(mTelephonyManager).listen(phoneStateListenerCaptor.capture(),
                eq(PhoneStateListener.LISTEN_DATA_CONNECTION_STATE));
        dataConnectionStateListener = phoneStateListenerCaptor.getValue();
        assertNotNull(dataConnectionStateListener);
        return dataConnectionStateListener;
    }

    /**
     * Test cellular data connection is on and then off
     */
    @Test
    public void testCellularDataConnectionOnOff() throws Exception {
        mWifiDataStall.disablePhoneStateListener();
        mWifiDataStall.enablePhoneStateListener();
        PhoneStateListener phoneStateListener = mockPhoneStateListener();
        phoneStateListener.onDataConnectionStateChanged(TelephonyManager.DATA_CONNECTED,
                TelephonyManager.NETWORK_TYPE_LTE);
        assertEquals(true, mWifiDataStall.isCellularDataAvailable());
        phoneStateListener.onDataConnectionStateChanged(
                TelephonyManager.DATA_DISCONNECTED, TelephonyManager.NETWORK_TYPE_LTE);
        assertEquals(false, mWifiDataStall.isCellularDataAvailable());
        mWifiDataStall.disablePhoneStateListener();
        verify(mTelephonyManager, times(1)).listen(phoneStateListener,
                PhoneStateListener.LISTEN_NONE);
        mWifiDataStall.disablePhoneStateListener();
        verify(mTelephonyManager, times(1)).listen(phoneStateListener,
                PhoneStateListener.LISTEN_NONE);
    }

    /**
     * Verify throughput when Rx link speed is unavailable.
     * Also verify the logging of channel utilization and throughput.
     */
    @Test
    public void verifyThroughputNoRxLinkSpeed() throws Exception {
        mWifiDataStall.checkDataStallAndThroughputSufficiency(null, mNewLlStats, mWifiInfo);
        verify(mWifiMetrics).incrementChannelUtilizationCount(10, 5850);
        verify(mWifiMetrics).incrementThroughputKbpsCount(50_000, 150_000, 5850);
        assertEquals(50_000, mWifiDataStall.getTxThroughputKbps());
        assertEquals(150_000, mWifiDataStall.getRxThroughputKbps());
        when(mWifiInfo.getRxLinkSpeedMbps()).thenReturn(-1);
        mWifiDataStall.checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo);
        assertEquals(960, mWifiDataStall.getTxThroughputKbps());
        assertEquals(-1, mWifiDataStall.getRxThroughputKbps());
        verify(mWifiMetrics).incrementThroughputKbpsCount(960, -1, 5850);
    }

    /**
     * Verify there is a Tx data stall from high Tx PER and low Tx throughput
     */
    @Test
    public void verifyDataStallTxFailure() throws Exception {
        when(mClock.getElapsedSinceBootMillis()).thenReturn(10L);

        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        verify(mWifiMetrics).incrementThroughputKbpsCount(960, 9609, 5850);
        verifyUpdateWifiIsUnusableLinkLayerStats();
        when(mClock.getElapsedSinceBootMillis()).thenReturn(
                10L + DeviceConfigFacade.DEFAULT_DATA_STALL_DURATION_MS);
        setUpWifiBytes(TEST_WIFI_BYTES, TEST_WIFI_BYTES);
        assertEquals(WifiIsUnusableEvent.TYPE_DATA_STALL_BAD_TX, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        assertEquals(false, mWifiDataStall.isThroughputSufficient());
        assertEquals(960, mWifiDataStall.getTxThroughputKbps());
        assertEquals(9609, mWifiDataStall.getRxThroughputKbps());
        verify(mWifiMetrics).logWifiIsUnusableEvent(WifiIsUnusableEvent.TYPE_DATA_STALL_BAD_TX);
    }

    /**
     * Verify there is no data stall if tx tput is above the threshold and Tx per is low
     */
    @Test
    public void verifyNoDataStallTxFailureWhenTxTputIsHigh() throws Exception {
        when(mWifiInfo.getLinkSpeed()).thenReturn(867);
        when(mClock.getElapsedSinceBootMillis()).thenReturn(10L);
        mNewLlStats.retries_be = mOldLlStats.retries_be;

        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        verifyUpdateWifiIsUnusableLinkLayerStats();
        when(mClock.getElapsedSinceBootMillis()).thenReturn(
                10L + DeviceConfigFacade.DEFAULT_DATA_STALL_DURATION_MS);
        setUpWifiBytes(TEST_WIFI_BYTES, TEST_WIFI_BYTES);
        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        assertEquals(true, mWifiDataStall.isThroughputSufficient());
        assertEquals(833132, mWifiDataStall.getTxThroughputKbps());
        assertEquals(9609, mWifiDataStall.getRxThroughputKbps());
        verify(mWifiMetrics, never()).logWifiIsUnusableEvent(
                WifiIsUnusableEvent.TYPE_DATA_STALL_BAD_TX);
    }

    /**
     * Verify there is no data stall from tx failures if tx is not consecutively bad
     */
    @Test
    public void verifyNoDataStallWhenTxFailureIsNotConsecutive() throws Exception {
        when(mClock.getElapsedSinceBootMillis()).thenReturn(10L);

        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        assertEquals(true, mWifiDataStall.isThroughputSufficient());
        verifyUpdateWifiIsUnusableLinkLayerStats();

        when(mClock.getElapsedSinceBootMillis()).thenReturn(
                10L + DeviceConfigFacade.DEFAULT_DATA_STALL_DURATION_MS);
        mNewLlStats.retries_be = 2 * mOldLlStats.retries_be;
        setUpWifiBytes(TEST_WIFI_BYTES, TEST_WIFI_BYTES);
        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        assertEquals(true, mWifiDataStall.isThroughputSufficient());
        verify(mWifiMetrics, never()).logWifiIsUnusableEvent(
                WifiIsUnusableEvent.TYPE_DATA_STALL_BAD_TX);
    }

    /**
     * Verify there is a data stall when Rx tput is low
     */
    @Test
    public void verifyDataStallRxFailure() throws Exception {
        when(mWifiInfo.getRxLinkSpeedMbps()).thenReturn(1);
        mNewLlStats.retries_be = 2 * mOldLlStats.retries_be;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(10L);
        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        verifyUpdateWifiIsUnusableLinkLayerStats();
        when(mClock.getElapsedSinceBootMillis()).thenReturn(
                10L + DeviceConfigFacade.DEFAULT_DATA_STALL_DURATION_MS);
        setUpWifiBytes(TEST_WIFI_BYTES, TEST_WIFI_BYTES);
        assertEquals(WifiIsUnusableEvent.TYPE_DATA_STALL_TX_WITHOUT_RX, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        assertEquals(false, mWifiDataStall.isThroughputSufficient());
        assertEquals(4804, mWifiDataStall.getTxThroughputKbps());
        assertEquals(960, mWifiDataStall.getRxThroughputKbps());
        verify(mWifiMetrics).logWifiIsUnusableEvent(
                WifiIsUnusableEvent.TYPE_DATA_STALL_TX_WITHOUT_RX);
    }

    /**
     * Verify there is no data stall and throughput is sufficient if there is no sufficient traffic
     */
    @Test
    public void verifyNoDataStallTrafficLow() throws Exception {
        when(mWifiInfo.getRxLinkSpeedMbps()).thenReturn(1);
        mNewLlStats.txmpdu_be = mOldLlStats.txmpdu_be + 1;
        mNewLlStats.retries_be = mOldLlStats.retries_be + 1;
        mNewLlStats.lostmpdu_be = mOldLlStats.lostmpdu_be + 1;
        mNewLlStats.rxmpdu_be = mOldLlStats.rxmpdu_be + 1;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(10L);

        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        verifyUpdateWifiIsUnusableLinkLayerStats();
        when(mClock.getElapsedSinceBootMillis()).thenReturn(
                10L + DeviceConfigFacade.DEFAULT_DATA_STALL_DURATION_MS);
        setUpWifiBytes(TEST_WIFI_BYTES, TEST_WIFI_BYTES);
        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        assertEquals(true, mWifiDataStall.isThroughputSufficient());
        assertEquals(9128, mWifiDataStall.getTxThroughputKbps());
        assertEquals(-1, mWifiDataStall.getRxThroughputKbps());
        verify(mWifiMetrics, never()).logWifiIsUnusableEvent(anyInt());
    }

    /**
     * Verify there is a data stall from low tx and rx throughput
     */
    @Test
    public void verifyDataStallBothTxRxFailure() throws Exception {
        // 1st poll with low tx and rx throughput
        when(mWifiInfo.getRxLinkSpeedMbps()).thenReturn(1);
        when(mClock.getElapsedSinceBootMillis()).thenReturn(10L);

        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        verifyUpdateWifiIsUnusableLinkLayerStats();
        assertEquals(960, mWifiDataStall.getTxThroughputKbps());
        assertEquals(960, mWifiDataStall.getRxThroughputKbps());

        // 2nd poll with low tx and rx throughput
        when(mClock.getElapsedSinceBootMillis()).thenReturn(
                10L + DeviceConfigFacade.DEFAULT_DATA_STALL_DURATION_MS);
        setUpWifiBytes(TEST_WIFI_BYTES, TEST_WIFI_BYTES);
        assertEquals(WifiIsUnusableEvent.TYPE_DATA_STALL_BOTH, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        assertEquals(false, mWifiDataStall.isThroughputSufficient());
        assertEquals(960, mWifiDataStall.getTxThroughputKbps());
        assertEquals(960, mWifiDataStall.getRxThroughputKbps());
        verify(mWifiMetrics).logWifiIsUnusableEvent(WifiIsUnusableEvent.TYPE_DATA_STALL_BOTH);

        // 3rd poll with low tx/rx traffic and throughput
        when(mWifiInfo.getLinkSpeed()).thenReturn(1);
        when(mWifiInfo.getRxLinkSpeedMbps()).thenReturn(1);
        mNewLlStats.txmpdu_be = mOldLlStats.txmpdu_be + 1;
        mNewLlStats.retries_be = mOldLlStats.retries_be + 1;
        mNewLlStats.lostmpdu_be = mOldLlStats.lostmpdu_be + 1;
        mNewLlStats.rxmpdu_be = mOldLlStats.rxmpdu_be + 1;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(
                10L + 2 * DeviceConfigFacade.DEFAULT_DATA_STALL_DURATION_MS);
        setUpWifiBytes(TEST_WIFI_BYTES, TEST_WIFI_BYTES);

        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        assertEquals(false, mWifiDataStall.isThroughputSufficient());
        assertEquals(960, mWifiDataStall.getTxThroughputKbps());
        assertEquals(960, mWifiDataStall.getRxThroughputKbps());

        // 4th poll with low tx/rx traffic, high throughput and unknown channel utilization
        when(mWifiChannelUtilization.getUtilizationRatio(anyInt())).thenReturn(-1);
        when(mWifiInfo.getLinkSpeed()).thenReturn(10);
        when(mWifiInfo.getRxLinkSpeedMbps()).thenReturn(10);
        when(mClock.getElapsedSinceBootMillis()).thenReturn(
                10L + 2 * DeviceConfigFacade.DEFAULT_DATA_STALL_DURATION_MS);
        setUpWifiBytes(TEST_WIFI_BYTES, TEST_WIFI_BYTES);
        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        assertEquals(true, mWifiDataStall.isThroughputSufficient());
        assertEquals(8943, mWifiDataStall.getTxThroughputKbps());
        assertEquals(9414, mWifiDataStall.getRxThroughputKbps());
    }

    /**
     * Verify that we can change the duration of evaluating Wifi conditions
     * to trigger data stall from DeviceConfigFacade
     */
    @Test
    public void verifyDataStallDurationDeviceConfigChange() throws Exception {
        when(mClock.getElapsedSinceBootMillis()).thenReturn(10L);
        when(mDeviceConfigFacade.getDataStallDurationMs()).thenReturn(
                DeviceConfigFacade.DEFAULT_DATA_STALL_DURATION_MS + 1);
        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        verifyUpdateWifiIsUnusableLinkLayerStats();
        when(mClock.getElapsedSinceBootMillis()).thenReturn(
                10L + DeviceConfigFacade.DEFAULT_DATA_STALL_DURATION_MS);
        setUpWifiBytes(TEST_WIFI_BYTES, TEST_WIFI_BYTES);
        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        assertEquals(false, mWifiDataStall.isThroughputSufficient());
        verify(mWifiMetrics, never()).logWifiIsUnusableEvent(
                WifiIsUnusableEvent.TYPE_DATA_STALL_BAD_TX);
    }

    /**
     * Verify that we can change the threshold of Tx packet error rate to trigger a data stall
     * from DeviceConfigFacade
     */
    @Test
    public void verifyDataStallTxPerThrDeviceConfigChange() throws Exception {
        when(mClock.getElapsedSinceBootMillis()).thenReturn(10L);
        when(mDeviceConfigFacade.getDataStallTxPerThr()).thenReturn(
                DeviceConfigFacade.DEFAULT_DATA_STALL_TX_PER_THR + 1);
        when(mDeviceConfigFacade.getDataStallTxTputThrKbps()).thenReturn(800);
        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        verifyUpdateWifiIsUnusableLinkLayerStats();
        when(mClock.getElapsedSinceBootMillis()).thenReturn(
                10L + DeviceConfigFacade.DEFAULT_DATA_STALL_DURATION_MS);
        setUpWifiBytes(TEST_WIFI_BYTES, TEST_WIFI_BYTES);
        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        assertEquals(false, mWifiDataStall.isThroughputSufficient());
        verify(mWifiMetrics, never()).logWifiIsUnusableEvent(
                WifiIsUnusableEvent.TYPE_DATA_STALL_BAD_TX);
    }

    /**
     * Verify there is no data stall when there are no new tx/rx failures
     */
    @Test
    public void verifyNoDataStallWhenNoFail() throws Exception {
        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        verify(mWifiMetrics, never()).resetWifiIsUnusableLinkLayerStats();
        verifyUpdateWifiIsUnusableLinkLayerStats();
        verify(mWifiMetrics, never()).logWifiIsUnusableEvent(anyInt());
    }

    /**
     * Verify there is no data stall when the time difference between
     * two WifiLinkLayerStats is too big.
     */
    @Test
    public void verifyNoDataStallBigTimeGap() throws Exception {
        mNewLlStats.lostmpdu_be = mOldLlStats.lostmpdu_be + TEST_MIN_TX_BAD;
        mNewLlStats.timeStampInMs = mOldLlStats.timeStampInMs
                + WifiDataStall.MAX_MS_DELTA_FOR_DATA_STALL + 1;
        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        verifyUpdateWifiIsUnusableLinkLayerStats();
        verify(mWifiMetrics, never()).logWifiIsUnusableEvent(anyInt());
    }

    /**
     * Verify that metrics get reset when there is a reset in WifiLinkLayerStats
     */
    @Test
    public void verifyReset() throws Exception {
        mNewLlStats.lostmpdu_be = mOldLlStats.lostmpdu_be - 1;
        assertEquals(WifiIsUnusableEvent.TYPE_UNKNOWN, mWifiDataStall
                .checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo));
        verify(mWifiMetrics).resetWifiIsUnusableLinkLayerStats();
        verify(mWifiMetrics, never()).updateWifiIsUnusableLinkLayerStats(
                anyLong(), anyLong(), anyLong(), anyLong(), anyLong());
        verify(mWifiMetrics, never()).logWifiIsUnusableEvent(anyInt());
    }

    /**
     * Check incrementConnectionDuration under various conditions
     */
    @Test
    public void testIncrementConnectionDuration() throws Exception {
        mWifiDataStall.enablePhoneStateListener();
        PhoneStateListener phoneStateListener = mockPhoneStateListener();
        phoneStateListener.onDataConnectionStateChanged(TelephonyManager.DATA_CONNECTED,
                TelephonyManager.NETWORK_TYPE_LTE);
        assertEquals(true, mWifiDataStall.isCellularDataAvailable());
        mNewLlStats.timeStampInMs = mOldLlStats.timeStampInMs + 1000;
        // Expect 1st throughput sufficiency check to return true
        // because it hits mLastTxBytes == 0 || mLastRxBytes == 0
        mWifiDataStall.checkDataStallAndThroughputSufficiency(
                mOldLlStats, mNewLlStats, mWifiInfo);
        verify(mWifiMetrics, times(1)).incrementConnectionDuration(
                1000, true, true);

        // Expect 2nd throughput sufficiency check to return false
        mWifiDataStall.checkDataStallAndThroughputSufficiency(
                mOldLlStats, mNewLlStats, mWifiInfo);
        verify(mWifiMetrics, times(1)).incrementConnectionDuration(
                1000, false, true);


        mNewLlStats.timeStampInMs = mOldLlStats.timeStampInMs + 2000;
        phoneStateListener.onDataConnectionStateChanged(
                TelephonyManager.DATA_DISCONNECTED, TelephonyManager.NETWORK_TYPE_LTE);
        assertEquals(false, mWifiDataStall.isCellularDataAvailable());
        mWifiDataStall.checkDataStallAndThroughputSufficiency(
                mOldLlStats, mNewLlStats, mWifiInfo);
        verify(mWifiMetrics, times(1)).incrementConnectionDuration(
                2000, false, false);

        // Expect this update to be ignored by connection duration counters due to its
        // too large poll interval
        mNewLlStats.timeStampInMs = mOldLlStats.timeStampInMs + 10000;
        mWifiDataStall.checkDataStallAndThroughputSufficiency(
                mOldLlStats, mNewLlStats, mWifiInfo);
        verify(mWifiMetrics, never()).incrementConnectionDuration(
                10000, false, false);
        mWifiDataStall.disablePhoneStateListener();
    }

    /**
     * Check statsd logging
     */
    @Test
    public void testWifiStatsLogWrite() throws Exception {
        mWifiDataStall.enableVerboseLogging(true);
        mWifiDataStall.enablePhoneStateListener();
        PhoneStateListener phoneStateListener = mockPhoneStateListener();
        phoneStateListener.onDataConnectionStateChanged(TelephonyManager.DATA_CONNECTED,
                TelephonyManager.NETWORK_TYPE_LTE);
        // static mocking for WifiStatsLog
        mSession = ExtendedMockito.mockitoSession()
                .strictness(Strictness.LENIENT)
                .mockStatic(WifiStatsLog.class)
                .startMocking();
        mNewLlStats.timeStampInMs = mOldLlStats.timeStampInMs + 3000;
        when(mWifiInfo.getFrequency()).thenReturn(5850);
        mWifiDataStall.checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo);
        ExtendedMockito.verify(() -> WifiStatsLog.write(
                WifiStatsLog.WIFI_HEALTH_STAT_REPORTED, 3000, true, true,
                WifiStatsLog.WIFI_HEALTH_STAT_REPORTED__BAND__BAND_5G_HIGH));

        mNewLlStats.timeStampInMs = mOldLlStats.timeStampInMs + 2000;
        when(mWifiInfo.getFrequency()).thenReturn(6850);
        mWifiDataStall.checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo);
        ExtendedMockito.verify(() -> WifiStatsLog.write(
                WifiStatsLog.WIFI_HEALTH_STAT_REPORTED, 2000, true, true,
                WifiStatsLog.WIFI_HEALTH_STAT_REPORTED__BAND__BAND_6G_MIDDLE));

        mNewLlStats.timeStampInMs = mOldLlStats.timeStampInMs + 1000;
        when(mWifiInfo.getFrequency()).thenReturn(1850);
        mWifiDataStall.checkDataStallAndThroughputSufficiency(mOldLlStats, mNewLlStats, mWifiInfo);
        ExtendedMockito.verify(() -> WifiStatsLog.write(
                WifiStatsLog.WIFI_HEALTH_STAT_REPORTED, 1000, true, true,
                WifiStatsLog.WIFI_HEALTH_STAT_REPORTED__BAND__UNKNOWN));
        mSession.finishMocking();
        mWifiDataStall.disablePhoneStateListener();
    }
}
