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

import static com.android.server.wifi.util.InformationElementUtil.BssLoad.INVALID;
import static com.android.server.wifi.util.InformationElementUtil.BssLoad.MAX_CHANNEL_UTILIZATION;
import static com.android.server.wifi.util.InformationElementUtil.BssLoad.MIN_CHANNEL_UTILIZATION;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.validateMockitoUsage;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.net.wifi.ScanResult;
import android.net.wifi.nl80211.DeviceWiphyCapabilities;

import androidx.test.filters.SmallTest;

import com.android.wifi.resources.R;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;

/**
 * Unit tests for {@link com.android.server.wifi.ThroughputPredictor}.
 */
@SmallTest
public class ThroughputPredictorTest extends WifiBaseTest {
    @Mock private DeviceWiphyCapabilities mDeviceCapabilities;
    @Mock private Context mContext;
    // For simulating the resources, we use a Spy on a MockResource
    // (which is really more of a stub than a mock, in spite if its name).
    // This is so that we get errors on any calls that we have not explicitly set up.
    @Spy
    private MockResources mResource = new MockResources();
    ThroughputPredictor mThroughputPredictor;
    WifiNative.ConnectionCapabilities mConnectionCap = new WifiNative.ConnectionCapabilities();

    /**
     * Sets up for unit test
     */
    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        when(mDeviceCapabilities.isWifiStandardSupported(ScanResult.WIFI_STANDARD_11N))
                .thenReturn(true);
        when(mDeviceCapabilities.isWifiStandardSupported(ScanResult.WIFI_STANDARD_11AC))
                .thenReturn(true);
        when(mDeviceCapabilities.isWifiStandardSupported(ScanResult.WIFI_STANDARD_11AX))
                .thenReturn(false);
        when(mDeviceCapabilities.isChannelWidthSupported(ScanResult.CHANNEL_WIDTH_40MHZ))
                .thenReturn(true);
        when(mDeviceCapabilities.isChannelWidthSupported(ScanResult.CHANNEL_WIDTH_80MHZ))
                .thenReturn(true);
        when(mDeviceCapabilities.isChannelWidthSupported(ScanResult.CHANNEL_WIDTH_160MHZ))
                .thenReturn(false);
        when(mDeviceCapabilities.getMaxNumberTxSpatialStreams()).thenReturn(2);
        when(mDeviceCapabilities.getMaxNumberRxSpatialStreams()).thenReturn(2);

