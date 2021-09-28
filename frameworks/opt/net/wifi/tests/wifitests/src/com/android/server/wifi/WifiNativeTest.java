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
 * limitations under the License
 */

package com.android.server.wifi;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.net.MacAddress;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiScanner;
import android.net.wifi.nl80211.NativeScanResult;
import android.net.wifi.nl80211.RadioChainInfo;
import android.net.wifi.nl80211.WifiNl80211Manager;
import android.net.wifi.nl80211.WifiNl80211Manager.SendMgmtFrameCallback;
import android.os.Handler;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.util.NativeUtil;
import com.android.server.wifi.util.NetdWrapper;

import org.junit.Before;
import org.junit.Test;
import org.mockito.AdditionalMatchers;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Random;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * Unit tests for {@link com.android.server.wifi.WifiNative}.
 */
@SmallTest
public class WifiNativeTest extends WifiBaseTest {
    private static final String WIFI_IFACE_NAME = "mockWlan";
    private static final long FATE_REPORT_DRIVER_TIMESTAMP_USEC = 12345;
    private static final byte[] FATE_REPORT_FRAME_BYTES = new byte[] {
            'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 0, 1, 2, 3, 4, 5, 6, 7};
    private static final WifiNative.TxFateReport TX_FATE_REPORT = new WifiNative.TxFateReport(
            WifiLoggerHal.TX_PKT_FATE_SENT,
            FATE_REPORT_DRIVER_TIMESTAMP_USEC,
            WifiLoggerHal.FRAME_TYPE_ETHERNET_II,
            FATE_REPORT_FRAME_BYTES
    );
    private static final WifiNative.RxFateReport RX_FATE_REPORT = new WifiNative.RxFateReport(
            WifiLoggerHal.RX_PKT_FATE_FW_DROP_INVALID,
            FATE_REPORT_DRIVER_TIMESTAMP_USEC,
            WifiLoggerHal.FRAME_TYPE_ETHERNET_II,
            FATE_REPORT_FRAME_BYTES
    );
    private static final FrameTypeMapping[] FRAME_TYPE_MAPPINGS = new FrameTypeMapping[] {
            new FrameTypeMapping(WifiLoggerHal.FRAME_TYPE_UNKNOWN, "unknown", "N/A"),
            new FrameTypeMapping(WifiLoggerHal.FRAME_TYPE_ETHERNET_II, "data", "Ethernet"),
            new FrameTypeMapping(WifiLoggerHal.FRAME_TYPE_80211_MGMT, "802.11 management",
                    "802.11 Mgmt"),
            new FrameTypeMapping((byte) 42, "42", "N/A")
    };
    private static final FateMapping[] TX_FATE_MAPPINGS = new FateMapping[] {
            new FateMapping(WifiLoggerHal.TX_PKT_FATE_ACKED, "acked"),
            new FateMapping(WifiLoggerHal.TX_PKT_FATE_SENT, "sent"),
            new FateMapping(WifiLoggerHal.TX_PKT_FATE_FW_QUEUED, "firmware queued"),
            new FateMapping(WifiLoggerHal.TX_PKT_FATE_FW_DROP_INVALID,
                    "firmware dropped (invalid frame)"),
            new FateMapping(
                    WifiLoggerHal.TX_PKT_FATE_FW_DROP_NOBUFS,  "firmware dropped (no bufs)"),
            new FateMapping(
                    WifiLoggerHal.TX_PKT_FATE_FW_DROP_OTHER, "firmware dropped (other)"),
            new FateMapping(WifiLoggerHal.TX_PKT_FATE_DRV_QUEUED, "driver queued"),
            new FateMapping(WifiLoggerHal.TX_PKT_FATE_DRV_DROP_INVALID,
                    "driver dropped (invalid frame)"),
            new FateMapping(WifiLoggerHal.TX_PKT_FATE_DRV_DROP_NOBUFS,
                    "driver dropped (no bufs)"),
            new FateMapping(WifiLoggerHal.TX_PKT_FATE_DRV_DROP_OTHER, "driver dropped (other)"),
            new FateMapping((byte) 42, "42")
    };
    private static final FateMapping[] RX_FATE_MAPPINGS = new FateMapping[] {
            new FateMapping(WifiLoggerHal.RX_PKT_FATE_SUCCESS, "success"),
            new FateMapping(WifiLoggerHal.RX_PKT_FATE_FW_QUEUED, "firmware queued"),
            new FateMapping(
                    WifiLoggerHal.RX_PKT_FATE_FW_DROP_FILTER, "firmware dropped (filter)"),
            new FateMapping(WifiLoggerHal.RX_PKT_FATE_FW_DROP_INVALID,
                    "firmware dropped (invalid frame)"),
            new FateMapping(
                    WifiLoggerHal.RX_PKT_FATE_FW_DROP_NOBUFS, "firmware dropped (no bufs)"),
            new FateMapping(
                    WifiLoggerHal.RX_PKT_FATE_FW_DROP_OTHER, "firmware dropped (other)"),
            new FateMapping(WifiLoggerHal.RX_PKT_FATE_DRV_QUEUED, "driver queued"),
            new FateMapping(
                    WifiLoggerHal.RX_PKT_FATE_DRV_DROP_FILTER, "driver dropped (filter)"),
            new FateMapping(WifiLoggerHal.RX_PKT_FATE_DRV_DROP_INVALID,
                    "driver dropped (invalid frame)"),
            new FateMapping(
                    WifiLoggerHal.RX_PKT_FATE_DRV_DROP_NOBUFS, "driver dropped (no bufs)"),
            new FateMapping(WifiLoggerHal.RX_PKT_FATE_DRV_DROP_OTHER, "driver dropped (other)"),
            new FateMapping((byte) 42, "42")
    };
    private static final WifiNl80211Manager.SignalPollResult SIGNAL_POLL_RESULT =
            new WifiNl80211Manager.SignalPollResult(-60, 12, 6, 5240);

    private static final Set<Integer> SCAN_FREQ_SET =
            new HashSet<Integer>() {{
                add(2410);
                add(2450);
                add(5050);
                add(5200);
            }};
    private static final String TEST_QUOTED_SSID_1 = "\"testSsid1\"";
    private static final String TEST_QUOTED_SSID_2 = "\"testSsid2\"";
    private static final int[] TEST_FREQUENCIES_1 = {};
    private static final int[] TEST_FREQUENCIES_2 = {2500, 5124};
    private static final List<String> SCAN_HIDDEN_NETWORK_SSID_SET =
            new ArrayList<String>() {{
                add(TEST_QUOTED_SSID_1);
                add(TEST_QUOTED_SSID_2);
            }};
    private static final List<byte[]> SCAN_HIDDEN_NETWORK_BYTE_SSID_SET =
            new ArrayList<byte[]>() {{
                add(NativeUtil.byteArrayFromArrayList(NativeUtil.decodeSsid(TEST_QUOTED_SSID_1)));
                add(NativeUtil.byteArrayFromArrayList(NativeUtil.decodeSsid(TEST_QUOTED_SSID_2)));
            }};

    private static final WifiNative.PnoSettings TEST_PNO_SETTINGS =
            new WifiNative.PnoSettings() {{
                isConnected = false;
                periodInMs = 6000;
                networkList = new WifiNative.PnoNetwork[2];
                networkList[0] = new WifiNative.PnoNetwork();
                networkList[1] = new WifiNative.PnoNetwork();
                networkList[0].ssid = TEST_QUOTED_SSID_1;
                networkList[1].ssid = TEST_QUOTED_SSID_2;
                networkList[0].frequencies = TEST_FREQUENCIES_1;
                networkList[1].frequencies = TEST_FREQUENCIES_2;
            }};
    private static final MacAddress TEST_MAC_ADDRESS = MacAddress.fromString("ee:33:a2:94:10:92");

    private static final String TEST_MAC_ADDRESS_STR = "f4:f5:e8:51:9e:09";
    private static final String TEST_BSSID_STR = "a8:bd:27:5b:33:72";
    private static final int TEST_MCS_RATE = 5;
    private static final int TEST_SEQUENCE_NUM = 0x66b0;

    private static final byte[] TEST_SSID =
            new byte[] {'G', 'o', 'o', 'g', 'l', 'e', 'G', 'u', 'e', 's', 't'};
    private static final byte[] TEST_BSSID =
            new byte[] {(byte) 0x12, (byte) 0xef, (byte) 0xa1,
                    (byte) 0x2c, (byte) 0x97, (byte) 0x8b};
    // This the IE buffer which is consistent with TEST_SSID.
    private static final byte[] TEST_INFO_ELEMENT_SSID =
            new byte[] {
                    // Element ID for SSID.
                    (byte) 0x00,
                    // Length of the SSID: 0x0b or 11.
                    (byte) 0x0b,
                    // This is string "GoogleGuest"
                    'G', 'o', 'o', 'g', 'l', 'e', 'G', 'u', 'e', 's', 't'};
    // RSN IE data indicating EAP key management.
    private static final byte[] TEST_INFO_ELEMENT_RSN =
            new byte[] {
                    // Element ID for RSN.
                    (byte) 0x30,
                    // Length of the element data.
                    (byte) 0x18,
                    (byte) 0x01, (byte) 0x00, (byte) 0x00, (byte) 0x0F, (byte) 0xAC, (byte) 0x02,
                    (byte) 0x02, (byte) 0x00, (byte) 0x00, (byte) 0x0F, (byte) 0xAC, (byte) 0x04,
                    (byte) 0x00, (byte) 0x0F, (byte) 0xAC, (byte) 0x02, (byte) 0x01, (byte) 0x00,
                    (byte) 0x00, (byte) 0x0F, (byte) 0xAC, (byte) 0x01, (byte) 0x00, (byte) 0x00 };

    private static final int TEST_FREQUENCY = 2456;
    private static final int TEST_SIGNAL_MBM = -4500;
    private static final long TEST_TSF = 34455441;
    private static final int TEST_CAPABILITY = 0b0000_0000_0010_0100;
    private static final boolean TEST_ASSOCIATED = true;
    private static final NativeScanResult MOCK_NATIVE_SCAN_RESULT = createMockNativeScanResult();
    private static NativeScanResult createMockNativeScanResult() {
        NativeScanResult result = new NativeScanResult();
        result.ssid = TEST_SSID;
        result.bssid = TEST_BSSID;
        result.infoElement = TEST_INFO_ELEMENT_SSID;
        result.frequency = TEST_FREQUENCY;
        result.signalMbm = TEST_SIGNAL_MBM;
        result.tsf = TEST_TSF;
        result.capability = TEST_CAPABILITY;
        result.associated = TEST_ASSOCIATED;
        result.radioChainInfos = new ArrayList<>();
        return result;
    }

    private static final RadioChainInfo MOCK_NATIVE_RADIO_CHAIN_INFO_1 = new RadioChainInfo(1, -89);
    private static final RadioChainInfo MOCK_NATIVE_RADIO_CHAIN_INFO_2 = new RadioChainInfo(0, -78);

    @Mock private WifiVendorHal mWifiVendorHal;
    @Mock private WifiNl80211Manager mWificondControl;
    @Mock private SupplicantStaIfaceHal mStaIfaceHal;
    @Mock private HostapdHal mHostapdHal;
    @Mock private WifiMonitor mWifiMonitor;
    @Mock private PropertyService mPropertyService;
    @Mock private WifiMetrics mWifiMetrics;
    @Mock private Handler mHandler;
    @Mock private SendMgmtFrameCallback mSendMgmtFrameCallback;
    @Mock private Random mRandom;
    @Mock private WifiInjector mWifiInjector;
    @Mock private NetdWrapper mNetdWrapper;

    ArgumentCaptor<WifiNl80211Manager.ScanEventCallback> mScanCallbackCaptor =
            ArgumentCaptor.forClass(WifiNl80211Manager.ScanEventCallback.class);

    private WifiNative mWifiNative;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        when(mWifiVendorHal.initialize(any())).thenReturn(true);
        when(mWifiVendorHal.isVendorHalSupported()).thenReturn(true);
        when(mWifiVendorHal.startVendorHal()).thenReturn(true);
        when(mWifiVendorHal.startVendorHalSta()).thenReturn(true);
        when(mWifiVendorHal.startVendorHalAp()).thenReturn(true);
        when(mWifiVendorHal.createStaIface(any())).thenReturn(WIFI_IFACE_NAME);

        when(mWificondControl.setupInterfaceForClientMode(any(), any(), any(), any())).thenReturn(
                true);

        when(mStaIfaceHal.registerDeathHandler(any())).thenReturn(true);
        when(mStaIfaceHal.isInitializationComplete()).thenReturn(true);
        when(mStaIfaceHal.initialize()).thenReturn(true);
        when(mStaIfaceHal.startDaemon()).thenReturn(true);
        when(mStaIfaceHal.setupIface(any())).thenReturn(true);

        when(mWifiInjector.makeNetdWrapper()).thenReturn(mNetdWrapper);

        mWifiNative = new WifiNative(
                mWifiVendorHal, mStaIfaceHal, mHostapdHal, mWificondControl,
                mWifiMonitor, mPropertyService, mWifiMetrics,
                mHandler, mRandom, mWifiInjector);
        mWifiNative.initialize();
    }

