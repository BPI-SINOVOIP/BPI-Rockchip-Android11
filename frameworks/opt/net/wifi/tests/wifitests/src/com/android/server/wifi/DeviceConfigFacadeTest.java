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

package com.android.server.wifi;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.verify;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.*;
import static org.mockito.MockitoAnnotations.*;

import android.app.test.MockAnswerUtil.AnswerWithArguments;
import android.content.Context;
import android.os.Handler;
import android.os.test.TestLooper;
import android.provider.DeviceConfig;
import android.provider.DeviceConfig.OnPropertiesChangedListener;
import android.util.ArraySet;

import androidx.test.filters.SmallTest;

import com.android.dx.mockito.inline.extended.ExtendedMockito;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.MockitoSession;

import java.util.Collections;
import java.util.Set;


/**
 * Unit tests for {@link com.android.server.wifi.DeviceConfigFacade}.
 */
@SmallTest
public class DeviceConfigFacadeTest extends WifiBaseTest {
    @Mock Context mContext;
    @Mock WifiMetrics mWifiMetrics;

    final ArgumentCaptor<OnPropertiesChangedListener> mOnPropertiesChangedListenerCaptor =
            ArgumentCaptor.forClass(OnPropertiesChangedListener.class);

    private DeviceConfigFacade mDeviceConfigFacade;
    private TestLooper mLooper = new TestLooper();
    private MockitoSession mSession;

    /**
     * Setup the mocks and an instance of WifiConfigManager before each test.
     */
    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        // static mocking
        mSession = ExtendedMockito.mockitoSession()
                .mockStatic(DeviceConfig.class, withSettings().lenient())
                .startMocking();
        // Have DeviceConfig return the default value passed in.
        when(DeviceConfig.getBoolean(anyString(), anyString(), anyBoolean()))
                .then(new AnswerWithArguments() {
                    public boolean answer(String namespace, String field, boolean def) {
                        return def;
                    }
                });
        when(DeviceConfig.getInt(anyString(), anyString(), anyInt()))
                .then(new AnswerWithArguments() {
                    public int answer(String namespace, String field, int def) {
                        return def;
                    }
                });
        when(DeviceConfig.getLong(anyString(), anyString(), anyLong()))
                .then(new AnswerWithArguments() {
                    public long answer(String namespace, String field, long def) {
                        return def;
                    }
                });
        when(DeviceConfig.getString(anyString(), anyString(), anyString()))
                .then(new AnswerWithArguments() {
                    public String answer(String namespace, String field, String def) {
                        return def;
                    }
                });