        when(mResource.getBoolean(
                R.bool.config_wifiFrameworkMaxNumSpatialStreamDeviceOverrideEnable))
                .thenReturn(false);
        when(mResource.getInteger(
                R.integer.config_wifiFrameworkMaxNumSpatialStreamDeviceOverrideValue))
                .thenReturn(2);
        when(mContext.getResources()).thenReturn(mResource);
        mThroughputPredictor = new ThroughputPredictor(mContext);
    }

    /** Cleans up test. */
    @After
    public void cleanup() {
        validateMockitoUsage();
    }

    @Test
    public void verifyVeryLowRssi() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11AC, ScanResult.CHANNEL_WIDTH_20MHZ, -200, 2412, 1,
                0, 0, false);

        assertEquals(0, predictedThroughputMbps);
    }

    @Test
    public void verifyMaxChannelUtilizationBssLoad() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11AC, ScanResult.CHANNEL_WIDTH_20MHZ, 0, 2412, 1,
                MAX_CHANNEL_UTILIZATION, 0, false);

        assertEquals(0, predictedThroughputMbps);
    }

    @Test
    public void verifyMaxChannelUtilizationLinkLayerStats() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11AC, ScanResult.CHANNEL_WIDTH_20MHZ, 0, 5210, 1,
                INVALID, MAX_CHANNEL_UTILIZATION, false);

        assertEquals(0, predictedThroughputMbps);
    }

    @Test
    public void verifyHighRssiMinChannelUtilizationAc5g80Mhz2ss() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11AC, ScanResult.CHANNEL_WIDTH_80MHZ, 0, 5180, 2,
                MIN_CHANNEL_UTILIZATION, 50, false);

        assertEquals(866, predictedThroughputMbps);
    }

    @Test
    public void verifyHighRssiMinChannelUtilizationAc5g80Mhz2ssOverriddenTo1ss() {
        when(mResource.getBoolean(
                R.bool.config_wifiFrameworkMaxNumSpatialStreamDeviceOverrideEnable))
                .thenReturn(true);
        when(mResource.getInteger(
                R.integer.config_wifiFrameworkMaxNumSpatialStreamDeviceOverrideValue))
                .thenReturn(1);
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11AC, ScanResult.CHANNEL_WIDTH_80MHZ, 0, 5180, 2,
                MIN_CHANNEL_UTILIZATION, 50, false);

        assertEquals(433, predictedThroughputMbps);
    }

    @Test
    public void verifyHighRssiMinChannelUtilizationAx5g160Mhz4ss() {
        when(mDeviceCapabilities.isWifiStandardSupported(ScanResult.WIFI_STANDARD_11AX))
                .thenReturn(true);
        when(mDeviceCapabilities.isChannelWidthSupported(ScanResult.CHANNEL_WIDTH_160MHZ))
                .thenReturn(true);
        when(mDeviceCapabilities.getMaxNumberTxSpatialStreams()).thenReturn(4);
        when(mDeviceCapabilities.getMaxNumberRxSpatialStreams()).thenReturn(4);

        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11AX, ScanResult.CHANNEL_WIDTH_160MHZ, 0, 5180, 4,
                MIN_CHANNEL_UTILIZATION, INVALID, false);

        assertEquals(4803, predictedThroughputMbps);
    }

    @Test
    public void verifyHighRssiMinChannelUtilizationAx5g160Mhz4ssOverriddenTo2ss() {
        when(mResource.getBoolean(
                R.bool.config_wifiFrameworkMaxNumSpatialStreamDeviceOverrideEnable))
                .thenReturn(true);
        when(mDeviceCapabilities.isWifiStandardSupported(ScanResult.WIFI_STANDARD_11AX))
                .thenReturn(true);
        when(mDeviceCapabilities.isChannelWidthSupported(ScanResult.CHANNEL_WIDTH_160MHZ))
                .thenReturn(true);
        when(mDeviceCapabilities.getMaxNumberTxSpatialStreams()).thenReturn(4);
        when(mDeviceCapabilities.getMaxNumberRxSpatialStreams()).thenReturn(4);

        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11AX, ScanResult.CHANNEL_WIDTH_160MHZ, 0, 5180, 4,
                MIN_CHANNEL_UTILIZATION, INVALID, false);

        assertEquals(2401, predictedThroughputMbps);
    }


    @Test
    public void verifyMidRssiMinChannelUtilizationAc5g80Mhz2ss() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11AC, ScanResult.CHANNEL_WIDTH_80MHZ, -50, 5180, 2,
                MIN_CHANNEL_UTILIZATION, INVALID, false);

        assertEquals(866, predictedThroughputMbps);
    }

    @Test
    public void verifyLowRssiMinChannelUtilizationAc5g80Mhz2ss() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11AC, ScanResult.CHANNEL_WIDTH_80MHZ, -80, 5180, 2,
                MIN_CHANNEL_UTILIZATION, INVALID, false);

        assertEquals(41, predictedThroughputMbps);
    }

    @Test
    public void verifyLowRssiDefaultChannelUtilizationAc5g80Mhz2ss() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11AC, ScanResult.CHANNEL_WIDTH_80MHZ, -80, 5180, 2,
                INVALID, INVALID, false);

        assertEquals(31, predictedThroughputMbps);
    }

    @Test
    public void verifyHighRssiMinChannelUtilizationAc2g20Mhz2ss() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11AC, ScanResult.CHANNEL_WIDTH_20MHZ, -20, 2437, 2,
                MIN_CHANNEL_UTILIZATION, INVALID, false);

        assertEquals(192, predictedThroughputMbps);
    }

    @Test
    public void verifyHighRssiMinChannelUtilizationAc2g20Mhz2ssBluetoothConnected() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11AC, ScanResult.CHANNEL_WIDTH_20MHZ, -20, 2437, 2,
                MIN_CHANNEL_UTILIZATION, INVALID, true);

        assertEquals(144, predictedThroughputMbps);
    }

    @Test
    public void verifyHighRssiMinChannelUtilizationLegacy5g20Mhz() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_LEGACY, ScanResult.CHANNEL_WIDTH_20MHZ, -50, 5180,
                1, MIN_CHANNEL_UTILIZATION, INVALID, false);

        assertEquals(54, predictedThroughputMbps);
    }

    @Test
    public void verifyLowRssiDefaultChannelUtilizationLegacy5g20Mhz() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_LEGACY, ScanResult.CHANNEL_WIDTH_20MHZ, -80, 5180,
                2, INVALID, INVALID, false);

        assertEquals(11, predictedThroughputMbps);
    }

    @Test
    public void verifyHighRssiMinChannelUtilizationHt2g20Mhz2ss() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11N, ScanResult.CHANNEL_WIDTH_20MHZ, -50, 2437, 2,
                MIN_CHANNEL_UTILIZATION, INVALID, false);

        assertEquals(144, predictedThroughputMbps);
    }

    @Test
    public void verifyHighRssiMinChannelUtilizationHt2g20MhzIncorrectNss() {
        when(mDeviceCapabilities.getMaxNumberTxSpatialStreams()).thenReturn(0);
        when(mDeviceCapabilities.getMaxNumberRxSpatialStreams()).thenReturn(0);
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11N, ScanResult.CHANNEL_WIDTH_20MHZ, -50, 2437, 2,
                MIN_CHANNEL_UTILIZATION, INVALID, false);
        // Expect to 1SS peak rate because maxNumberSpatialStream is overridden to 1.
        assertEquals(72, predictedThroughputMbps);
    }

    @Test
    public void verifyLowRssiDefaultChannelUtilizationHt2g20Mhz1ss() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11N, ScanResult.CHANNEL_WIDTH_20MHZ, -80, 2437, 1,
                INVALID, INVALID, true);

        assertEquals(5, predictedThroughputMbps);
    }

    @Test
    public void verifyHighRssiHighChannelUtilizationAc2g20Mhz2ss() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11AC, ScanResult.CHANNEL_WIDTH_20MHZ, -50, 2437, 2,
                INVALID, 80, true);

        assertEquals(84, predictedThroughputMbps);
    }

    @Test
    public void verifyRssiBoundaryHighChannelUtilizationAc2g20Mhz2ss() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11AC, ScanResult.CHANNEL_WIDTH_20MHZ, -69, 2437, 2,
                INVALID, 80, true);

        assertEquals(46, predictedThroughputMbps);
    }

    @Test
    public void verifyRssiBoundaryHighChannelUtilizationAc5g40Mhz2ss() {
        int predictedThroughputMbps = mThroughputPredictor.predictThroughput(mDeviceCapabilities,
                ScanResult.WIFI_STANDARD_11AC, ScanResult.CHANNEL_WIDTH_40MHZ, -66, 5180, 2,
                INVALID, 80, false);

        assertEquals(103, predictedThroughputMbps);
    }

    @Test
    public void verifyMaxThroughputAc40Mhz2ss() {
        mConnectionCap.wifiStandard = ScanResult.WIFI_STANDARD_11AC;
        mConnectionCap.channelBandwidth = ScanResult.CHANNEL_WIDTH_40MHZ;
        mConnectionCap.maxNumberTxSpatialStreams = 2;
        assertEquals(400, mThroughputPredictor.predictMaxTxThroughput(mConnectionCap));
    }

    @Test
    public void verifyMaxThroughputAc80Mhz2ss() {
        mConnectionCap.wifiStandard = ScanResult.WIFI_STANDARD_11AC;
        mConnectionCap.channelBandwidth = ScanResult.CHANNEL_WIDTH_80MHZ;
        mConnectionCap.maxNumberRxSpatialStreams = 2;
        assertEquals(866, mThroughputPredictor.predictMaxRxThroughput(mConnectionCap));
    }

    @Test
    public void verifyMaxThroughputAc80MhzIncorrectNss() {
        mConnectionCap.wifiStandard = ScanResult.WIFI_STANDARD_11AC;
        mConnectionCap.channelBandwidth = ScanResult.CHANNEL_WIDTH_80MHZ;
        mConnectionCap.maxNumberRxSpatialStreams = -5;
        // Expect to 1SS peak rate because maxNumberSpatialStream is overridden to 1.
        assertEquals(433, mThroughputPredictor.predictMaxRxThroughput(mConnectionCap));
    }

    @Test
    public void verifyMaxThroughputN20Mhz1ss() {
        mConnectionCap.wifiStandard = ScanResult.WIFI_STANDARD_11N;
        mConnectionCap.channelBandwidth = ScanResult.CHANNEL_WIDTH_20MHZ;
        mConnectionCap.maxNumberRxSpatialStreams = 1;
        assertEquals(72, mThroughputPredictor.predictMaxRxThroughput(mConnectionCap));
    }

    @Test
    public void verifyMaxThroughputLegacy20Mhz1ss() {
        mConnectionCap.wifiStandard = ScanResult.WIFI_STANDARD_LEGACY;
        mConnectionCap.channelBandwidth = ScanResult.CHANNEL_WIDTH_80MHZ;
        mConnectionCap.maxNumberRxSpatialStreams = 1;
        assertEquals(54, mThroughputPredictor.predictMaxRxThroughput(mConnectionCap));
    }

    @Test
    public void verifyMaxThroughputAx80Mhz2ss() {
        mConnectionCap.wifiStandard = ScanResult.WIFI_STANDARD_11AX;
        mConnectionCap.channelBandwidth = ScanResult.CHANNEL_WIDTH_80MHZ;
        mConnectionCap.maxNumberRxSpatialStreams = 2;
        assertEquals(1200, mThroughputPredictor.predictMaxRxThroughput(mConnectionCap));
    }

    @Test
    public void verifyMaxThroughputUnknownStandard() {
        mConnectionCap.wifiStandard = ScanResult.WIFI_STANDARD_UNKNOWN;
        mConnectionCap.channelBandwidth = ScanResult.CHANNEL_WIDTH_80MHZ;
        mConnectionCap.maxNumberTxSpatialStreams = 2;
        assertEquals(-1, mThroughputPredictor.predictMaxTxThroughput(mConnectionCap));
    }

    @Test
    public void verifyTxThroughputAc20Mhz2ssMiddleSnr() {
        mConnectionCap.wifiStandard = ScanResult.WIFI_STANDARD_11AC;
        mConnectionCap.channelBandwidth = ScanResult.CHANNEL_WIDTH_20MHZ;
        mConnectionCap.maxNumberTxSpatialStreams = 2;
        assertEquals(131, mThroughputPredictor.predictTxThroughput(mConnectionCap,
                -50, 2437, 80));
    }

    @Test
    public void verifyRxThroughputAx160Mhz4ssHighSnrInvalidUtilization() {
        mConnectionCap.wifiStandard = ScanResult.WIFI_STANDARD_11AX;
        mConnectionCap.channelBandwidth = ScanResult.CHANNEL_WIDTH_160MHZ;
        mConnectionCap.maxNumberRxSpatialStreams = 4;
        //mConnectionCap.maxNumberTxSpatialStreams = 4;
        assertEquals(2881, mThroughputPredictor.predictRxThroughput(mConnectionCap,
                -10, 5180, INVALID));
    }
}