    /**
     * Verifies that TxFateReport's constructor sets all of the TxFateReport fields.
     */
    @Test
    public void testTxFateReportCtorSetsFields() {
        WifiNative.TxFateReport fateReport = new WifiNative.TxFateReport(
                WifiLoggerHal.TX_PKT_FATE_SENT,  // non-zero value
                FATE_REPORT_DRIVER_TIMESTAMP_USEC,
                WifiLoggerHal.FRAME_TYPE_ETHERNET_II,  // non-zero value
                FATE_REPORT_FRAME_BYTES
        );
        assertEquals(WifiLoggerHal.TX_PKT_FATE_SENT, fateReport.mFate);
        assertEquals(FATE_REPORT_DRIVER_TIMESTAMP_USEC, fateReport.mDriverTimestampUSec);
        assertEquals(WifiLoggerHal.FRAME_TYPE_ETHERNET_II, fateReport.mFrameType);
        assertArrayEquals(FATE_REPORT_FRAME_BYTES, fateReport.mFrameBytes);
    }

    /**
     * Verifies that RxFateReport's constructor sets all of the RxFateReport fields.
     */
    @Test
    public void testRxFateReportCtorSetsFields() {
        WifiNative.RxFateReport fateReport = new WifiNative.RxFateReport(
                WifiLoggerHal.RX_PKT_FATE_FW_DROP_INVALID,  // non-zero value
                FATE_REPORT_DRIVER_TIMESTAMP_USEC,
                WifiLoggerHal.FRAME_TYPE_ETHERNET_II,  // non-zero value
                FATE_REPORT_FRAME_BYTES
        );
        assertEquals(WifiLoggerHal.RX_PKT_FATE_FW_DROP_INVALID, fateReport.mFate);
        assertEquals(FATE_REPORT_DRIVER_TIMESTAMP_USEC, fateReport.mDriverTimestampUSec);
        assertEquals(WifiLoggerHal.FRAME_TYPE_ETHERNET_II, fateReport.mFrameType);
        assertArrayEquals(FATE_REPORT_FRAME_BYTES, fateReport.mFrameBytes);
    }

