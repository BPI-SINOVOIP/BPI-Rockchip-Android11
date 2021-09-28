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
import static org.mockito.Mockito.*;

import android.os.BatteryStatsManager;
import android.os.connectivity.WifiBatteryStats;
import android.text.format.DateUtils;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.proto.nano.WifiMetricsProto.WifiPowerStats;
import com.android.server.wifi.proto.nano.WifiMetricsProto.WifiRadioUsage;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.io.ByteArrayOutputStream;
import java.io.PrintWriter;

/**
 * Unit tests for {@link com.android.server.wifi.WifiPowerMetrics}.
 */
@SmallTest
public class WifiPowerMetricsTest extends WifiBaseTest {
    @Mock
    BatteryStatsManager mBatteryStats;
    WifiPowerMetrics mWifiPowerMetrics;

    private static final long DEFAULT_VALUE = 0;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mWifiPowerMetrics = new WifiPowerMetrics(mBatteryStats);
    }

    /**
     * Tests that WifiRadioUsage has its fields set according to the corresponding fields in
     * WifiBatteryStats
     * @throws Exception
     */
    @Test
    public void testBuildWifiRadioUsageProto() throws Exception {
        final long loggingDuration = 280;
        final long scanTime = 23;
        WifiBatteryStats wifiBatteryStats = mock(WifiBatteryStats.class);
        when(wifiBatteryStats.getLoggingDurationMillis()).thenReturn(loggingDuration);
        when(wifiBatteryStats.getScanTimeMillis()).thenReturn(scanTime);
        when(mBatteryStats.getWifiBatteryStats()).thenReturn(wifiBatteryStats);
        WifiRadioUsage wifiRadioUsage = mWifiPowerMetrics.buildWifiRadioUsageProto();
        verify(mBatteryStats).getWifiBatteryStats();
        assertEquals("loggingDurationMs must match with field from WifiBatteryStats",
                loggingDuration, wifiRadioUsage.loggingDurationMs);
        assertEquals("scanTimeMs must match with field from WifiBatteryStats",
                scanTime, wifiRadioUsage.scanTimeMs);
    }

    /**
     * Tests that WifiRadioUsage has its fields set to the |DEFAULT_VALUE| when BatteryStatsManager
     * returns null
     * @throws Exception
     */
    @Test
    public void testBuildWifiRadioUsageProtoNull() throws Exception {
        when(mBatteryStats.getWifiBatteryStats()).thenReturn(null);
        WifiRadioUsage wifiRadioUsage = mWifiPowerMetrics.buildWifiRadioUsageProto();
        verify(mBatteryStats).getWifiBatteryStats();
        assertEquals("loggingDurationMs must be default value when getWifiBatteryStats returns "
                        + "null", DEFAULT_VALUE, wifiRadioUsage.loggingDurationMs);
        assertEquals("scanTimeMs must be default value when getWifiBatteryStats returns null",
                DEFAULT_VALUE, wifiRadioUsage.scanTimeMs);
    }

    /**
     * Tests that dump() pulls data from BatteryStatsManager
     * @throws Exception
     */
    @Test
    public void testDumpCallsAppropriateMethods() throws Exception {
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        PrintWriter writer = new PrintWriter(stream);
        mWifiPowerMetrics.dump(writer);
        verify(mBatteryStats, atLeastOnce()).getWifiBatteryStats();
    }

    /**
     * Tests that WifiPowerStats has its fields set according to the corresponding fields in
     * WifiBatteryStats
     * @throws Exception
     */
    @Test
    public void testBuildProto() throws Exception {
        final long monitoredRailEnergyConsumedMaMs = 12000;
        final long numBytesTx = 65000;
        final long numBytesRx = 4560000;
        final long numPacketsTx = 3456;
        final long numPacketsRx = 5436456;
        final long txTimeMs = 2300;
        final long rxTimeMs = 343258;
        final long idleTimeMs = 32322233;
        final long scanTimeMs = 345566;
        final long sleepTimeMs = 323270343;
        final double monitoredRailEnergyConsumedMah = monitoredRailEnergyConsumedMaMs
                / ((double) DateUtils.HOUR_IN_MILLIS);
        WifiBatteryStats wifiBatteryStats = mock(WifiBatteryStats.class);
        when(wifiBatteryStats.getEnergyConsumedMaMillis())
                .thenReturn(monitoredRailEnergyConsumedMaMs);
        when(wifiBatteryStats.getNumBytesTx()).thenReturn(numBytesTx);
        when(wifiBatteryStats.getNumBytesRx()).thenReturn(numBytesRx);
        when(wifiBatteryStats.getNumPacketsTx()).thenReturn(numPacketsTx);
        when(wifiBatteryStats.getNumPacketsRx()).thenReturn(numPacketsRx);
        when(wifiBatteryStats.getTxTimeMillis()).thenReturn(txTimeMs);
        when(wifiBatteryStats.getRxTimeMillis()).thenReturn(rxTimeMs);
        when(wifiBatteryStats.getIdleTimeMillis()).thenReturn(idleTimeMs);
        when(wifiBatteryStats.getScanTimeMillis()).thenReturn(scanTimeMs);
        when(wifiBatteryStats.getSleepTimeMillis()).thenReturn(sleepTimeMs);

        when(mBatteryStats.getWifiBatteryStats()).thenReturn(wifiBatteryStats);
        WifiPowerStats wifiPowerStats = mWifiPowerMetrics.buildProto();
        verify(mBatteryStats).getWifiBatteryStats();
        assertEquals("monitoredRailEnergyConsumedMah must match with field from WifiPowerStats",
                monitoredRailEnergyConsumedMah, wifiPowerStats.monitoredRailEnergyConsumedMah,
                0.01);
        assertEquals("numBytesTx must match with field from WifiBatteryStats",
                numBytesTx, wifiPowerStats.numBytesTx);
        assertEquals("numBytesRx must match with field from WifiBatteryStats",
                numBytesRx, wifiPowerStats.numBytesRx);
        assertEquals("numPacketsTx must match with field from WifiBatteryStats",
                numPacketsTx, wifiPowerStats.numPacketsTx);
        assertEquals("numPacketsRx must match with field from WifiBatteryStats",
                numPacketsRx, wifiPowerStats.numPacketsRx);
        assertEquals("txTimeMs must match with field from WifiBatteryStats",
                txTimeMs, wifiPowerStats.txTimeMs);
        assertEquals("rxTimeMs must match with field from WifiBatteryStats",
                rxTimeMs, wifiPowerStats.rxTimeMs);
        assertEquals("idleTimeMs must match with field from WifiBatteryStats",
                idleTimeMs, wifiPowerStats.idleTimeMs);
        assertEquals("scanTimeMs must match with field from WifiBatteryStats",
                scanTimeMs, wifiPowerStats.scanTimeMs);
        assertEquals("sleepTimeMs must match with field from WifiBatteryStats",
                sleepTimeMs, wifiPowerStats.sleepTimeMs);
    }

    /**
     * Tests that WifiPowerStats has its fields set to the |DEFAULT_VALUE| when BatteryStatsManager
     * returns null
     * @throws Exception
     */
    @Test
    public void testBuildProtoNull() throws Exception {
        when(mBatteryStats.getWifiBatteryStats()).thenReturn(null);
        WifiPowerStats wifiPowerStats = mWifiPowerMetrics.buildProto();
        verify(mBatteryStats).getWifiBatteryStats();
        assertEquals("monitoredRailEnergyConsumedMah must be default value when getWifiBatteryStats"
                + " returns null", DEFAULT_VALUE, wifiPowerStats.monitoredRailEnergyConsumedMah,
                0.01);
    }
}
