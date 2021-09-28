/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.server.wifi.scanner;

import static com.android.server.wifi.ScanTestUtil.NativeScanSettingsBuilder;
import static com.android.server.wifi.ScanTestUtil.setupMockChannels;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.net.wifi.WifiScanner;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.ScanDetail;
import com.android.server.wifi.ScanResults;
import com.android.server.wifi.WifiMonitor;
import com.android.server.wifi.WifiNative;
import com.android.server.wifi.scanner.ChannelHelper.ChannelCollection;

import org.junit.Before;
import org.junit.Test;
import org.mockito.InOrder;


import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * Unit tests for {@link com.android.server.wifi.scanner.WificondScannerImpl}.
 */
@SmallTest
public class WificondScannerTest extends BaseWifiScannerImplTest {
    private static final String NATIVE_SCAN_TITLE = "Latest native scan results:";
    private static final String NATIVE_PNO_SCAN_TITLE = "Latest native pno scan results:";
    private static final String NATIVE_SCAN_IE_TITLE = "Latest native scan results IEs:";

    WifiMonitor mWifiMonitorSpy;
    @Before
    public void setup() throws Exception {
        setupMockChannels(mWifiNative,
                new int[]{2400, 2450},
                new int[]{5150, 5175},
                new int[]{5600, 5650},
                new int[]{5945, 5985});
        mWifiMonitorSpy = spy(mWifiMonitor);
        mScanner = new WificondScannerImpl(mContext, BaseWifiScannerImplTest.IFACE_NAME,
                mWifiNative, mWifiMonitorSpy, new WificondChannelHelper(mWifiNative),
                mLooper.getLooper(), mClock);
    }

    /**
     * Test that WificondScannerImpl will not issue a scan and report scan failure
     * when there is no channel to scan for.
     */
    @Test
    public void singleScanNotIssuedIfNoAvailableChannels() {
        // Use mocked ChannelHelper and ChannelCollection to simulate the scenario
        // that no channel is available for this request.
        ChannelHelper channelHelper = mock(ChannelHelper.class);
        ChannelCollection channelCollection = mock(ChannelCollection.class);
        when(channelHelper.createChannelCollection()).thenReturn(channelCollection);
        when(channelCollection.isEmpty()).thenReturn(true);

        mScanner = new WificondScannerImpl(mContext, BaseWifiScannerImplTest.IFACE_NAME,
                mWifiNative, mWifiMonitor, channelHelper, mLooper.getLooper(), mClock);

        WifiNative.ScanSettings settings = new NativeScanSettingsBuilder()
                .withBasePeriod(10000) // ms
                .withMaxApPerScan(10)
                .addBucketWithBand(10000 /* ms */, WifiScanner.REPORT_EVENT_AFTER_EACH_SCAN,
                        WifiScanner.WIFI_BAND_5_GHZ)
                .build();
        WifiNative.ScanEventHandler eventHandler = mock(WifiNative.ScanEventHandler.class);
        mScanner.startSingleScan(settings, eventHandler);

        mLooper.dispatchAll();

        // No scan is issued to WifiNative.
        verify(mWifiNative, never()).scan(any(), anyInt(), any(), any(List.class));
        // A scan failed event must be reported.
        verify(eventHandler).onScanStatus(WifiNative.WIFI_SCAN_FAILED);
    }

    @Test
    public void externalScanResultsDoNotCauseSpuriousTimerCancellationOrCrash() {
        mWifiMonitor.sendMessage(IFACE_NAME, WifiMonitor.SCAN_RESULTS_EVENT);
        mLooper.dispatchAll();
        verify(mAlarmManager.getAlarmManager(), never()).cancel(any(PendingIntent.class));
        verify(mAlarmManager.getAlarmManager(), never())
                .cancel(any(AlarmManager.OnAlarmListener.class));
        verify(mAlarmManager.getAlarmManager(), never()).cancel(isNull(PendingIntent.class));
        verify(mAlarmManager.getAlarmManager(), never())
                .cancel(isNull(AlarmManager.OnAlarmListener.class));
    }