    /**
     * Verifies the hashCode methods for HiddenNetwork and PnoNetwork classes
     */
    @Test
    public void testHashCode() {
        WifiNative.HiddenNetwork hiddenNet1 = new WifiNative.HiddenNetwork();
        hiddenNet1.ssid = new String("sametext");

        WifiNative.HiddenNetwork hiddenNet2 = new WifiNative.HiddenNetwork();
        hiddenNet2.ssid = new String("sametext");

        assertTrue(hiddenNet1.equals(hiddenNet2));
        assertEquals(hiddenNet1.hashCode(), hiddenNet2.hashCode());

        WifiNative.PnoNetwork pnoNet1 = new WifiNative.PnoNetwork();
        pnoNet1.ssid = new String("sametext");
        pnoNet1.flags = 2;
        pnoNet1.auth_bit_field = 4;
        pnoNet1.frequencies = TEST_FREQUENCIES_2;

        WifiNative.PnoNetwork pnoNet2 = new WifiNative.PnoNetwork();
        pnoNet2.ssid = new String("sametext");
        pnoNet2.flags = 2;
        pnoNet2.auth_bit_field = 4;
        pnoNet2.frequencies = TEST_FREQUENCIES_2;

        assertTrue(pnoNet1.equals(pnoNet2));
        assertEquals(pnoNet1.hashCode(), pnoNet2.hashCode());
    }

    // Support classes for test{Tx,Rx}FateReportToString.
    private static class FrameTypeMapping {
        byte mTypeNumber;
        String mExpectedTypeText;
        String mExpectedProtocolText;
        FrameTypeMapping(byte typeNumber, String expectedTypeText, String expectedProtocolText) {
            this.mTypeNumber = typeNumber;
            this.mExpectedTypeText = expectedTypeText;
            this.mExpectedProtocolText = expectedProtocolText;
        }
    }
    private static class FateMapping {
        byte mFateNumber;
        String mExpectedText;
        FateMapping(byte fateNumber, String expectedText) {
            this.mFateNumber = fateNumber;
            this.mExpectedText = expectedText;
        }
    }

    /**
     * Verifies that FateReport.getTableHeader() prints the right header.
     */
    @Test
    public void testFateReportTableHeader() {
        final String header = WifiNative.FateReport.getTableHeader();
        assertEquals(
                "\nTime usec        Walltime      Direction  Fate                              "
                + "Protocol      Type                     Result\n"
                + "---------        --------      ---------  ----                              "
                + "--------      ----                     ------\n", header);
    }

