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

package android.telephony.cts;

import static androidx.test.InstrumentationRegistry.getContext;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Matchers.anyInt;

import android.os.BatteryStatsManager;
import android.os.connectivity.CellularBatteryStats;

import org.junit.Test;

/**
 * Test BatteryStatsManager and CellularBatteryStats to ensure that valid data is being reported
 * and that invalid data is not reported.
 */
public class BatteryStatsManagerTest{

    /** Test that {@link CellularBatteryStats} getters return sane values. */
    @Test
    public void testGetCellularBatteryStats() {
        BatteryStatsManager bsm = getContext().getSystemService(BatteryStatsManager.class);
        CellularBatteryStats cellularBatteryStats = bsm.getCellularBatteryStats();

        assertThat(cellularBatteryStats.getEnergyConsumedMaMillis()).isAtLeast(0L);
        assertThat(cellularBatteryStats.getIdleTimeMillis()).isAtLeast(0L);
        assertThat(cellularBatteryStats.getLoggingDurationMillis()).isAtLeast(0L);
        assertThat(cellularBatteryStats.getKernelActiveTimeMillis()).isAtLeast(0L);
        assertThat(cellularBatteryStats.getMonitoredRailChargeConsumedMaMillis()).isAtLeast(0L);
        assertThat(cellularBatteryStats.getNumBytesRx()).isAtLeast(0L);
        assertThat(cellularBatteryStats.getNumBytesTx()).isAtLeast(0L);
        assertThat(cellularBatteryStats.getNumPacketsRx()).isAtLeast(0L);
        assertThat(cellularBatteryStats.getNumPacketsTx()).isAtLeast(0L);
        assertThat(cellularBatteryStats.getRxTimeMillis()).isAtLeast(0L);
        assertThat(cellularBatteryStats.getSleepTimeMillis()).isAtLeast(0L);
        assertThat(cellularBatteryStats.getTimeInRatMicros(anyInt())).isAtLeast(-1L);
        assertThat(cellularBatteryStats.getTimeInRxSignalStrengthLevelMicros(
                anyInt())).isAtLeast(-1L);
    }
}