    @Test
    public void externalScanResultsAfterOurScanDoNotCauseSpuriousTimerCancellationOrCrash() {
        WifiNative.ScanSettings settings = new NativeScanSettingsBuilder()
                .withBasePeriod(10000) // ms
                .withMaxApPerScan(10)
                .addBucketWithBand(10000 /* ms */, WifiScanner.REPORT_EVENT_AFTER_EACH_SCAN,
                        WifiScanner.WIFI_BAND_24_GHZ)
                .build();

        doSuccessfulSingleScanTest(settings,
                expectedBandScanFreqs(WifiScanner.WIFI_BAND_24_GHZ),
                new ArrayList<String>(),
                ScanResults.create(0, WifiScanner.WIFI_BAND_24_GHZ,
                        2400, 2450, 2450, 2400, 2450, 2450, 2400, 2450, 2450), false);

        mWifiMonitor.sendMessage(IFACE_NAME, WifiMonitor.SCAN_RESULTS_EVENT);
        mLooper.dispatchAll();
        verify(mAlarmManager.getAlarmManager(), never()).cancel(any(PendingIntent.class));
        verify(mAlarmManager.getAlarmManager(), times(1))
                .cancel(any(AlarmManager.OnAlarmListener.class));
        verify(mAlarmManager.getAlarmManager(), never()).cancel(isNull(PendingIntent.class));
        verify(mAlarmManager.getAlarmManager(), never())
                .cancel(isNull(AlarmManager.OnAlarmListener.class));
    }

    @Test
    public void lateScanResultsDoNotCauseSpuriousTimerCancellationOrCrash() {
        WifiNative.ScanSettings settings = new NativeScanSettingsBuilder()
                .withBasePeriod(10000) // ms
                .withMaxApPerScan(10)
                .addBucketWithBand(10000 /* ms */, WifiScanner.REPORT_EVENT_AFTER_EACH_SCAN,
                        WifiScanner.WIFI_BAND_24_GHZ)
                .build();

        // Kick off a scan
        when(mWifiNative.scan(eq(IFACE_NAME), anyInt(), any(), any(List.class))).thenReturn(true);
        WifiNative.ScanEventHandler eventHandler = mock(WifiNative.ScanEventHandler.class);
        assertTrue(mScanner.startSingleScan(settings, eventHandler));
        mLooper.dispatchAll();

        // Report a timeout
        mAlarmManager.dispatch(WificondScannerImpl.TIMEOUT_ALARM_TAG);
        mLooper.dispatchAll();

        // Now report scan results (results lost the race with timeout)
        mWifiMonitor.sendMessage(IFACE_NAME, WifiMonitor.SCAN_RESULTS_EVENT);
        mLooper.dispatchAll();

        verify(mAlarmManager.getAlarmManager(), never()).cancel(any(PendingIntent.class));
        verify(mAlarmManager.getAlarmManager(), never())
                .cancel(any(AlarmManager.OnAlarmListener.class));
        verify(mAlarmManager.getAlarmManager(), never()).cancel(isNull(PendingIntent.class));
        verify(mAlarmManager.getAlarmManager(), never())
                .cancel(isNull(AlarmManager.OnAlarmListener.class));
    }

    /**
     * Test that dump() of WificondScannerImpl dumps native scan results with correct format when
     * scan result is empty.
     */
    @Test
    public void testDumpWithCorrectFormatWithEmptyScanResult() {
        // The format of scan dump when zero Ap in scan result.
        // ---------------------------------------------
        // "Latest native scan results:\n"    : Key word can't modify.
        // "Latest native pno scan results:\n": Key word can't modify.
        // " ...                           \n": Any String and any line. No limited.
        // "Latest native scan results IEs:\n": Key word can't modify.
        // "\n"                               : Should be stop with "\n"
        //---------------------------------------------
        Pattern zeroScanResultRegex = Pattern.compile(
                "" + NATIVE_SCAN_TITLE + "\n"
                + "" + NATIVE_PNO_SCAN_TITLE + "\n"
                + "(.*\n)*"
                + "" + NATIVE_SCAN_IE_TITLE + "\n"
                + "\n");

        String dump = dumpObject();

        assertLogContainsRequestPattern(zeroScanResultRegex, dump);

    }