    /**
     * Verifies that TxFateReport.toTableRowString() includes the information we care about.
     */
    @Test
    public void testTxFateReportToTableRowString() {
        WifiNative.TxFateReport fateReport = TX_FATE_REPORT;
        assertTrue(
                fateReport.toTableRowString().replaceAll("\\s+", " ").trim().matches(
                    FATE_REPORT_DRIVER_TIMESTAMP_USEC + " "  // timestamp
                            + "\\d{2}:\\d{2}:\\d{2}\\.\\d{3} "  // walltime
                            + "TX "  // direction
                            + "sent "  // fate
                            + "Ethernet "  // type
                            + "N/A "  // protocol
                            + "N/A"  // result
                )
        );

        for (FrameTypeMapping frameTypeMapping : FRAME_TYPE_MAPPINGS) {
            fateReport = new WifiNative.TxFateReport(
                    WifiLoggerHal.TX_PKT_FATE_SENT,
                    FATE_REPORT_DRIVER_TIMESTAMP_USEC,
                    frameTypeMapping.mTypeNumber,
                    FATE_REPORT_FRAME_BYTES
            );
            assertTrue(
                    fateReport.toTableRowString().replaceAll("\\s+", " ").trim().matches(
                            FATE_REPORT_DRIVER_TIMESTAMP_USEC + " "  // timestamp
                                    + "\\d{2}:\\d{2}:\\d{2}\\.\\d{3} "  // walltime
                                    + "TX "  // direction
                                    + "sent "  // fate
                                    + frameTypeMapping.mExpectedProtocolText + " "  // type
                                    + "N/A "  // protocol
                                    + "N/A"  // result
                    )
            );
        }

        for (FateMapping fateMapping : TX_FATE_MAPPINGS) {
            fateReport = new WifiNative.TxFateReport(
                    fateMapping.mFateNumber,
                    FATE_REPORT_DRIVER_TIMESTAMP_USEC,
                    WifiLoggerHal.FRAME_TYPE_80211_MGMT,
                    FATE_REPORT_FRAME_BYTES
            );
            assertTrue(
                    fateReport.toTableRowString().replaceAll("\\s+", " ").trim().matches(
                            FATE_REPORT_DRIVER_TIMESTAMP_USEC + " "  // timestamp
                                    + "\\d{2}:\\d{2}:\\d{2}\\.\\d{3} "  // walltime
                                    + "TX "  // direction
                                    + Pattern.quote(fateMapping.mExpectedText) + " "  // fate
                                    + "802.11 Mgmt "  // type
                                    + "N/A "  // protocol
                                    + "N/A"  // result
                    )
            );
        }
    }

    /**
     * Verifies that TxFateReport.toVerboseStringWithPiiAllowed() includes the information we care
     * about.
     */
    @Test
    public void testTxFateReportToVerboseStringWithPiiAllowed() {
        WifiNative.TxFateReport fateReport = TX_FATE_REPORT;

        String verboseFateString = fateReport.toVerboseStringWithPiiAllowed();
        assertTrue(verboseFateString.contains("Frame direction: TX"));
        assertTrue(verboseFateString.contains("Frame timestamp: 12345"));
        assertTrue(verboseFateString.contains("Frame fate: sent"));
        assertTrue(verboseFateString.contains("Frame type: data"));
        assertTrue(verboseFateString.contains("Frame protocol: Ethernet"));
        assertTrue(verboseFateString.contains("Frame protocol type: N/A"));
        assertTrue(verboseFateString.contains("Frame length: 16"));
        assertTrue(verboseFateString.contains(
                "61 62 63 64 65 66 67 68 00 01 02 03 04 05 06 07")); // hex dump
        // TODO(quiche): uncomment this, once b/27975149 is fixed.
        // assertTrue(verboseFateString.contains("abcdefgh........"));  // hex dump

        for (FrameTypeMapping frameTypeMapping : FRAME_TYPE_MAPPINGS) {
            fateReport = new WifiNative.TxFateReport(
                    WifiLoggerHal.TX_PKT_FATE_SENT,
                    FATE_REPORT_DRIVER_TIMESTAMP_USEC,
                    frameTypeMapping.mTypeNumber,
                    FATE_REPORT_FRAME_BYTES
            );
            verboseFateString = fateReport.toVerboseStringWithPiiAllowed();
            assertTrue(verboseFateString.contains("Frame type: "
                    + frameTypeMapping.mExpectedTypeText));
        }

        for (FateMapping fateMapping : TX_FATE_MAPPINGS) {
            fateReport = new WifiNative.TxFateReport(
                    fateMapping.mFateNumber,
                    FATE_REPORT_DRIVER_TIMESTAMP_USEC,
                    WifiLoggerHal.FRAME_TYPE_80211_MGMT,
                    FATE_REPORT_FRAME_BYTES
            );
            verboseFateString = fateReport.toVerboseStringWithPiiAllowed();
            assertTrue(verboseFateString.contains("Frame fate: " + fateMapping.mExpectedText));
        }
    }

    /**
     * Verifies that RxFateReport.toTableRowString() includes the information we care about.
     */
    @Test
    public void testRxFateReportToTableRowString() {
        WifiNative.RxFateReport fateReport = RX_FATE_REPORT;
        assertTrue(
                fateReport.toTableRowString().replaceAll("\\s+", " ").trim().matches(
                        FATE_REPORT_DRIVER_TIMESTAMP_USEC + " "  // timestamp
                                + "\\d{2}:\\d{2}:\\d{2}\\.\\d{3} "  // walltime
                                + "RX "  // direction
                                + Pattern.quote("firmware dropped (invalid frame) ")  // fate
                                + "Ethernet "  // type
                                + "N/A "  // protocol
                                + "N/A"  // result
                )
        );

        // FrameTypeMappings omitted, as they're the same as for TX.

        for (FateMapping fateMapping : RX_FATE_MAPPINGS) {
            fateReport = new WifiNative.RxFateReport(
                    fateMapping.mFateNumber,
                    FATE_REPORT_DRIVER_TIMESTAMP_USEC,
                    WifiLoggerHal.FRAME_TYPE_80211_MGMT,
                    FATE_REPORT_FRAME_BYTES
            );
            assertTrue(
                    fateReport.toTableRowString().replaceAll("\\s+", " ").trim().matches(
                            FATE_REPORT_DRIVER_TIMESTAMP_USEC + " "  // timestamp
                                    + "\\d{2}:\\d{2}:\\d{2}\\.\\d{3} "  // walltime
                                    + "RX "  // direction
                                    + Pattern.quote(fateMapping.mExpectedText) + " " // fate
                                    + "802.11 Mgmt "  // type
                                    + "N/A " // protocol
                                    + "N/A"  // result
                    )
            );
        }
    }

