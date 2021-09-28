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

import static android.app.ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND_SERVICE;

import static com.android.server.wifi.ScanRequestProxy.SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS;
import static com.android.server.wifi.ScanRequestProxy.SCAN_REQUEST_THROTTLE_TIME_WINDOW_FG_APPS_MS;
import static com.android.server.wifi.WifiSettingsConfigStore.WIFI_SCAN_THROTTLE_ENABLED;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import android.app.ActivityManager;
import android.app.AppOpsManager;
import android.content.Context;
import android.content.Intent;
import android.net.wifi.IScanResultsCallback;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiScanner;
import android.net.wifi.WifiScanner.ScanSettings.HiddenNetwork;
import android.os.Handler;
import android.os.IBinder;
import android.os.UserHandle;
import android.os.WorkSource;
import android.os.test.TestLooper;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.util.WifiPermissionsUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Unit tests for {@link com.android.server.wifi.ScanRequestProxy}.
 */
@SmallTest
public class ScanRequestProxyTest extends WifiBaseTest {
    private static final int TEST_UID = 5;
    private static final String TEST_PACKAGE_NAME_1 = "com.test.1";
    private static final String TEST_PACKAGE_NAME_2 = "com.test.2";
    private static final List<HiddenNetwork> TEST_HIDDEN_NETWORKS_LIST =
            new ArrayList<HiddenNetwork>() {{
                add(new HiddenNetwork("test_ssid_1"));
                add(new HiddenNetwork("test_ssid_2"));

            }};
    private static final List<HiddenNetwork> TEST_HIDDEN_NETWORKS_LIST_NS =
            new ArrayList<HiddenNetwork>() {{
                add(new HiddenNetwork("test_ssid_3"));
                add(new HiddenNetwork("test_ssid_4"));
            }};

    @Mock private Context mContext;
    @Mock private AppOpsManager mAppOps;
    @Mock private ActivityManager mActivityManager;
    @Mock private WifiInjector mWifiInjector;
    @Mock private WifiConfigManager mWifiConfigManager;
    @Mock private WifiScanner mWifiScanner;
    @Mock private WifiPermissionsUtil mWifiPermissionsUtil;
    @Mock private WifiMetrics mWifiMetrics;
    @Mock private Clock mClock;
    @Mock private WifiSettingsConfigStore mWifiSettingsConfigStore;
    @Mock private WifiNetworkSuggestionsManager mWifiNetworkSuggestionsManager;
    @Mock private IScanResultsCallback mScanResultsCallback;
    @Mock private IScanResultsCallback mAnotherScanResultsCallback;
    @Mock private TestLooper mLooper;
    @Mock private IBinder mBinder;
    @Mock private IBinder mAnotherBinder;

    private ArgumentCaptor<WorkSource> mWorkSourceArgumentCaptor =
            ArgumentCaptor.forClass(WorkSource.class);
    private ArgumentCaptor<WifiScanner.ScanSettings> mScanSettingsArgumentCaptor =
            ArgumentCaptor.forClass(WifiScanner.ScanSettings.class);
    private ArgumentCaptor<WifiScanner.ScanListener> mScanRequestListenerArgumentCaptor =
            ArgumentCaptor.forClass(WifiScanner.ScanListener.class);
    private ArgumentCaptor<WifiScanner.ScanListener> mGlobalScanListenerArgumentCaptor =
            ArgumentCaptor.forClass(WifiScanner.ScanListener.class);
    private WifiScanner.ScanData[] mTestScanDatas1;
    private WifiScanner.ScanData[] mTestScanDatas2;
    private InOrder mInOrder;

    private ScanRequestProxy mScanRequestProxy;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        when(mWifiInjector.getWifiScanner()).thenReturn(mWifiScanner);
        when(mWifiInjector.getWifiNetworkSuggestionsManager())
                .thenReturn(mWifiNetworkSuggestionsManager);
        when(mWifiConfigManager.retrieveHiddenNetworkList()).thenReturn(TEST_HIDDEN_NETWORKS_LIST);
        when(mWifiNetworkSuggestionsManager.retrieveHiddenNetworkList())
                .thenReturn(TEST_HIDDEN_NETWORKS_LIST_NS);
        doNothing().when(mWifiScanner).registerScanListener(
                any(),
                mGlobalScanListenerArgumentCaptor.capture());
        doNothing().when(mWifiScanner).startScan(
                mScanSettingsArgumentCaptor.capture(),
                any(),
                mScanRequestListenerArgumentCaptor.capture(),
                mWorkSourceArgumentCaptor.capture());