    /**
     * Test that dump() of WificondScannerImpl dumps native scan results with correct format when
     * the number of AP in the scan result is not zero.
     */
    @Test
    public void testDumpWithCorrectFormatWithScanResult() {
        // Prepare the setting to trigger a scan.
        WifiNative.ScanSettings settings = new NativeScanSettingsBuilder()
                .withBasePeriod(10000)
                .withMaxApPerScan(2)
                .addBucketWithBand(10000,
                        WifiScanner.REPORT_EVENT_AFTER_EACH_SCAN
                        | WifiScanner.REPORT_EVENT_FULL_SCAN_RESULT,
                        WifiScanner.WIFI_BAND_24_GHZ)
                .build();
        long approxScanStartUs = mClock.getElapsedSinceBootMillis() * 1000;
        ArrayList<ScanDetail> rawScanResults = new ArrayList<>(Arrays.asList(
                ScanResults.generateNativeResults(0, 5150, 5171)));
        WifiNative.ScanEventHandler eventHandler = mock(WifiNative.ScanEventHandler.class);
        InOrder order = inOrder(eventHandler, mWifiNative);
        // scan succeeds
        when(mWifiNative.scan(eq(IFACE_NAME), anyInt(), any(), any(List.class))).thenReturn(true);
        when(mWifiNative.getScanResults(eq(IFACE_NAME))).thenReturn(rawScanResults);

        int ap_count = rawScanResults.size();
        // The format of scan dump when the number of AP in the scan result is not zero.
        // ---------------------------------------------
        // "Latest native scan results:\n"    : Key word can't modify.
        // "    BSSID ...              \n"    : Must start with 4 spaces but only show when
        //                                      the number of AP in the scan result is not zero.
        // "  The APs information      \n"    : Must start with 2 spaces and show bssid first.
        // "  ...                      \n"    : Continues to print AP information and
        //                                      total AP information line should be
        //                                      same as the number of AP in the scan result.
        // "Latest native pno scan results:\n": Key word can't modify.
        // " ...                           \n": Any String and any line. No limited.
        // "Latest native scan results IEs:\n": Key word can't modify.
        // ie raw data                        : loop to start print ie raw data, finish with "\n"
        //   ...                              : Continues to print ie raw data and
        //                                      total ie raw data line should be same as
        //                                      the number of AP in the scan result.
        // "\n"                               : Should be stop with "\n"
        //---------------------------------------------
        Pattern scanResultRegex = Pattern.compile(
                "" + NATIVE_SCAN_TITLE + "\n"
                + "    .*\n"
                + "(  .{2}:.{2}:.{2}:.{2}:.{2}:.{2}.*\n){" + ap_count + "}"
                + "" + NATIVE_PNO_SCAN_TITLE + "\n"
                + "(.*\n)*"
                + "" + NATIVE_SCAN_IE_TITLE + "\n"
                + "(.*\n){" + ap_count + "}"
                + "\n");

        // Trigger a scan to update mNativeScanResults in WificondScannerImpl.
        assertTrue(mScanner.startSingleScan(settings, eventHandler));
        Set<Integer> expectedScan = expectedBandScanFreqs(WifiScanner.WIFI_BAND_24_GHZ);
        order.verify(mWifiNative).scan(eq(IFACE_NAME), anyInt(), eq(expectedScan), any(List.class));

        // Notify scan has finished
        mWifiMonitor.sendMessage(eq(IFACE_NAME), WifiMonitor.SCAN_RESULTS_EVENT);
        mLooper.dispatchAll();
        // Get the dump string.
        String dump = dumpObject();

        assertLogContainsRequestPattern(scanResultRegex, dump);
    }

    @Test
    public void cleanupDeregistersHandlers() {
        mScanner.cleanup();
        verify(mWifiMonitorSpy, times(1)).deregisterHandler(anyString(),
                eq(WifiMonitor.SCAN_FAILED_EVENT), any());
        verify(mWifiMonitorSpy, times(1)).deregisterHandler(anyString(),
                eq(WifiMonitor.PNO_SCAN_RESULTS_EVENT), any());
        verify(mWifiMonitorSpy, times(1)).deregisterHandler(anyString(),
                eq(WifiMonitor.SCAN_RESULTS_EVENT), any());
    }

    private void assertLogContainsRequestPattern(Pattern logLineRegex, String log) {
        assertTrue("dump did not contain log = " + logLineRegex.toString() + "\n" + log + "\n",
                logLineRegex.matcher(log).find());
    }

    private String dumpObject() {
        StringWriter stringWriter = new StringWriter();
        mScanner.dump(new FileDescriptor(), new PrintWriter(stringWriter),
                new String[0]);
        return stringWriter.toString();
    }
}