    /**
     * Verifies that RxFateReport.toVerboseStringWithPiiAllowed() includes the information we care
     * about.
     */
    @Test
    public void testRxFateReportToVerboseStringWithPiiAllowed() {
        WifiNative.RxFateReport fateReport = RX_FATE_REPORT;

        String verboseFateString = fateReport.toVerboseStringWithPiiAllowed();
        assertTrue(verboseFateString.contains("Frame direction: RX"));
        assertTrue(verboseFateString.contains("Frame timestamp: 12345"));
        assertTrue(verboseFateString.contains("Frame fate: firmware dropped (invalid frame)"));
        assertTrue(verboseFateString.contains("Frame type: data"));
        assertTrue(verboseFateString.contains("Frame protocol: Ethernet"));
        assertTrue(verboseFateString.contains("Frame protocol type: N/A"));
        assertTrue(verboseFateString.contains("Frame length: 16"));
        assertTrue(verboseFateString.contains(
                "61 62 63 64 65 66 67 68 00 01 02 03 04 05 06 07")); // hex dump
        // TODO(quiche): uncomment this, once b/27975149 is fixed.
        // assertTrue(verboseFateString.contains("abcdefgh........"));  // hex dump

        // FrameTypeMappings omitted, as they're the same as for TX.

        for (FateMapping fateMapping : RX_FATE_MAPPINGS) {
            fateReport = new WifiNative.RxFateReport(
                    fateMapping.mFateNumber,
                    FATE_REPORT_DRIVER_TIMESTAMP_USEC,
                    WifiLoggerHal.FRAME_TYPE_80211_MGMT,
                    FATE_REPORT_FRAME_BYTES
            );
            verboseFateString = fateReport.toVerboseStringWithPiiAllowed();
            assertTrue(verboseFateString.contains("Frame fate: " + fateMapping.mExpectedText));
        }
    }

    /**
     * Verifies that startPktFateMonitoring returns false when HAL is not started.
     */
    @Test
    public void testStartPktFateMonitoringReturnsFalseWhenHalIsNotStarted() {
        assertFalse(mWifiNative.isHalStarted());
        assertFalse(mWifiNative.startPktFateMonitoring(WIFI_IFACE_NAME));
    }

    /**
     * Verifies that getTxPktFates returns error when HAL is not started.
     */
    @Test
    public void testGetTxPktFatesReturnsErrorWhenHalIsNotStarted() {
        WifiNative.TxFateReport[] fateReports = null;
        assertFalse(mWifiNative.isHalStarted());
        assertFalse(mWifiNative.getTxPktFates(WIFI_IFACE_NAME, fateReports));
    }

    /**
     * Verifies that getRxPktFates returns error when HAL is not started.
     */
    @Test
    public void testGetRxPktFatesReturnsErrorWhenHalIsNotStarted() {
        WifiNative.RxFateReport[] fateReports = null;
        assertFalse(mWifiNative.isHalStarted());
        assertFalse(mWifiNative.getRxPktFates(WIFI_IFACE_NAME, fateReports));
    }

    // TODO(quiche): Add tests for the success cases (when HAL has been started). Specifically:
    // - testStartPktFateMonitoringCallsHalIfHalIsStarted()
    // - testGetTxPktFatesCallsHalIfHalIsStarted()
    // - testGetRxPktFatesCallsHalIfHalIsStarted()
    //
    // Adding these tests is difficult to do at the moment, because we can't mock out the HAL
    // itself. Also, we can't mock out the native methods, because those methods are private.
    // b/28005116.

    /** Verifies that getDriverStateDumpNative returns null when HAL is not started. */
    @Test
    public void testGetDriverStateDumpReturnsNullWhenHalIsNotStarted() {
        assertEquals(null, mWifiNative.getDriverStateDump());
    }

    // TODO(b/28005116): Add test for the success case of getDriverStateDump().

    /**
     * Verifies client mode + scan success.
     */
    @Test
    public void testClientModeScanSuccess() {
        mWifiNative.setupInterfaceForClientInConnectivityMode(null);
        verify(mWificondControl).setupInterfaceForClientMode(eq(WIFI_IFACE_NAME), any(),
                mScanCallbackCaptor.capture(), any());

        mScanCallbackCaptor.getValue().onScanResultReady();
        verify(mWifiMonitor).broadcastScanResultEvent(WIFI_IFACE_NAME);
    }

    /**
     * Verifies client mode + scan failure.
     */
    @Test
    public void testClientModeScanFailure() {
        mWifiNative.setupInterfaceForClientInConnectivityMode(null);
        verify(mWificondControl).setupInterfaceForClientMode(eq(WIFI_IFACE_NAME), any(),
                mScanCallbackCaptor.capture(), any());

        mScanCallbackCaptor.getValue().onScanFailed();
        verify(mWifiMonitor).broadcastScanFailedEvent(WIFI_IFACE_NAME);
    }

    /**
     * Verifies client mode + PNO scan success.
     */
    @Test
    public void testClientModePnoScanSuccess() {
        mWifiNative.setupInterfaceForClientInConnectivityMode(null);
        verify(mWificondControl).setupInterfaceForClientMode(eq(WIFI_IFACE_NAME), any(),
                any(), mScanCallbackCaptor.capture());

        mScanCallbackCaptor.getValue().onScanResultReady();
        verify(mWifiMonitor).broadcastPnoScanResultEvent(WIFI_IFACE_NAME);
        verify(mWifiMetrics).incrementPnoFoundNetworkEventCount();
    }

    /**
     * Verifies client mode + PNO scan failure.
     */
    @Test
    public void testClientModePnoScanFailure() {
        mWifiNative.setupInterfaceForClientInConnectivityMode(null);
        verify(mWificondControl).setupInterfaceForClientMode(eq(WIFI_IFACE_NAME), any(),
                any(), mScanCallbackCaptor.capture());

        mScanCallbackCaptor.getValue().onScanFailed();
        verify(mWifiMetrics).incrementPnoScanFailedCount();
    }

    /**
     * Verifies scan mode + scan success.
     */
    @Test
    public void testScanModeScanSuccess() {
        mWifiNative.setupInterfaceForClientInScanMode(null);
        verify(mWificondControl).setupInterfaceForClientMode(eq(WIFI_IFACE_NAME), any(),
                mScanCallbackCaptor.capture(), any());

        mScanCallbackCaptor.getValue().onScanResultReady();
        verify(mWifiMonitor).broadcastScanResultEvent(WIFI_IFACE_NAME);
    }

    /**
     * Verifies scan mode + scan failure.
     */
    @Test
    public void testScanModeScanFailure() {
        mWifiNative.setupInterfaceForClientInScanMode(null);
        verify(mWificondControl).setupInterfaceForClientMode(eq(WIFI_IFACE_NAME), any(),
                mScanCallbackCaptor.capture(), any());

        mScanCallbackCaptor.getValue().onScanFailed();
        verify(mWifiMonitor).broadcastScanFailedEvent(WIFI_IFACE_NAME);
    }