        mInOrder = inOrder(mWifiScanner, mWifiConfigManager,
                mContext, mWifiNetworkSuggestionsManager);
        mTestScanDatas1 =
                ScanTestUtil.createScanDatas(new int[][]{{ 2417, 2427, 5180, 5170 }},
                        new int[]{0},
                        new int[]{WifiScanner.WIFI_BAND_ALL});
        mTestScanDatas2 =
                ScanTestUtil.createScanDatas(new int[][]{{ 2412, 2422, 5200, 5210 }},
                        new int[]{0},
                        new int[]{WifiScanner.WIFI_BAND_ALL});

        // Scan throttling is enabled by default.
        when(mWifiSettingsConfigStore.get(eq(WIFI_SCAN_THROTTLE_ENABLED))).thenReturn(true);
        mLooper = new TestLooper();
        mScanRequestProxy =
            new ScanRequestProxy(mContext, mAppOps, mActivityManager, mWifiInjector,
                    mWifiConfigManager, mWifiPermissionsUtil, mWifiMetrics, mClock,
                    new Handler(mLooper.getLooper()), mWifiSettingsConfigStore);
        when(mScanResultsCallback.asBinder()).thenReturn(mBinder);
        when(mAnotherScanResultsCallback.asBinder()).thenReturn(mAnotherBinder);
    }

    @After
    public void cleanUp() throws Exception {
        verifyNoMoreInteractions(mWifiScanner, mWifiConfigManager, mContext, mWifiMetrics);
        validateMockitoUsage();
    }

    private void enableScanning() {
        // Enable scanning
        mScanRequestProxy.enableScanning(true, false);
        mInOrder.verify(mWifiScanner).registerScanListener(any(), any());
        mInOrder.verify(mWifiScanner).setScanningEnabled(true);
        validateScanAvailableBroadcastSent(true);

        when(mClock.getElapsedSinceBootMillis()).thenReturn(782L);
    }

    /**
     * Verify scan enable sequence.
     */
    @Test
    public void testEnableScanning() {
        mScanRequestProxy.enableScanning(true, false);
        mInOrder.verify(mWifiScanner).registerScanListener(any(), any());
        mInOrder.verify(mWifiScanner).setScanningEnabled(true);
        validateScanAvailableBroadcastSent(true);
    }

    /**
     * Verify scan disable sequence.
     */
    @Test
    public void testDisableScanning() {
        mScanRequestProxy.enableScanning(false, false);
        mInOrder.verify(mWifiScanner).registerScanListener(any(), any());
        mInOrder.verify(mWifiScanner).setScanningEnabled(false);
        validateScanAvailableBroadcastSent(false);
    }

    /**
     * Verify scan request will be rejected if we cannot get a handle to wifiscanner.
     */
    @Test
    public void testStartScanFailWithoutScanner() {
        when(mWifiInjector.getWifiScanner()).thenReturn(null);
        assertFalse(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        validateScanResultsFailureBroadcastSent(TEST_PACKAGE_NAME_1);
    }

    /**
     * Verify scan request will forwarded to wifiscanner if wifiscanner is present.
     */
    @Test
    public void testStartScanSuccess() {
        enableScanning();
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());

        assertTrue(mWorkSourceArgumentCaptor.getValue().equals(
                new WorkSource(TEST_UID, TEST_PACKAGE_NAME_1)));
        validateScanSettings(mScanSettingsArgumentCaptor.getValue(), false);

        verify(mWifiMetrics).incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Verify scan request will forwarded to wifiscanner if wifiscanner is present.
     */
    @Test
    public void testStartScanSuccessFromAppWithNetworkSettings() {
        enableScanning();
        when(mWifiPermissionsUtil.checkNetworkSettingsPermission(TEST_UID)).thenReturn(true);
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());

        assertTrue(mWorkSourceArgumentCaptor.getValue().equals(
                new WorkSource(TEST_UID, TEST_PACKAGE_NAME_1)));
        validateScanSettings(mScanSettingsArgumentCaptor.getValue(), false, true);
    }

    /**
     * Verify scan request will forwarded to wifiscanner if wifiscanner is present.
     */
    @Test
    public void testStartScanSuccessFromAppWithNetworkSetupWizard() {
        enableScanning();
        when(mWifiPermissionsUtil.checkNetworkSetupWizardPermission(TEST_UID)).thenReturn(true);
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());

        assertEquals(mWorkSourceArgumentCaptor.getValue(),
                new WorkSource(TEST_UID, TEST_PACKAGE_NAME_1));
        validateScanSettings(mScanSettingsArgumentCaptor.getValue(), false, true);
    }

    /**
     * Verify that hidden network list is not retrieved when hidden network scanning is disabled.
     */
    @Test
    public void testStartScanWithHiddenNetworkScanningDisabled() {
        mScanRequestProxy.enableScanning(true, false);
        mInOrder.verify(mWifiScanner).registerScanListener(any(), any());
        mInOrder.verify(mWifiScanner).setScanningEnabled(true);
        validateScanAvailableBroadcastSent(true);

        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiConfigManager, never()).retrieveHiddenNetworkList();
        mInOrder.verify(mWifiNetworkSuggestionsManager, never()).retrieveHiddenNetworkList();
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());

        assertEquals(mWorkSourceArgumentCaptor.getValue(),
                new WorkSource(TEST_UID, TEST_PACKAGE_NAME_1));
        validateScanSettings(mScanSettingsArgumentCaptor.getValue(), false);

        verify(mWifiMetrics).incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Verify that hidden network list is retrieved when hidden network scanning is enabled.
     */
    @Test
    public void testStartScanWithHiddenNetworkScanningEnabled() {
        mScanRequestProxy.enableScanning(true, true);
        mInOrder.verify(mWifiScanner).registerScanListener(any(), any());
        mInOrder.verify(mWifiScanner).setScanningEnabled(true);
        validateScanAvailableBroadcastSent(true);

        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));

        mInOrder.verify(mWifiConfigManager).retrieveHiddenNetworkList();
        mInOrder.verify(mWifiNetworkSuggestionsManager).retrieveHiddenNetworkList();
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());

        assertEquals(mWorkSourceArgumentCaptor.getValue(),
                new WorkSource(TEST_UID, TEST_PACKAGE_NAME_1));
        validateScanSettings(mScanSettingsArgumentCaptor.getValue(), true);

        verify(mWifiMetrics).incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Verify a successful scan request and processing of scan results.
     */
    @Test
    public void testScanSuccess() {
        // Make a scan request.
        testStartScanSuccess();

        // Verify the scan results processing.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas1);
        validateScanResultsAvailableBroadcastSent(true);

        // Validate the scan results in the cache.
        ScanTestUtil.assertScanResultsEquals(
                mTestScanDatas1[0].getResults(),
                mScanRequestProxy.getScanResults().stream().toArray(ScanResult[]::new));

        verify(mWifiMetrics).incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Verify a successful scan request and processing of scan failure.
     */
    @Test
    public void testScanFailure() {
        // Make a scan request.
        testStartScanSuccess();

        // Verify the scan failure processing.
        mScanRequestListenerArgumentCaptor.getValue().onFailure(0, "failed");
        validateScanResultsAvailableBroadcastSent(false);

        // Ensure scan results in the cache is empty.
        assertTrue(mScanRequestProxy.getScanResults().isEmpty());

        verify(mWifiMetrics).incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Verify processing of successive successful scans.
     */
    @Test
    public void testScanSuccessOverwritesPreviousResults() {
        enableScanning();
        // Make scan request 1.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        // Verify the scan results processing for request 1.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas1);
        validateScanResultsAvailableBroadcastSent(true);
        // Validate the scan results in the cache.
        ScanTestUtil.assertScanResultsEquals(
                mTestScanDatas1[0].getResults(),
                mScanRequestProxy.getScanResults().stream().toArray(ScanResult[]::new));

        // Make scan request 2.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_2));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        // Verify the scan results processing for request 2.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas2);
        validateScanResultsAvailableBroadcastSent(true);
        // Validate the scan results in the cache.
        ScanTestUtil.assertScanResultsEquals(
                mTestScanDatas2[0].getResults(),
                mScanRequestProxy.getScanResults().stream().toArray(ScanResult[]::new));

        verify(mWifiMetrics, times(2)).incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Verify processing of a successful scan followed by a failure.
     */
    @Test
    public void testScanFailureDoesNotOverwritePreviousResults() {
        enableScanning();
        // Make scan request 1.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        // Verify the scan results processing for request 1.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas1);
        validateScanResultsAvailableBroadcastSent(true);
        // Validate the scan results in the cache.
        ScanTestUtil.assertScanResultsEquals(
                mTestScanDatas1[0].getResults(),
                mScanRequestProxy.getScanResults().stream().toArray(ScanResult[]::new));

        // Make scan request 2.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_2));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        // Verify the scan failure processing.
        mScanRequestListenerArgumentCaptor.getValue().onFailure(0, "failed");
        validateScanResultsAvailableBroadcastSent(false);
        // Validate the scan results from a previous successful scan in the cache.
        ScanTestUtil.assertScanResultsEquals(
                mTestScanDatas1[0].getResults(),
                mScanRequestProxy.getScanResults().stream().toArray(ScanResult[]::new));

        verify(mWifiMetrics, times(2)).incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Verify processing of a new scan request after a previous scan success.
     * Verify that we send out two broadcasts (two successes).
     */
    @Test
    public void testNewScanRequestAfterPreviousScanSucceeds() {
        enableScanning();
        // Make scan request 1.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        // Now send the scan results for request 1.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas1);
        validateScanResultsAvailableBroadcastSent(true);
        // Validate the scan results in the cache.
        ScanTestUtil.assertScanResultsEquals(
                mTestScanDatas1[0].getResults(),
                mScanRequestProxy.getScanResults().stream().toArray(ScanResult[]::new));

        // Make scan request 2.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_2));
        // Ensure that we did send a second scan request to scanner.
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        // Now send the scan results for request 2.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas2);
        validateScanResultsAvailableBroadcastSent(true);
        // Validate the scan results in the cache.
        ScanTestUtil.assertScanResultsEquals(
                mTestScanDatas2[0].getResults(),
                mScanRequestProxy.getScanResults().stream().toArray(ScanResult[]::new));

        verify(mWifiMetrics, times(2)).incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Verify processing of a new scan request after a previous scan success, but with bad scan
     * data.
     * Verify that we send out two broadcasts (one failure & one success).
     */
    @Test
    public void testNewScanRequestAfterPreviousScanSucceedsWithInvalidScanDatas() {
        enableScanning();
        // Make scan request 1.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());

        // Now send scan success for request 1, but with invalid scan datas.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(
                new WifiScanner.ScanData[] {mTestScanDatas1[0], mTestScanDatas2[0]});
        validateScanResultsAvailableBroadcastSent(false);
        // Validate the scan results in the cache.
        assertTrue(mScanRequestProxy.getScanResults().isEmpty());

        // Make scan request 2.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_2));
        // Ensure that we did send a second scan request to scanner.
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        // Now send the scan results for request 2.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas2);
        validateScanResultsAvailableBroadcastSent(true);
        // Validate the scan results in the cache.
        ScanTestUtil.assertScanResultsEquals(
                mTestScanDatas2[0].getResults(),
                mScanRequestProxy.getScanResults().stream().toArray(ScanResult[]::new));

        verify(mWifiMetrics, times(2)).incrementExternalAppOneshotScanRequestsCount();
    }


    /**
     * Verify processing of a new scan request after a previous scan failure.
     * Verify that we send out two broadcasts (one failure & one success).
     */
    @Test
    public void testNewScanRequestAfterPreviousScanFailure() {
        enableScanning();
        // Make scan request 1.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());

        // Now send scan failure for request 1.
        mScanRequestListenerArgumentCaptor.getValue().onFailure(0, "failed");
        validateScanResultsAvailableBroadcastSent(false);
        // Validate the scan results in the cache.
        assertTrue(mScanRequestProxy.getScanResults().isEmpty());

        // Make scan request 2.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_2));
        // Ensure that we did send a second scan request to scanner.
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        // Now send the scan results for request 2.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas2);
        validateScanResultsAvailableBroadcastSent(true);
        // Validate the scan results in the cache.
        ScanTestUtil.assertScanResultsEquals(
                mTestScanDatas2[0].getResults(),
                mScanRequestProxy.getScanResults().stream().toArray(ScanResult[]::new));

        verify(mWifiMetrics, times(2)).incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Verify that we clear scan results when scan state is toggled.
     */
    @Test
    public void testToggleScanStateClearsScanResults() {
        // Enable scanning
        mScanRequestProxy.enableScanning(true, false);
        mInOrder.verify(mWifiScanner).registerScanListener(any(), any());
        mInOrder.verify(mWifiScanner).setScanningEnabled(true);
        validateScanAvailableBroadcastSent(true);

        // Make scan request 1.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        // Verify the scan results processing for request 1.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas1);
        validateScanResultsAvailableBroadcastSent(true);
        // Validate the scan results in the cache.
        ScanTestUtil.assertScanResultsEquals(
                mTestScanDatas1[0].getResults(),
                mScanRequestProxy.getScanResults().stream().toArray(ScanResult[]::new));

        // Disable scanning
        mScanRequestProxy.enableScanning(false, false);
        verify(mWifiScanner).setScanningEnabled(false);
        validateScanAvailableBroadcastSent(false);

        assertTrue(mScanRequestProxy.getScanResults().isEmpty());

        verify(mWifiMetrics).incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Verify that we don't use the same listener for multiple scan requests.
     */
    @Test
    public void testSuccessiveScanRequestsDontUseSameListener() {
        enableScanning();
        WifiScanner.ScanListener listener1;
        WifiScanner.ScanListener listener2;
        // Make scan request 1.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        listener1 = mScanRequestListenerArgumentCaptor.getValue();

        // Make scan request 2.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_2));
        // Ensure that we did send a second scan request to scanner.
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        listener2 = mScanRequestListenerArgumentCaptor.getValue();

        assertNotEquals(listener1, listener2);

        verify(mWifiMetrics, times(2)).incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Ensure new scan requests from the same app are rejected if there are more than
     * {@link ScanRequestProxy#SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS} requests in
     * {@link ScanRequestProxy#SCAN_REQUEST_THROTTLE_TIME_WINDOW_FG_APPS_MS}
     */
    @Test
    public void testSuccessiveScanRequestFromSameFgAppThrottled() {
        enableScanning();
        long firstRequestMs = 782;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs);
        for (int i = 0; i < SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS; i++) {
            when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs + i);
            assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
            mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        }
        // Make next scan request from the same package name & ensure that it is throttled.
        assertFalse(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        validateScanResultsFailureBroadcastSent(TEST_PACKAGE_NAME_1);

        verify(mWifiMetrics, times(SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS + 1))
                .incrementExternalAppOneshotScanRequestsCount();
        verify(mWifiMetrics).incrementExternalForegroundAppOneshotScanRequestsThrottledCount();
    }

    /**
     * Ensure new scan requests from the same app are rejected if there are more than
     * {@link ScanRequestProxy#SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS} requests after
     * {@link ScanRequestProxy#SCAN_REQUEST_THROTTLE_TIME_WINDOW_FG_APPS_MS}
     */
    @Test
    public void testSuccessiveScanRequestFromSameFgAppNotThrottled() {
        enableScanning();
        long firstRequestMs = 782;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs);
        for (int i = 0; i < SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS; i++) {
            when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs + i);
            assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
            mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        }
        long lastRequestMs = firstRequestMs + SCAN_REQUEST_THROTTLE_TIME_WINDOW_FG_APPS_MS + 1;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(lastRequestMs);
        // Make next scan request from the same package name & ensure that it is not throttled.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());

        verify(mWifiMetrics, times(SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS + 1))
                .incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Ensure new scan requests from the same app with NETWORK_SETTINGS permission are not
     * throttled.
     */
    @Test
    public void testSuccessiveScanRequestFromSameAppWithNetworkSettingsPermissionNotThrottled() {
        enableScanning();
        when(mWifiPermissionsUtil.checkNetworkSettingsPermission(TEST_UID)).thenReturn(true);

        long firstRequestMs = 782;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs);
        for (int i = 0; i < SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS; i++) {
            when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs + i);
            assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
            mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        }
        // Make next scan request from the same package name & ensure that it is not throttled.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
    }

    /**
     * Ensure new scan requests from the same app with NETWORK_SETUP_WIZARD permission are not
     * throttled.
     */
    @Test
    public void testSuccessiveScanRequestFromSameAppWithNetworkSetupWizardPermissionNotThrottled() {
        enableScanning();
        when(mWifiPermissionsUtil.checkNetworkSetupWizardPermission(TEST_UID)).thenReturn(true);

        long firstRequestMs = 782;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs);
        for (int i = 0; i < SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS; i++) {
            when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs + i);
            assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
            mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        }
        // Make next scan request from the same package name & ensure that it is not throttled.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
    }

    /**
     * Ensure new scan requests from the same app are not throttled when the user turns
     * off scan throttling.
     */
    @Test
    public void testSuccessiveScanRequestFromSameAppWhenThrottlingIsDisabledNotThrottled() {
        // Triggers the scan throttle setting registration.
        testEnableScanning();
        mScanRequestProxy.setScanThrottleEnabled(false);
        verify(mWifiSettingsConfigStore).put(WIFI_SCAN_THROTTLE_ENABLED, false);

        long firstRequestMs = 782;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs);
        for (int i = 0; i < SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS; i++) {
            when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs + i);
            assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
            mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        }
        // Make next scan request from the same package name & ensure that it is not throttled.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
    }

    /**
     * Ensure new scan requests from different apps are not throttled.
     */
    @Test
    public void testSuccessiveScanRequestFromDifferentFgAppsNotThrottled() {
        enableScanning();
        long firstRequestMs = 782;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs);
        for (int i = 0; i < SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS / 2; i++) {
            when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs + i);
            assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
            mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        }
        for (int i = 0; i < SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS / 2; i++) {
            when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs + i);
            assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_2));
            mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        }
        // Make next scan request from both the package name & ensure that it is not throttled.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_2));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());

        verify(mWifiMetrics, times(SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS + 2))
                .incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Ensure new scan requests from the same app after removal & re-install is not
     * throttled.
     * Verifies that we clear the scan timestamps for apps that were removed.
     */
    @Test
    public void testSuccessiveScanRequestFromSameAppAfterRemovalAndReinstallNotThrottled() {
        enableScanning();
        long firstRequestMs = 782;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs);
        for (int i = 0; i < SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS; i++) {
            when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs + i);
            assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
            mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        }
        // Now simulate removing the app.
        mScanRequestProxy.clearScanRequestTimestampsForApp(TEST_PACKAGE_NAME_1, TEST_UID);

        // Make next scan request from the same package name (simulating a reinstall) & ensure that
        // it is not throttled.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());

        verify(mWifiMetrics, times(SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS + 1))
                .incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Ensure new scan requests after removal of the app from a different user is throttled.
     * The app has same the package name across users, but different UID's. Verifies that
     * the cache is cleared only for the specific app for a specific user when an app is removed.
     */
    @Test
    public void testSuccessiveScanRequestFromSameAppAfterRemovalOnAnotherUserThrottled() {
        enableScanning();
        long firstRequestMs = 782;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs);
        for (int i = 0; i < SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS; i++) {
            when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs + i);
            assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
            mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        }
        // Now simulate removing the app for another user (User 1).
        mScanRequestProxy.clearScanRequestTimestampsForApp(
                TEST_PACKAGE_NAME_1,
                UserHandle.getUid(UserHandle.USER_SYSTEM + 1, TEST_UID));

        // Make next scan request from the same package name & ensure that is throttled.
        assertFalse(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        validateScanResultsFailureBroadcastSent(TEST_PACKAGE_NAME_1);

        verify(mWifiMetrics, times(SCAN_REQUEST_THROTTLE_MAX_IN_TIME_WINDOW_FG_APPS + 1))
                .incrementExternalAppOneshotScanRequestsCount();
        verify(mWifiMetrics).incrementExternalForegroundAppOneshotScanRequestsThrottledCount();
    }

    /**
     * Ensure scan requests from different background apps are throttled if it's before
     * {@link ScanRequestProxy#SCAN_REQUEST_THROTTLE_INTERVAL_BG_APPS_MS}.
     */
    @Test
    public void testSuccessiveScanRequestFromBgAppsThrottled() {
        enableScanning();
        when(mActivityManager.getPackageImportance(TEST_PACKAGE_NAME_1))
                .thenReturn(IMPORTANCE_FOREGROUND_SERVICE + 1);
        when(mActivityManager.getPackageImportance(TEST_PACKAGE_NAME_2))
                .thenReturn(IMPORTANCE_FOREGROUND_SERVICE + 1);

        long firstRequestMs = 782;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs);
        // Make scan request 1.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());

        // Make scan request 2 from the different package name & ensure that it is throttled.
        assertFalse(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_2));
        validateScanResultsFailureBroadcastSent(TEST_PACKAGE_NAME_2);

        verify(mWifiMetrics, times(2)).incrementExternalAppOneshotScanRequestsCount();
        verify(mWifiMetrics).incrementExternalBackgroundAppOneshotScanRequestsThrottledCount();
    }

    /**
     * Ensure scan requests from different background apps are not throttled if it's after
     * {@link ScanRequestProxy#SCAN_REQUEST_THROTTLE_INTERVAL_BG_APPS_MS}.
     */
    @Test
    public void testSuccessiveScanRequestFromBgAppsNotThrottled() {
        enableScanning();
        when(mActivityManager.getPackageImportance(TEST_PACKAGE_NAME_1))
                .thenReturn(ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND + 1);
        when(mActivityManager.getPackageImportance(TEST_PACKAGE_NAME_2))
                .thenReturn(ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND + 1);

        long firstRequestMs = 782;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(firstRequestMs);
        // Make scan request 1.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());

        long secondRequestMs =
                firstRequestMs + ScanRequestProxy.SCAN_REQUEST_THROTTLE_INTERVAL_BG_APPS_MS + 1;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(secondRequestMs);
        // Make scan request 2 from the different package name & ensure that it is throttled.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_2));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());

        verify(mWifiMetrics, times(2)).incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Verify processing of a successful scan followed by full scan results from
     * internal scans.
     */
    @Test
    public void testFullInternalScanResultsOverwritesPreviousResults() {
        enableScanning();
        // Make scan request 1.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        // Verify the scan results processing for request 1.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas1);
        validateScanResultsAvailableBroadcastSent(true);
        // Validate the scan results in the cache.
        ScanTestUtil.assertScanResultsEquals(
                mTestScanDatas1[0].getResults(),
                mScanRequestProxy.getScanResults().stream().toArray(ScanResult[]::new));

        // Now send results from an internal full scan request.
        // Verify the scan failure processing.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas2);
        validateScanResultsAvailableBroadcastSent(true);
        // Validate the scan results in the cache.
        ScanTestUtil.assertScanResultsEquals(
                mTestScanDatas2[0].getResults(),
                mScanRequestProxy.getScanResults().stream().toArray(ScanResult[]::new));

        verify(mWifiMetrics).incrementExternalAppOneshotScanRequestsCount();
    }

    /**
     * Verify processing of a successful scan followed by partial scan results from
     * internal scans.
     */
    @Test
    public void testPartialInternalScanResultsDoesNotOverwritePreviousResults() {
        enableScanning();
        // Make scan request 1.
        assertTrue(mScanRequestProxy.startScan(TEST_UID, TEST_PACKAGE_NAME_1));
        mInOrder.verify(mWifiScanner).startScan(any(), any(), any(), any());
        // Verify the scan results processing for request 1.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas1);
        validateScanResultsAvailableBroadcastSent(true);
        // Validate the scan results in the cache.
        ScanTestUtil.assertScanResultsEquals(
                mTestScanDatas1[0].getResults(),
                mScanRequestProxy.getScanResults().stream().toArray(ScanResult[]::new));

        // Now send results from an internal partial scan request.
        mTestScanDatas2 = ScanTestUtil.createScanDatas(new int[][]{{ 2412, 2422, 5200, 5210 }},
                new int[]{0},
                new int[]{WifiScanner.WIFI_BAND_BOTH});
        // Verify the scan failure processing.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas2);
        // Validate the scan results from a previous successful scan in the cache.
        ScanTestUtil.assertScanResultsEquals(
                mTestScanDatas1[0].getResults(),
                mScanRequestProxy.getScanResults().stream().toArray(ScanResult[]::new));

        verify(mWifiMetrics).incrementExternalAppOneshotScanRequestsCount();
    }

    private void validateScanSettings(WifiScanner.ScanSettings scanSettings,
                                      boolean expectHiddenNetworks) {
        validateScanSettings(scanSettings, expectHiddenNetworks, false);
    }

    private void validateScanSettings(WifiScanner.ScanSettings scanSettings,
                                      boolean expectHiddenNetworks,
                                      boolean expectHighAccuracyType) {
        assertNotNull(scanSettings);
        assertEquals(WifiScanner.WIFI_BAND_ALL, scanSettings.band);
        if (expectHighAccuracyType) {
            assertEquals(WifiScanner.SCAN_TYPE_HIGH_ACCURACY, scanSettings.type);
        } else {
            assertEquals(WifiScanner.SCAN_TYPE_LOW_LATENCY, scanSettings.type);
        }
        assertEquals(WifiScanner.REPORT_EVENT_AFTER_EACH_SCAN
                | WifiScanner.REPORT_EVENT_FULL_SCAN_RESULT, scanSettings.reportEvents);
        List<HiddenNetwork> hiddenNetworkList =
                new ArrayList<>();
        hiddenNetworkList.addAll(TEST_HIDDEN_NETWORKS_LIST);
        hiddenNetworkList.addAll(TEST_HIDDEN_NETWORKS_LIST_NS);
        if (expectHiddenNetworks) {
            assertEquals(hiddenNetworkList.size(), scanSettings.hiddenNetworks.size());
            for (HiddenNetwork hiddenNetwork : scanSettings.hiddenNetworks) {
                validateHiddenNetworkInList(hiddenNetwork, hiddenNetworkList);
            }
        } else {
            assertEquals(Collections.emptyList(), scanSettings.hiddenNetworks);
        }
    }

    private void validateHiddenNetworkInList(
            HiddenNetwork expectedHiddenNetwork,
            List<HiddenNetwork> hiddenNetworkList) {
        for (HiddenNetwork hiddenNetwork : hiddenNetworkList) {
            if (hiddenNetwork.ssid.equals(expectedHiddenNetwork.ssid)) {
                return;
            }
        }
        fail();
    }

    private void validateScanResultsAvailableBroadcastSent(boolean expectScanSuceeded) {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<UserHandle> userHandleCaptor = ArgumentCaptor.forClass(UserHandle.class);
        mInOrder.verify(mContext).sendBroadcastAsUser(
                intentCaptor.capture(), userHandleCaptor.capture());

        assertEquals(userHandleCaptor.getValue(), UserHandle.ALL);

        Intent intent = intentCaptor.getValue();
        assertEquals(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION, intent.getAction());
        assertEquals(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT, intent.getFlags());
        boolean scanSucceeded = intent.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false);
        assertEquals(expectScanSuceeded, scanSucceeded);
    }

    private void validateScanResultsFailureBroadcastSent(String expectedPackageName) {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<UserHandle> userHandleCaptor = ArgumentCaptor.forClass(UserHandle.class);
        mInOrder.verify(mContext).sendBroadcastAsUser(
                intentCaptor.capture(), userHandleCaptor.capture());

        assertEquals(userHandleCaptor.getValue(), UserHandle.ALL);

        Intent intent = intentCaptor.getValue();
        assertEquals(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION, intent.getAction());
        assertEquals(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT, intent.getFlags());
        boolean scanSucceeded = intent.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false);
        assertFalse(scanSucceeded);
        String packageName = intent.getPackage();
        assertEquals(expectedPackageName, packageName);
    }

    private void validateScanAvailableBroadcastSent(boolean expectedScanAvailable) {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<UserHandle> userHandleCaptor = ArgumentCaptor.forClass(UserHandle.class);
        mInOrder.verify(mContext).sendStickyBroadcastAsUser(
                intentCaptor.capture(), userHandleCaptor.capture());

        assertEquals(userHandleCaptor.getValue(), UserHandle.ALL);

        Intent intent = intentCaptor.getValue();
        assertEquals(WifiManager.ACTION_WIFI_SCAN_AVAILABILITY_CHANGED, intent.getAction());
        assertEquals(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT, intent.getFlags());
        boolean scanAvailable = intent.getBooleanExtra(WifiManager.EXTRA_SCAN_AVAILABLE, false);
        assertEquals(expectedScanAvailable, scanAvailable);
    }

    /**
     * Test register two different scan result Callback, all of them will receive the event.
     */
    @Test
    public void testScanSuccessWithMultipleCallback() throws Exception {
        mScanRequestProxy.registerScanResultsCallback(mScanResultsCallback);
        mScanRequestProxy.registerScanResultsCallback(mAnotherScanResultsCallback);
        testStartScanSuccess();
        // Verify the scan results processing.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas1);
        mLooper.dispatchAll();
        validateScanResultsAvailableBroadcastSent(true);

        mScanRequestProxy.unregisterScanResultsCallback(mScanResultsCallback);
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas2);
        mLooper.dispatchAll();
        validateScanResultsAvailableBroadcastSent(true);
        verify(mScanResultsCallback).onScanResultsAvailable();
        verify(mAnotherScanResultsCallback, times(2)).onScanResultsAvailable();
    }

    /**
     * Verify that registering twice with same Callback will replace the first Callback.
     */
    @Test
    public void testReplacesOldListenerWithNewCallbackWhenRegisteringTwice() throws Exception {
        mScanRequestProxy.registerScanResultsCallback(mScanResultsCallback);
        mScanRequestProxy.registerScanResultsCallback(mScanResultsCallback);
        mLooper.dispatchAll();
        // Verify old listener is replaced.
        verify(mBinder, times(2)).linkToDeath(any(), anyInt());
        verify(mBinder).unlinkToDeath(any(), anyInt());
        testStartScanSuccess();
        // Verify the scan results processing.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas1);
        mLooper.dispatchAll();
        verify(mScanResultsCallback).onScanResultsAvailable();
        validateScanResultsAvailableBroadcastSent(true);
    }

    /**
     * Test registered scan result Callback will be unregistered when calling binder is died.
     */
    @Test
    public void testUnregisterScanResultCallbackOnBinderDied() throws Exception {
        ArgumentCaptor<IBinder.DeathRecipient> drCaptor =
                ArgumentCaptor.forClass(IBinder.DeathRecipient.class);
        mScanRequestProxy.registerScanResultsCallback(mScanResultsCallback);
        verify(mBinder).linkToDeath(drCaptor.capture(), anyInt());
        testStartScanSuccess();
        // Verify the scan results processing.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas1);
        mLooper.dispatchAll();
        validateScanResultsAvailableBroadcastSent(true);
        verify(mScanResultsCallback).onScanResultsAvailable();
        drCaptor.getValue().binderDied();
        mLooper.dispatchAll();
        reset(mScanResultsCallback);
        // Verify the scan results processing.
        mGlobalScanListenerArgumentCaptor.getValue().onResults(mTestScanDatas1);
        mLooper.dispatchAll();
        validateScanResultsAvailableBroadcastSent(true);
        verify(mScanResultsCallback, never()).onScanResultsAvailable();
    }

    /** Test that modifying the returned scan results list does not change the original. */
    @Test
    public void testGetScanResults_modifyReturnedList_doesNotChangeOriginal() {
        // initialize scan results
        testStartScanSuccess();

        List<ScanResult> scanResults = mScanRequestProxy.getScanResults();
        int scanResultsOriginalSize = scanResults.size();

        scanResults.add(new ScanResult());

        assertThat(mScanRequestProxy.getScanResults()).hasSize(scanResultsOriginalSize);
    }
}