        mDeviceConfigFacade = new DeviceConfigFacade(mContext, new Handler(mLooper.getLooper()),
                mWifiMetrics);
        verify(() -> DeviceConfig.addOnPropertiesChangedListener(anyString(), any(),
                mOnPropertiesChangedListenerCaptor.capture()));
    }

    /**
     * Called after each test
     */
    @After
    public void cleanup() {
        validateMockitoUsage();
        mSession.finishMocking();
    }

    /**
     * Verifies that default values are set correctly
     */
    @Test
    public void testDefaultValue() throws Exception {
        assertEquals(false, mDeviceConfigFacade.isAbnormalConnectionBugreportEnabled());
        assertEquals(DeviceConfigFacade.DEFAULT_ABNORMAL_CONNECTION_DURATION_MS,
                mDeviceConfigFacade.getAbnormalConnectionDurationMs());
        assertEquals(DeviceConfigFacade.DEFAULT_DATA_STALL_DURATION_MS,
                mDeviceConfigFacade.getDataStallDurationMs());
        assertEquals(DeviceConfigFacade.DEFAULT_DATA_STALL_TX_TPUT_THR_KBPS,
                mDeviceConfigFacade.getDataStallTxTputThrKbps());
        assertEquals(DeviceConfigFacade.DEFAULT_DATA_STALL_RX_TPUT_THR_KBPS,
                mDeviceConfigFacade.getDataStallRxTputThrKbps());
        assertEquals(DeviceConfigFacade.DEFAULT_DATA_STALL_TX_PER_THR,
                mDeviceConfigFacade.getDataStallTxPerThr());
        assertEquals(DeviceConfigFacade.DEFAULT_DATA_STALL_CCA_LEVEL_THR,
                mDeviceConfigFacade.getDataStallCcaLevelThr());
        assertEquals(DeviceConfigFacade.DEFAULT_TX_TPUT_SUFFICIENT_THR_LOW_KBPS,
                mDeviceConfigFacade.getTxTputSufficientLowThrKbps());
        assertEquals(DeviceConfigFacade.DEFAULT_TX_TPUT_SUFFICIENT_THR_HIGH_KBPS,
                mDeviceConfigFacade.getTxTputSufficientHighThrKbps());
        assertEquals(DeviceConfigFacade.DEFAULT_RX_TPUT_SUFFICIENT_THR_LOW_KBPS,
                mDeviceConfigFacade.getRxTputSufficientLowThrKbps());
        assertEquals(DeviceConfigFacade.DEFAULT_RX_TPUT_SUFFICIENT_THR_HIGH_KBPS,
                mDeviceConfigFacade.getRxTputSufficientHighThrKbps());
        assertEquals(DeviceConfigFacade.DEFAULT_TPUT_SUFFICIENT_RATIO_THR_NUM,
                mDeviceConfigFacade.getTputSufficientRatioThrNum());
        assertEquals(DeviceConfigFacade.DEFAULT_TPUT_SUFFICIENT_RATIO_THR_DEN,
                mDeviceConfigFacade.getTputSufficientRatioThrDen());
        assertEquals(DeviceConfigFacade.DEFAULT_TX_PACKET_PER_SECOND_THR,
                mDeviceConfigFacade.getTxPktPerSecondThr());
        assertEquals(DeviceConfigFacade.DEFAULT_RX_PACKET_PER_SECOND_THR,
                mDeviceConfigFacade.getRxPktPerSecondThr());
        assertEquals(DeviceConfigFacade.DEFAULT_CONNECTION_FAILURE_HIGH_THR_PERCENT,
                mDeviceConfigFacade.getConnectionFailureHighThrPercent());
        assertEquals(DeviceConfigFacade.DEFAULT_CONNECTION_FAILURE_COUNT_MIN,
                mDeviceConfigFacade.getConnectionFailureCountMin());
        assertEquals(DeviceConfigFacade.DEFAULT_ASSOC_REJECTION_HIGH_THR_PERCENT,
                mDeviceConfigFacade.getAssocRejectionHighThrPercent());
        assertEquals(DeviceConfigFacade.DEFAULT_ASSOC_REJECTION_COUNT_MIN,
                mDeviceConfigFacade.getAssocRejectionCountMin());
        assertEquals(DeviceConfigFacade.DEFAULT_ASSOC_TIMEOUT_HIGH_THR_PERCENT,
                mDeviceConfigFacade.getAssocTimeoutHighThrPercent());
        assertEquals(DeviceConfigFacade.DEFAULT_ASSOC_TIMEOUT_COUNT_MIN,
                mDeviceConfigFacade.getAssocTimeoutCountMin());
        assertEquals(DeviceConfigFacade.DEFAULT_AUTH_FAILURE_HIGH_THR_PERCENT,
                mDeviceConfigFacade.getAuthFailureHighThrPercent());
        assertEquals(DeviceConfigFacade.DEFAULT_AUTH_FAILURE_COUNT_MIN,
                mDeviceConfigFacade.getAuthFailureCountMin());
        assertEquals(DeviceConfigFacade.DEFAULT_SHORT_CONNECTION_NONLOCAL_HIGH_THR_PERCENT,
                mDeviceConfigFacade.getShortConnectionNonlocalHighThrPercent());
        assertEquals(DeviceConfigFacade.DEFAULT_SHORT_CONNECTION_NONLOCAL_COUNT_MIN,
                mDeviceConfigFacade.getShortConnectionNonlocalCountMin());
        assertEquals(DeviceConfigFacade.DEFAULT_DISCONNECTION_NONLOCAL_HIGH_THR_PERCENT,
                mDeviceConfigFacade.getDisconnectionNonlocalHighThrPercent());
        assertEquals(DeviceConfigFacade.DEFAULT_DISCONNECTION_NONLOCAL_COUNT_MIN,
                mDeviceConfigFacade.getDisconnectionNonlocalCountMin());
        assertEquals(DeviceConfigFacade.DEFAULT_HEALTH_MONITOR_RATIO_THR_NUMERATOR,
                mDeviceConfigFacade.getHealthMonitorRatioThrNumerator());
        assertEquals(DeviceConfigFacade.DEFAULT_HEALTH_MONITOR_MIN_RSSI_THR_DBM,
                mDeviceConfigFacade.getHealthMonitorMinRssiThrDbm());
        assertEquals(Collections.emptySet(),
                mDeviceConfigFacade.getRandomizationFlakySsidHotlist());
        assertEquals(Collections.emptySet(),
                mDeviceConfigFacade.getAggressiveMacRandomizationSsidAllowlist());
        assertEquals(Collections.emptySet(),
                mDeviceConfigFacade.getAggressiveMacRandomizationSsidBlocklist());
        assertEquals(false, mDeviceConfigFacade.isAbnormalConnectionFailureBugreportEnabled());
        assertEquals(false, mDeviceConfigFacade.isAbnormalDisconnectionBugreportEnabled());
        assertEquals(DeviceConfigFacade.DEFAULT_HEALTH_MONITOR_MIN_NUM_CONNECTION_ATTEMPT,
                mDeviceConfigFacade.getHealthMonitorMinNumConnectionAttempt());
        assertEquals(DeviceConfigFacade.DEFAULT_BUG_REPORT_MIN_WINDOW_MS,
                mDeviceConfigFacade.getBugReportMinWindowMs());
        assertEquals(false, mDeviceConfigFacade.isOverlappingConnectionBugreportEnabled());
        assertEquals(DeviceConfigFacade.DEFAULT_OVERLAPPING_CONNECTION_DURATION_THRESHOLD_MS,
                mDeviceConfigFacade.getOverlappingConnectionDurationThresholdMs());
        assertEquals(DeviceConfigFacade.DEFAULT_TX_LINK_SPEED_LOW_THRESHOLD_MBPS,
                mDeviceConfigFacade.getTxLinkSpeedLowThresholdMbps());
        assertEquals(DeviceConfigFacade.DEFAULT_RX_LINK_SPEED_LOW_THRESHOLD_MBPS,
                mDeviceConfigFacade.getRxLinkSpeedLowThresholdMbps());
        assertEquals(DeviceConfigFacade.DEFAULT_HEALTH_MONITOR_RSSI_POLL_VALID_TIME_MS,
                mDeviceConfigFacade.getHealthMonitorRssiPollValidTimeMs());
        assertEquals(DeviceConfigFacade.DEFAULT_HEALTH_MONITOR_SHORT_CONNECTION_DURATION_THR_MS,
                mDeviceConfigFacade.getHealthMonitorShortConnectionDurationThrMs());
        assertEquals(DeviceConfigFacade.DEFAULT_ABNORMAL_DISCONNECTION_REASON_CODE_MASK,
                mDeviceConfigFacade.getAbnormalDisconnectionReasonCodeMask());
        assertEquals(DeviceConfigFacade.DEFAULT_NONSTATIONARY_SCAN_RSSI_VALID_TIME_MS,
                mDeviceConfigFacade.getNonstationaryScanRssiValidTimeMs());
        assertEquals(DeviceConfigFacade.DEFAULT_STATIONARY_SCAN_RSSI_VALID_TIME_MS,
                mDeviceConfigFacade.getStationaryScanRssiValidTimeMs());
        assertEquals(DeviceConfigFacade.DEFAULT_HEALTH_MONITOR_FW_ALERT_VALID_TIME_MS,
                mDeviceConfigFacade.getHealthMonitorFwAlertValidTimeMs());
        assertEquals(DeviceConfigFacade.DEFAULT_MIN_CONFIRMATION_DURATION_SEND_LOW_SCORE_MS,
                mDeviceConfigFacade.getMinConfirmationDurationSendLowScoreMs());
        assertEquals(DeviceConfigFacade.DEFAULT_MIN_CONFIRMATION_DURATION_SEND_HIGH_SCORE_MS,
                mDeviceConfigFacade.getMinConfirmationDurationSendHighScoreMs());
        assertEquals(DeviceConfigFacade.DEFAULT_RSSI_THRESHOLD_NOT_SEND_LOW_SCORE_TO_CS_DBM,
                mDeviceConfigFacade.getRssiThresholdNotSendLowScoreToCsDbm());
    }

    /**
     * Verifies that all fields are updated properly.
     */
    @Test
    public void testFieldUpdates() throws Exception {
        // Simulate updating the fields
        when(DeviceConfig.getBoolean(anyString(), eq("abnormal_connection_bugreport_enabled"),
                anyBoolean())).thenReturn(true);
        when(DeviceConfig.getInt(anyString(), eq("abnormal_connection_duration_ms"),
                anyInt())).thenReturn(100);
        when(DeviceConfig.getInt(anyString(), eq("data_stall_duration_ms"),
                anyInt())).thenReturn(0);
        when(DeviceConfig.getInt(anyString(), eq("data_stall_tx_tput_thr_kbps"),
                anyInt())).thenReturn(1000);
        when(DeviceConfig.getInt(anyString(), eq("data_stall_rx_tput_thr_kbps"),
                anyInt())).thenReturn(1500);
        when(DeviceConfig.getInt(anyString(), eq("data_stall_tx_per_thr"),
                anyInt())).thenReturn(95);
        when(DeviceConfig.getInt(anyString(), eq("data_stall_cca_level_thr"),
                anyInt())).thenReturn(80);
        when(DeviceConfig.getInt(anyString(), eq("tput_sufficient_low_thr_kbps"),
                anyInt())).thenReturn(4000);
        when(DeviceConfig.getInt(anyString(), eq("tput_sufficient_high_thr_kbps"),
                anyInt())).thenReturn(8000);
        when(DeviceConfig.getInt(anyString(), eq("rx_tput_sufficient_low_thr_kbps"),
                anyInt())).thenReturn(5000);
        when(DeviceConfig.getInt(anyString(), eq("rx_tput_sufficient_high_thr_kbps"),
                anyInt())).thenReturn(9000);
        when(DeviceConfig.getInt(anyString(), eq("tput_sufficient_ratio_thr_num"),
                anyInt())).thenReturn(3);
        when(DeviceConfig.getInt(anyString(), eq("tput_sufficient_ratio_thr_den"),
                anyInt())).thenReturn(2);
        when(DeviceConfig.getInt(anyString(), eq("tx_pkt_per_second_thr"),
                anyInt())).thenReturn(10);
        when(DeviceConfig.getInt(anyString(), eq("rx_pkt_per_second_thr"),
                anyInt())).thenReturn(5);
        when(DeviceConfig.getInt(anyString(), eq("connection_failure_high_thr_percent"),
                anyInt())).thenReturn(31);
        when(DeviceConfig.getInt(anyString(), eq("connection_failure_count_min"),
                anyInt())).thenReturn(4);
        when(DeviceConfig.getInt(anyString(), eq("assoc_rejection_high_thr_percent"),
                anyInt())).thenReturn(10);
        when(DeviceConfig.getInt(anyString(), eq("assoc_rejection_count_min"),
                anyInt())).thenReturn(5);
        when(DeviceConfig.getInt(anyString(), eq("assoc_timeout_high_thr_percent"),
                anyInt())).thenReturn(12);
        when(DeviceConfig.getInt(anyString(), eq("assoc_timeout_count_min"),
                anyInt())).thenReturn(6);
        when(DeviceConfig.getInt(anyString(), eq("auth_failure_high_thr_percent"),
                anyInt())).thenReturn(11);
        when(DeviceConfig.getInt(anyString(), eq("auth_failure_count_min"),
                anyInt())).thenReturn(7);
        when(DeviceConfig.getInt(anyString(), eq("short_connection_nonlocal_high_thr_percent"),
                anyInt())).thenReturn(8);
        when(DeviceConfig.getInt(anyString(), eq("short_connection_nonlocal_count_min"),
                anyInt())).thenReturn(8);
        when(DeviceConfig.getInt(anyString(), eq("disconnection_nonlocal_high_thr_percent"),
                anyInt())).thenReturn(12);
        when(DeviceConfig.getInt(anyString(), eq("disconnection_nonlocal_count_min"),
                anyInt())).thenReturn(9);
        when(DeviceConfig.getInt(anyString(), eq("health_monitor_ratio_thr_numerator"),
                anyInt())).thenReturn(3);
        when(DeviceConfig.getInt(anyString(), eq("health_monitor_min_rssi_thr_dbm"),
                anyInt())).thenReturn(-67);
        String testSsidList = "ssid_1,ssid_2";
        when(DeviceConfig.getString(anyString(), eq("randomization_flaky_ssid_hotlist"),
                anyString())).thenReturn(testSsidList);
        when(DeviceConfig.getString(anyString(), eq("aggressive_randomization_ssid_allowlist"),
                anyString())).thenReturn(testSsidList);
        when(DeviceConfig.getString(anyString(), eq("aggressive_randomization_ssid_blocklist"),
                anyString())).thenReturn(testSsidList);
        when(DeviceConfig.getBoolean(anyString(),
                eq("abnormal_connection_failure_bugreport_enabled"),
                anyBoolean())).thenReturn(true);
        when(DeviceConfig.getBoolean(anyString(), eq("abnormal_disconnection_bugreport_enabled"),
                anyBoolean())).thenReturn(true);
        when(DeviceConfig.getInt(anyString(), eq("health_monitor_min_num_connection_attempt"),
                anyInt())).thenReturn(20);
        when(DeviceConfig.getInt(anyString(), eq("bug_report_min_window_ms"),
                anyInt())).thenReturn(1000);
        when(DeviceConfig.getBoolean(anyString(), eq("overlapping_connection_bugreport_enabled"),
                anyBoolean())).thenReturn(true);
        when(DeviceConfig.getInt(anyString(), eq("overlapping_connection_duration_threshold_ms"),
                anyInt())).thenReturn(50000);
        when(DeviceConfig.getInt(anyString(), eq("tx_link_speed_low_threshold_mbps"),
                anyInt())).thenReturn(9);
        when(DeviceConfig.getInt(anyString(), eq("rx_link_speed_low_threshold_mbps"),
                anyInt())).thenReturn(10);
        when(DeviceConfig.getInt(anyString(), eq("health_monitor_short_connection_duration_thr_ms"),
                anyInt())).thenReturn(30_000);
        when(DeviceConfig.getLong(anyString(), eq("abnormal_disconnection_reason_code_mask"),
                anyLong())).thenReturn(0xffff_fff3_0000_ffffL);
        when(DeviceConfig.getInt(anyString(), eq("health_monitor_rssi_poll_valid_time_ms"),
                anyInt())).thenReturn(2000);
        when(DeviceConfig.getInt(anyString(), eq("nonstationary_scan_rssi_valid_time_ms"),
                anyInt())).thenReturn(4000);
        when(DeviceConfig.getInt(anyString(), eq("stationary_scan_rssi_valid_time_ms"),
                anyInt())).thenReturn(3000);
        when(DeviceConfig.getInt(anyString(), eq("health_monitor_fw_alert_valid_time_ms"),
                anyInt())).thenReturn(1000);
        when(DeviceConfig.getInt(anyString(), eq("min_confirmation_duration_send_low_score_ms"),
                anyInt())).thenReturn(4000);
        when(DeviceConfig.getInt(anyString(), eq("min_confirmation_duration_send_high_score_ms"),
                anyInt())).thenReturn(1000);
        when(DeviceConfig.getInt(anyString(), eq("rssi_threshold_not_send_low_score_to_cs_dbm"),
                anyInt())).thenReturn(-70);
        mOnPropertiesChangedListenerCaptor.getValue().onPropertiesChanged(null);

        // Verifying fields are updated to the new values
        Set<String> testSsidSet = new ArraySet<>();
        testSsidSet.add("\"ssid_1\"");
        testSsidSet.add("\"ssid_2\"");
        assertEquals(true, mDeviceConfigFacade.isAbnormalConnectionBugreportEnabled());
        assertEquals(100, mDeviceConfigFacade.getAbnormalConnectionDurationMs());
        assertEquals(0, mDeviceConfigFacade.getDataStallDurationMs());
        assertEquals(1000, mDeviceConfigFacade.getDataStallTxTputThrKbps());
        assertEquals(1500, mDeviceConfigFacade.getDataStallRxTputThrKbps());
        assertEquals(95, mDeviceConfigFacade.getDataStallTxPerThr());
        assertEquals(80, mDeviceConfigFacade.getDataStallCcaLevelThr());
        assertEquals(4000, mDeviceConfigFacade.getTxTputSufficientLowThrKbps());
        assertEquals(8000, mDeviceConfigFacade.getTxTputSufficientHighThrKbps());
        assertEquals(5000, mDeviceConfigFacade.getRxTputSufficientLowThrKbps());
        assertEquals(9000, mDeviceConfigFacade.getRxTputSufficientHighThrKbps());
        assertEquals(3, mDeviceConfigFacade.getTputSufficientRatioThrNum());
        assertEquals(2, mDeviceConfigFacade.getTputSufficientRatioThrDen());
        assertEquals(10, mDeviceConfigFacade.getTxPktPerSecondThr());
        assertEquals(5, mDeviceConfigFacade.getRxPktPerSecondThr());
        assertEquals(31, mDeviceConfigFacade.getConnectionFailureHighThrPercent());
        assertEquals(4, mDeviceConfigFacade.getConnectionFailureCountMin());
        assertEquals(10, mDeviceConfigFacade.getAssocRejectionHighThrPercent());
        assertEquals(5, mDeviceConfigFacade.getAssocRejectionCountMin());
        assertEquals(12, mDeviceConfigFacade.getAssocTimeoutHighThrPercent());
        assertEquals(6, mDeviceConfigFacade.getAssocTimeoutCountMin());
        assertEquals(11, mDeviceConfigFacade.getAuthFailureHighThrPercent());
        assertEquals(7, mDeviceConfigFacade.getAuthFailureCountMin());
        assertEquals(8, mDeviceConfigFacade.getShortConnectionNonlocalHighThrPercent());
        assertEquals(8, mDeviceConfigFacade.getShortConnectionNonlocalCountMin());
        assertEquals(12, mDeviceConfigFacade.getDisconnectionNonlocalHighThrPercent());
        assertEquals(9, mDeviceConfigFacade.getDisconnectionNonlocalCountMin());
        assertEquals(3, mDeviceConfigFacade.getHealthMonitorRatioThrNumerator());
        assertEquals(-67, mDeviceConfigFacade.getHealthMonitorMinRssiThrDbm());
        assertEquals(testSsidSet, mDeviceConfigFacade.getRandomizationFlakySsidHotlist());
        assertEquals(testSsidSet,
                mDeviceConfigFacade.getAggressiveMacRandomizationSsidAllowlist());
        assertEquals(testSsidSet,
                mDeviceConfigFacade.getAggressiveMacRandomizationSsidBlocklist());
        assertEquals(true, mDeviceConfigFacade.isAbnormalConnectionFailureBugreportEnabled());
        assertEquals(true, mDeviceConfigFacade.isAbnormalDisconnectionBugreportEnabled());
        assertEquals(20, mDeviceConfigFacade.getHealthMonitorMinNumConnectionAttempt());
        assertEquals(1000, mDeviceConfigFacade.getBugReportMinWindowMs());
        assertEquals(true, mDeviceConfigFacade.isOverlappingConnectionBugreportEnabled());
        assertEquals(50000, mDeviceConfigFacade.getOverlappingConnectionDurationThresholdMs());
        assertEquals(9, mDeviceConfigFacade.getTxLinkSpeedLowThresholdMbps());
        assertEquals(10, mDeviceConfigFacade.getRxLinkSpeedLowThresholdMbps());
        assertEquals(30_000,
                mDeviceConfigFacade.getHealthMonitorShortConnectionDurationThrMs());
        assertEquals(0xffff_fff3_0000_ffffL,
                mDeviceConfigFacade.getAbnormalDisconnectionReasonCodeMask());
        assertEquals(2000, mDeviceConfigFacade.getHealthMonitorRssiPollValidTimeMs());
        assertEquals(4000, mDeviceConfigFacade.getNonstationaryScanRssiValidTimeMs());
        assertEquals(3000, mDeviceConfigFacade.getStationaryScanRssiValidTimeMs());
        assertEquals(1000, mDeviceConfigFacade.getHealthMonitorFwAlertValidTimeMs());
        assertEquals(4000, mDeviceConfigFacade.getMinConfirmationDurationSendLowScoreMs());
        assertEquals(1000, mDeviceConfigFacade.getMinConfirmationDurationSendHighScoreMs());
        assertEquals(-70, mDeviceConfigFacade.getRssiThresholdNotSendLowScoreToCsDbm());
    }
}