    /**
     * Verifies scan mode + PNO scan success.
     */
    @Test
    public void testScanModePnoScanSuccess() {
        mWifiNative.setupInterfaceForClientInScanMode(null);
        verify(mWificondControl).setupInterfaceForClientMode(eq(WIFI_IFACE_NAME), any(),
                any(), mScanCallbackCaptor.capture());

        mScanCallbackCaptor.getValue().onScanResultReady();
        verify(mWifiMonitor).broadcastPnoScanResultEvent(WIFI_IFACE_NAME);
        verify(mWifiMetrics).incrementPnoFoundNetworkEventCount();
    }

    /**
     * Verifies scan mode + PNO scan failure.
     */
    @Test
    public void testScanModePnoScanFailure() {
        mWifiNative.setupInterfaceForClientInScanMode(null);
        verify(mWificondControl).setupInterfaceForClientMode(eq(WIFI_IFACE_NAME), any(),
                any(), mScanCallbackCaptor.capture());

        mScanCallbackCaptor.getValue().onScanFailed();
        verify(mWifiMetrics).incrementPnoScanFailedCount();
    }


    /**
     * Verifies that signalPoll() calls underlying WificondControl.
     */
    @Test
    public void testSignalPoll() throws Exception {
        when(mWificondControl.signalPoll(WIFI_IFACE_NAME))
                .thenReturn(SIGNAL_POLL_RESULT);

        WifiNl80211Manager.SignalPollResult pollResult = mWifiNative.signalPoll(WIFI_IFACE_NAME);
        assertEquals(SIGNAL_POLL_RESULT.currentRssiDbm, pollResult.currentRssiDbm);
        assertEquals(SIGNAL_POLL_RESULT.txBitrateMbps, pollResult.txBitrateMbps);
        assertEquals(SIGNAL_POLL_RESULT.associationFrequencyMHz,
                pollResult.associationFrequencyMHz);
        assertEquals(SIGNAL_POLL_RESULT.rxBitrateMbps, pollResult.rxBitrateMbps);

        verify(mWificondControl).signalPoll(WIFI_IFACE_NAME);
    }

    /**
     * Verifies that scan() calls underlying WificondControl.
     */
    @Test
    public void testScan() throws Exception {
        mWifiNative.scan(WIFI_IFACE_NAME, WifiScanner.SCAN_TYPE_HIGH_ACCURACY, SCAN_FREQ_SET,
                SCAN_HIDDEN_NETWORK_SSID_SET);
        ArgumentCaptor<List<byte[]>> ssidSetCaptor = ArgumentCaptor.forClass(List.class);
        verify(mWificondControl).startScan(
                eq(WIFI_IFACE_NAME), eq(WifiScanner.SCAN_TYPE_HIGH_ACCURACY),
                eq(SCAN_FREQ_SET), ssidSetCaptor.capture());
        List<byte[]> ssidSet = ssidSetCaptor.getValue();
        assertArrayEquals(ssidSet.toArray(), SCAN_HIDDEN_NETWORK_BYTE_SSID_SET.toArray());
    }

    /**
     * Verifies that startPnoscan() calls underlying WificondControl.
     */
    @Test
    public void testStartPnoScanOnRequestProcessed() throws Exception {
        mWifiNative.startPnoScan(WIFI_IFACE_NAME, TEST_PNO_SETTINGS);

        ArgumentCaptor<WifiNl80211Manager.PnoScanRequestCallback> captor =
                ArgumentCaptor.forClass(WifiNl80211Manager.PnoScanRequestCallback.class);
        verify(mWificondControl).startPnoScan(eq(WIFI_IFACE_NAME),
                eq(TEST_PNO_SETTINGS.toNativePnoSettings()), any(), captor.capture());
        captor.getValue().onPnoRequestSucceeded();
        verify(mWifiMetrics).incrementPnoScanStartAttemptCount();
    }

    /**
     * Verifies that startPnoscan() calls underlying WificondControl.
     */
    @Test
    public void testStartPnoScanOnRequestFailed() throws Exception {
        mWifiNative.startPnoScan(WIFI_IFACE_NAME, TEST_PNO_SETTINGS);

        ArgumentCaptor<WifiNl80211Manager.PnoScanRequestCallback> captor =
                ArgumentCaptor.forClass(WifiNl80211Manager.PnoScanRequestCallback.class);
        verify(mWificondControl).startPnoScan(eq(WIFI_IFACE_NAME),
                eq(TEST_PNO_SETTINGS.toNativePnoSettings()), any(), captor.capture());
        captor.getValue().onPnoRequestFailed();
        verify(mWifiMetrics).incrementPnoScanStartAttemptCount();
        verify(mWifiMetrics).incrementPnoScanFailedCount();
    }

    /**
     * Verifies that stopPnoscan() calls underlying WificondControl.
     */
    @Test
    public void testStopPnoScan() throws Exception {
        mWifiNative.stopPnoScan(WIFI_IFACE_NAME);
        verify(mWificondControl).stopPnoScan(WIFI_IFACE_NAME);
    }

    /**
     * Verifies that getScanResults() can parse NativeScanResult from wificond correctly,
     */
    @Test
    public void testGetScanResults() {
        // Mock the returned array of NativeScanResult.
        List<NativeScanResult> mockScanResults = Arrays.asList(MOCK_NATIVE_SCAN_RESULT);
        when(mWificondControl.getScanResults(anyString(), anyInt())).thenReturn(mockScanResults);

        ArrayList<ScanDetail> returnedScanResults = mWifiNative.getScanResults(WIFI_IFACE_NAME);
        assertEquals(mockScanResults.size(), returnedScanResults.size());
        // Since NativeScanResult is organized differently from ScanResult, this only checks
        // a few fields.
        for (int i = 0; i < mockScanResults.size(); i++) {
            assertArrayEquals(mockScanResults.get(i).getSsid(),
                    returnedScanResults.get(i).getScanResult().SSID.getBytes());
            assertEquals(mockScanResults.get(i).getFrequencyMhz(),
                    returnedScanResults.get(i).getScanResult().frequency);
            assertEquals(mockScanResults.get(i).getTsf(),
                    returnedScanResults.get(i).getScanResult().timestamp);
        }
    }

    /**
     * Verifies that getScanResults() can parse NativeScanResult from wificond correctly,
     * when there is radio chain info.
     */
    @Test
    public void testGetScanResultsWithRadioChainInfo() throws Exception {
        // Mock the returned array of NativeScanResult.
        NativeScanResult nativeScanResult = createMockNativeScanResult();
        // Add radio chain info
        List<RadioChainInfo> nativeRadioChainInfos = Arrays.asList(
                MOCK_NATIVE_RADIO_CHAIN_INFO_1, MOCK_NATIVE_RADIO_CHAIN_INFO_2);
        nativeScanResult.radioChainInfos = nativeRadioChainInfos;
        List<NativeScanResult> mockScanResults = Arrays.asList(nativeScanResult);

        when(mWificondControl.getScanResults(anyString(), anyInt())).thenReturn(mockScanResults);

        ArrayList<ScanDetail> returnedScanResults = mWifiNative.getScanResults(WIFI_IFACE_NAME);
        assertEquals(mockScanResults.size(), returnedScanResults.size());
        // Since NativeScanResult is organized differently from ScanResult, this only checks
        // a few fields.
        for (int i = 0; i < mockScanResults.size(); i++) {
            assertArrayEquals(mockScanResults.get(i).getSsid(),
                    returnedScanResults.get(i).getScanResult().SSID.getBytes());
            assertEquals(mockScanResults.get(i).getFrequencyMhz(),
                    returnedScanResults.get(i).getScanResult().frequency);
            assertEquals(mockScanResults.get(i).getTsf(),
                    returnedScanResults.get(i).getScanResult().timestamp);
            ScanResult.RadioChainInfo[] scanRcis = returnedScanResults.get(
                    i).getScanResult().radioChainInfos;
            assertEquals(nativeRadioChainInfos.size(), scanRcis.length);
            for (int j = 0; j < scanRcis.length; ++j) {
                assertEquals(nativeRadioChainInfos.get(j).getChainId(), scanRcis[j].id);
                assertEquals(nativeRadioChainInfos.get(j).getLevelDbm(), scanRcis[j].level);
            }
        }
    }

    /**
     * Verifies that connectToNetwork() calls underlying WificondControl and SupplicantStaIfaceHal.
     */
    @Test
    public void testConnectToNetwork() throws Exception {
        WifiConfiguration config = mock(WifiConfiguration.class);
        mWifiNative.connectToNetwork(WIFI_IFACE_NAME, config);
        // connectToNetwork() should abort ongoing scan before connection.
        verify(mWificondControl).abortScan(WIFI_IFACE_NAME);
        verify(mStaIfaceHal).connectToNetwork(WIFI_IFACE_NAME, config);
    }

    /**
     * Verifies that roamToNetwork() calls underlying WificondControl and SupplicantStaIfaceHal.
     */
    @Test
    public void testRoamToNetwork() throws Exception {
        WifiConfiguration config = mock(WifiConfiguration.class);
        mWifiNative.roamToNetwork(WIFI_IFACE_NAME, config);
        // roamToNetwork() should abort ongoing scan before connection.
        verify(mWificondControl).abortScan(WIFI_IFACE_NAME);
        verify(mStaIfaceHal).roamToNetwork(WIFI_IFACE_NAME, config);
    }

    /**
     * Verifies that setMacAddress() calls underlying WifiVendorHal.
     */
    @Test
    public void testSetMacAddress() throws Exception {
        mWifiNative.setMacAddress(WIFI_IFACE_NAME, TEST_MAC_ADDRESS);
        verify(mStaIfaceHal).disconnect(WIFI_IFACE_NAME);
        verify(mWifiVendorHal).setMacAddress(WIFI_IFACE_NAME, TEST_MAC_ADDRESS);
    }

    /**
     * Verifies that isSetMacAddressSupported() calls underlying WifiVendorHal.
     */
    @Test
    public void testIsSetMacAddressSupported() throws Exception {
        mWifiNative.isSetMacAddressSupported(WIFI_IFACE_NAME);
        verify(mWifiVendorHal).isSetMacAddressSupported(WIFI_IFACE_NAME);
    }

    /**
     * Test that selectTxPowerScenario() calls into WifiVendorHal (success case)
     */
    @Test
    public void testSelectTxPowerScenario_success() throws Exception {
        when(mWifiVendorHal.selectTxPowerScenario(any(SarInfo.class))).thenReturn(true);
        SarInfo sarInfo = new SarInfo();
        assertTrue(mWifiNative.selectTxPowerScenario(sarInfo));
        verify(mWifiVendorHal).selectTxPowerScenario(sarInfo);
    }

    /**
     * Test that selectTxPowerScenario() calls into WifiVendorHal (failure case)
     */
    @Test
    public void testSelectTxPowerScenario_failure() throws Exception {
        when(mWifiVendorHal.selectTxPowerScenario(any(SarInfo.class))).thenReturn(false);
        SarInfo sarInfo = new SarInfo();
        assertFalse(mWifiNative.selectTxPowerScenario(sarInfo));
        verify(mWifiVendorHal).selectTxPowerScenario(sarInfo);
    }

    /**
     * Test that setPowerSave() with true, results in calling into SupplicantStaIfaceHal
     */
    @Test
    public void testSetPowerSaveTrue() throws Exception {
        mWifiNative.setPowerSave(WIFI_IFACE_NAME, true);
        verify(mStaIfaceHal).setPowerSave(WIFI_IFACE_NAME, true);
    }

    /**
     * Test that setPowerSave() with false, results in calling into SupplicantStaIfaceHal
     */
    @Test
    public void testSetPowerSaveFalse() throws Exception {
        mWifiNative.setPowerSave(WIFI_IFACE_NAME, false);
        verify(mStaIfaceHal).setPowerSave(WIFI_IFACE_NAME, false);
    }

    /**
     * Test that setLowLatencyMode() with true, results in calling into WifiVendorHal
     */
    @Test
    public void testLowLatencyModeTrue() throws Exception {
        when(mWifiVendorHal.setLowLatencyMode(anyBoolean())).thenReturn(true);
        assertTrue(mWifiNative.setLowLatencyMode(true));
        verify(mWifiVendorHal).setLowLatencyMode(true);
    }

    /**
     * Test that setLowLatencyMode() with false, results in calling into WifiVendorHal
     */
    @Test
    public void testLowLatencyModeFalse() throws Exception {
        when(mWifiVendorHal.setLowLatencyMode(anyBoolean())).thenReturn(true);
        assertTrue(mWifiNative.setLowLatencyMode(false));
        verify(mWifiVendorHal).setLowLatencyMode(false);
    }

    /**
     * Test that setLowLatencyMode() returns with failure when WifiVendorHal fails.
     */
    @Test
    public void testSetLowLatencyModeFail() throws Exception {
        final boolean lowLatencyMode = true;
        when(mWifiVendorHal.setLowLatencyMode(anyBoolean())).thenReturn(false);
        assertFalse(mWifiNative.setLowLatencyMode(lowLatencyMode));
        verify(mWifiVendorHal).setLowLatencyMode(lowLatencyMode);
    }

    @Test
    public void testGetFactoryMacAddress() throws Exception {
        when(mWifiVendorHal.getFactoryMacAddress(any())).thenReturn(MacAddress.BROADCAST_ADDRESS);
        assertNotNull(mWifiNative.getFactoryMacAddress(WIFI_IFACE_NAME));
        verify(mWifiVendorHal).getFactoryMacAddress(any());
    }

    /**
     * Test that flushRingBufferData(), results in calling into WifiVendorHal
     */
    @Test
    public void testFlushRingBufferDataTrue() throws Exception {
        when(mWifiVendorHal.flushRingBufferData()).thenReturn(true);
        assertTrue(mWifiNative.flushRingBufferData());
        verify(mWifiVendorHal).flushRingBufferData();
    }

    /**
     * Tests that WifiNative#sendMgmtFrame() calls WificondControl#sendMgmtFrame()
     */
    @Test
    public void testSendMgmtFrame() {
        mWifiNative.sendMgmtFrame(WIFI_IFACE_NAME, FATE_REPORT_FRAME_BYTES,
                mSendMgmtFrameCallback, TEST_MCS_RATE);

        verify(mWificondControl).sendMgmtFrame(eq(WIFI_IFACE_NAME),
                AdditionalMatchers.aryEq(FATE_REPORT_FRAME_BYTES), eq(TEST_MCS_RATE),
                any(), eq(mSendMgmtFrameCallback));
    }

    /**
     * Tests that probeLink() generates the correct frame and calls WificondControl#sendMgmtFrame().
     */
    @Test
    public void testProbeLinkSuccess() {
        byte[] expectedFrame = {
                0x40, 0x00, 0x3c, 0x00, (byte) 0xa8, (byte) 0xbd, 0x27, 0x5b,
                0x33, 0x72, (byte) 0xf4, (byte) 0xf5, (byte) 0xe8, 0x51, (byte) 0x9e, 0x09,
                (byte) 0xa8, (byte) 0xbd, 0x27, 0x5b, 0x33, 0x72, (byte) 0xb0, 0x66,
                0x00, 0x00
        };

        when(mStaIfaceHal.getMacAddress(WIFI_IFACE_NAME)).thenReturn(TEST_MAC_ADDRESS_STR);

        when(mRandom.nextInt()).thenReturn(TEST_SEQUENCE_NUM);

        mWifiNative.probeLink(WIFI_IFACE_NAME, MacAddress.fromString(TEST_BSSID_STR),
                mSendMgmtFrameCallback, TEST_MCS_RATE);

        verify(mSendMgmtFrameCallback, never()).onFailure(anyInt());
        verify(mWificondControl).sendMgmtFrame(eq(WIFI_IFACE_NAME),
                AdditionalMatchers.aryEq(expectedFrame), eq(TEST_MCS_RATE),
                any(), eq(mSendMgmtFrameCallback));
    }

    /**
     * Tests that probeLink() triggers the failure callback when it cannot get the sender MAC
     * address.
     */
    @Test
    public void testProbeLinkFailureCannotGetSenderMac() {
        when(mStaIfaceHal.getMacAddress(WIFI_IFACE_NAME)).thenReturn(null);

        mWifiNative.probeLink(WIFI_IFACE_NAME, MacAddress.fromString(TEST_BSSID_STR),
                mSendMgmtFrameCallback, TEST_MCS_RATE);

        verify(mSendMgmtFrameCallback).onFailure(
                WifiNl80211Manager.SEND_MGMT_FRAME_ERROR_UNKNOWN);
        verify(mWificondControl, never()).sendMgmtFrame(any(), any(), anyInt(), any(), any());
    }

    /**
     * Tests that probeLink() triggers the failure callback when it cannot get the BSSID.
     */
    @Test
    public void testProbeLinkFailureCannotGetBssid() {
        when(mStaIfaceHal.getMacAddress(WIFI_IFACE_NAME)).thenReturn(TEST_MAC_ADDRESS_STR);

        mWifiNative.probeLink(WIFI_IFACE_NAME, null, mSendMgmtFrameCallback, TEST_MCS_RATE);

        verify(mSendMgmtFrameCallback).onFailure(
                WifiNl80211Manager.SEND_MGMT_FRAME_ERROR_UNKNOWN);
        verify(mWificondControl, never()).sendMgmtFrame(any(), any(), anyInt(), any(), any());
    }

    /**
     * Tests that WifiNative#addHlpReq() calls
     * SupplicantStaIfaceHal#addHlpReq()
     */
    @Test
    public void testaddHlpReq() {
        byte[] hlpPacket = {
                0x40, 0x00, 0x3c, 0x00, (byte) 0xa8, (byte) 0xbd, 0x27, 0x5b,
                0x33, 0x72, (byte) 0xf4, (byte) 0xf5, (byte) 0xe8, 0x51, (byte) 0x9e, 0x09,
                (byte) 0xa8, (byte) 0xbd, 0x27, 0x5b, 0x33, 0x72, (byte) 0xb0, 0x66,
                0x00, 0x00
        };
        mWifiNative.addHlpReq(WIFI_IFACE_NAME, TEST_MAC_ADDRESS, hlpPacket);

        verify(mStaIfaceHal).addHlpReq(eq(WIFI_IFACE_NAME),
                eq(TEST_MAC_ADDRESS.toByteArray()), eq(hlpPacket));
    }

    /**
     * Tests that WifiNative#flushAllHlp() calls
     * SupplicantStaIfaceHal#flushAllHlp()
     */
    @Test
    public void testflushAllHlp() {
        mWifiNative.flushAllHlp(WIFI_IFACE_NAME);

        verify(mStaIfaceHal).flushAllHlp(eq(WIFI_IFACE_NAME));
    }
}
