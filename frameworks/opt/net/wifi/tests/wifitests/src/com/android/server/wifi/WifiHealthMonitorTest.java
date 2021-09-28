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

import static com.android.server.wifi.DeviceConfigFacade.DEFAULT_HEALTH_MONITOR_MIN_NUM_CONNECTION_ATTEMPT;
import static com.android.server.wifi.WifiScoreCard.TS_NONE;
import static com.android.server.wifi.util.NativeUtil.hexStringFromByteArray;

import static org.junit.Assert.*;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.*;
import static org.mockito.Mockito.when;

import android.app.test.MockAnswerUtil.AnswerWithArguments;
import android.app.test.TestAlarmManager;
import android.content.Context;
import android.content.pm.ModuleInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.MacAddress;
import android.net.wifi.ScanResult.InformationElement;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiScanner;
import android.net.wifi.WifiScanner.ScanData;
import android.net.wifi.WifiScanner.ScanListener;
import android.net.wifi.WifiScanner.ScanSettings;
import android.net.wifi.WifiSsid;
import android.os.Build;
import android.os.Handler;
import android.os.test.TestLooper;

import androidx.test.filters.SmallTest;

import com.android.dx.mockito.inline.extended.ExtendedMockito;
import com.android.server.wifi.WifiConfigManager.OnNetworkUpdateListener;
import com.android.server.wifi.WifiHealthMonitor.ScanStats;
import com.android.server.wifi.WifiHealthMonitor.WifiSoftwareBuildInfo;
import com.android.server.wifi.WifiHealthMonitor.WifiSystemInfoStats;
import com.android.server.wifi.WifiScoreCard.PerNetwork;
import com.android.server.wifi.proto.WifiScoreCardProto.SystemInfoStats;
import com.android.server.wifi.proto.WifiStatsLog;
import com.android.server.wifi.proto.nano.WifiMetricsProto.HealthMonitorMetrics;


import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.MockitoSession;
import org.mockito.quality.Strictness;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.List;

/**
 * Unit tests for {@link com.android.server.wifi.WifiHealthMonitor}.
 */
@SmallTest
public class WifiHealthMonitorTest extends WifiBaseTest {

    static final WifiSsid TEST_SSID_1 = WifiSsid.createFromAsciiEncoded("Joe's Place");
    static final WifiSsid TEST_SSID_2 = WifiSsid.createFromAsciiEncoded("Poe's Place");
    static final MacAddress TEST_BSSID_1 = MacAddress.fromString("aa:bb:cc:dd:ee:ff");
    private static final long CURRENT_ELAPSED_TIME_MS = 1000;

    private WifiScoreCard mWifiScoreCard;
    private WifiHealthMonitor mWifiHealthMonitor;
    private MockitoSession mSession;

    @Mock
    Clock mClock;
    @Mock
    WifiScoreCard.MemoryStore mMemoryStore;
    @Mock
    WifiInjector mWifiInjector;
    @Mock
    Context mContext;
    @Mock
    DeviceConfigFacade mDeviceConfigFacade;
    @Mock
    WifiNative mWifiNative;
    @Mock
    PackageManager mPackageManager;
    @Mock
    PackageInfo mPackageInfo;
    @Mock
    ModuleInfo mModuleInfo;

    private final ArrayList<String> mKeys = new ArrayList<>();
    private final ArrayList<WifiScoreCard.BlobListener> mBlobListeners = new ArrayList<>();
    private final ArrayList<byte[]> mBlobs = new ArrayList<>();

    private ScanSettings mScanSettings = new ScanSettings();
    private WifiConfigManager mWifiConfigManager;
    private long mMilliSecondsSinceBoot;
    private ExtendedWifiInfo mWifiInfo;
    private WifiConfiguration mWifiConfig;
    private String mDriverVersion;
    private String mFirmwareVersion;
    private static final long MODULE_VERSION = 1L;
    private TestAlarmManager mAlarmManager;
    private TestLooper mLooper = new TestLooper();
    private List<WifiConfiguration> mConfiguredNetworks;
    private WifiScanner mWifiScanner;
    private ScanData mScanData;
    private ScanListener mScanListener;
    private OnNetworkUpdateListener mOnNetworkUpdateListener;

    private void millisecondsPass(long ms) {
        mMilliSecondsSinceBoot += ms;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(mMilliSecondsSinceBoot);
        when(mClock.getWallClockMillis()).thenReturn(mMilliSecondsSinceBoot + 1_500_000_000_000L);
    }

    /**
     * Sets up for unit test.
     */
    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mKeys.clear();
        mBlobListeners.clear();
        mBlobs.clear();
        mConfiguredNetworks = new ArrayList<>();
        mMilliSecondsSinceBoot = 0;
        mWifiInfo = new ExtendedWifiInfo(mock(Context.class));
        mWifiInfo.setBSSID(TEST_BSSID_1.toString());
        mWifiInfo.setSSID(TEST_SSID_1);
        // Add 1st configuration
        mWifiConfig = new WifiConfiguration();
        mWifiConfig.SSID = mWifiInfo.getSSID();
        mConfiguredNetworks.add(mWifiConfig);
        // Add 2nd configuration
        mWifiInfo.setSSID(TEST_SSID_2);
        mWifiConfig = new WifiConfiguration();
        mWifiConfig.SSID = mWifiInfo.getSSID();
        mConfiguredNetworks.add(mWifiConfig);

        millisecondsPass(0);

        mDriverVersion = "build 1.1";
        mFirmwareVersion = "HW 1.1";
        when(mPackageInfo.getLongVersionCode()).thenReturn(MODULE_VERSION);
        when(mPackageManager.getPackageInfo(anyString(), anyInt())).thenReturn(mPackageInfo);
        when(mPackageManager.getModuleInfo(anyString(), anyInt())).thenReturn(mModuleInfo);
        when(mModuleInfo.getPackageName()).thenReturn("WifiAPK");
        when(mContext.getPackageManager()).thenReturn(mPackageManager);

        mWifiConfigManager = mockConfigManager();

        mWifiScoreCard = new WifiScoreCard(mClock, "some seed", mDeviceConfigFacade);
        mAlarmManager = new TestAlarmManager();
        when(mContext.getSystemService(Context.ALARM_SERVICE))
                .thenReturn(mAlarmManager.getAlarmManager());

        mScanData = mockScanData();
        mWifiScanner = mockWifiScanner(WifiScanner.WIFI_BAND_ALL);
        when(mWifiInjector.getWifiScanner()).thenReturn(mWifiScanner);
        when(mWifiNative.getDriverVersion()).thenReturn(mDriverVersion);
        when(mWifiNative.getFirmwareVersion()).thenReturn(mFirmwareVersion);
        when(mDeviceConfigFacade.getConnectionFailureHighThrPercent()).thenReturn(
                DeviceConfigFacade.DEFAULT_CONNECTION_FAILURE_HIGH_THR_PERCENT);
        when(mDeviceConfigFacade.getConnectionFailureCountMin()).thenReturn(
                DeviceConfigFacade.DEFAULT_CONNECTION_FAILURE_COUNT_MIN);
        when(mDeviceConfigFacade.getAssocRejectionHighThrPercent()).thenReturn(
                DeviceConfigFacade.DEFAULT_ASSOC_REJECTION_HIGH_THR_PERCENT);
        when(mDeviceConfigFacade.getAssocRejectionCountMin()).thenReturn(
                DeviceConfigFacade.DEFAULT_ASSOC_REJECTION_COUNT_MIN);
        when(mDeviceConfigFacade.getAssocTimeoutHighThrPercent()).thenReturn(
                DeviceConfigFacade.DEFAULT_ASSOC_TIMEOUT_HIGH_THR_PERCENT);
        when(mDeviceConfigFacade.getAssocTimeoutCountMin()).thenReturn(
                DeviceConfigFacade.DEFAULT_ASSOC_TIMEOUT_COUNT_MIN);
        when(mDeviceConfigFacade.getAuthFailureHighThrPercent()).thenReturn(
                DeviceConfigFacade.DEFAULT_AUTH_FAILURE_HIGH_THR_PERCENT);
        when(mDeviceConfigFacade.getAuthFailureCountMin()).thenReturn(
                DeviceConfigFacade.DEFAULT_AUTH_FAILURE_COUNT_MIN);
        when(mDeviceConfigFacade.getShortConnectionNonlocalHighThrPercent()).thenReturn(
                DeviceConfigFacade.DEFAULT_SHORT_CONNECTION_NONLOCAL_HIGH_THR_PERCENT);
        when(mDeviceConfigFacade.getShortConnectionNonlocalCountMin()).thenReturn(
                DeviceConfigFacade.DEFAULT_SHORT_CONNECTION_NONLOCAL_COUNT_MIN);
        when(mDeviceConfigFacade.getDisconnectionNonlocalHighThrPercent()).thenReturn(
                DeviceConfigFacade.DEFAULT_DISCONNECTION_NONLOCAL_HIGH_THR_PERCENT);
        when(mDeviceConfigFacade.getDisconnectionNonlocalCountMin()).thenReturn(
                DeviceConfigFacade.DEFAULT_DISCONNECTION_NONLOCAL_COUNT_MIN);
        when(mDeviceConfigFacade.getHealthMonitorMinRssiThrDbm()).thenReturn(
                DeviceConfigFacade.DEFAULT_HEALTH_MONITOR_MIN_RSSI_THR_DBM);
        when(mDeviceConfigFacade.getHealthMonitorRatioThrNumerator()).thenReturn(
                DeviceConfigFacade.DEFAULT_HEALTH_MONITOR_RATIO_THR_NUMERATOR);
        when(mDeviceConfigFacade.getHealthMonitorMinNumConnectionAttempt()).thenReturn(
                DeviceConfigFacade.DEFAULT_HEALTH_MONITOR_MIN_NUM_CONNECTION_ATTEMPT);
        when(mDeviceConfigFacade.getHealthMonitorShortConnectionDurationThrMs()).thenReturn(
                DeviceConfigFacade.DEFAULT_HEALTH_MONITOR_SHORT_CONNECTION_DURATION_THR_MS);
        when(mDeviceConfigFacade.getAbnormalDisconnectionReasonCodeMask()).thenReturn(
                DeviceConfigFacade.DEFAULT_ABNORMAL_DISCONNECTION_REASON_CODE_MASK);
        when(mDeviceConfigFacade.getHealthMonitorRssiPollValidTimeMs()).thenReturn(
                DeviceConfigFacade.DEFAULT_HEALTH_MONITOR_RSSI_POLL_VALID_TIME_MS);
        when(mDeviceConfigFacade.getHealthMonitorFwAlertValidTimeMs()).thenReturn(
                DeviceConfigFacade.DEFAULT_HEALTH_MONITOR_FW_ALERT_VALID_TIME_MS);
        when(mDeviceConfigFacade.getNonstationaryScanRssiValidTimeMs()).thenReturn(
                DeviceConfigFacade.DEFAULT_NONSTATIONARY_SCAN_RSSI_VALID_TIME_MS);
        when(mDeviceConfigFacade.getStationaryScanRssiValidTimeMs()).thenReturn(
                DeviceConfigFacade.DEFAULT_STATIONARY_SCAN_RSSI_VALID_TIME_MS);
        mWifiHealthMonitor = new WifiHealthMonitor(mContext, mWifiInjector, mClock,
                mWifiConfigManager, mWifiScoreCard, new Handler(mLooper.getLooper()), mWifiNative,
                "some seed", mDeviceConfigFacade);
    }

    private WifiConfigManager mockConfigManager() {
        WifiConfigManager wifiConfigManager = mock(WifiConfigManager.class);
        when(wifiConfigManager.getConfiguredNetworks()).thenReturn(mConfiguredNetworks);
        when(wifiConfigManager.findScanRssi(anyInt(), anyInt()))
                .thenReturn(-53);

        doAnswer(new AnswerWithArguments() {
            public void answer(OnNetworkUpdateListener listener) throws Exception {
                mOnNetworkUpdateListener = listener;
            }
        }).when(wifiConfigManager).addOnNetworkUpdateListener(anyObject());

        doAnswer(new AnswerWithArguments() {
            public boolean answer(int networkId, int uid, String packageName) throws Exception {
                mOnNetworkUpdateListener.onNetworkRemoved(mWifiConfig);
                return true;
            }
        }).when(wifiConfigManager).removeNetwork(anyInt(), anyInt(), anyString());

        doAnswer(new AnswerWithArguments() {
            public NetworkUpdateResult answer(WifiConfiguration config, int uid) throws Exception {
                mOnNetworkUpdateListener.onNetworkAdded(config);
                return new NetworkUpdateResult(1);
            }
        }).when(wifiConfigManager).addOrUpdateNetwork(any(), anyInt());

        return wifiConfigManager;
    }

    ScanData mockScanData() {
        ScanData[] scanDatas =
                ScanTestUtil.createScanDatas(new int[][]{{5150, 5175, 2412, 2437}}, new int[]{0});
        // Scan result does require to have an IE.
        scanDatas[0].getResults()[0].informationElements = new InformationElement[0];
        scanDatas[0].getResults()[1].informationElements = new InformationElement[0];
        scanDatas[0].getResults()[2].informationElements = new InformationElement[0];
        scanDatas[0].getResults()[3].informationElements = new InformationElement[0];

        return scanDatas[0];
    }

    ScanData mockScanDataAbove2GOnly() {
        ScanData[] scanDatas =
                ScanTestUtil.createScanDatas(new int[][]{{5150, 5175, 5500, 5845}}, new int[]{0});
        // Scan result does require to have an IE.
        scanDatas[0].getResults()[0].informationElements = new InformationElement[0];
        scanDatas[0].getResults()[1].informationElements = new InformationElement[0];
        scanDatas[0].getResults()[2].informationElements = new InformationElement[0];
        scanDatas[0].getResults()[3].informationElements = new InformationElement[0];

        return scanDatas[0];
    }

    WifiScanner mockWifiScanner(@WifiScanner.WifiBand int wifiBand) {
        WifiScanner scanner = mock(WifiScanner.class);

        doAnswer(new AnswerWithArguments() {
            public void answer(ScanListener listener) throws Exception {
                mScanListener = listener;
            }
        }).when(scanner).registerScanListener(anyObject());

        ScanData[] scanDatas = new ScanData[1];
        scanDatas[0] = mock(ScanData.class);
        when(scanDatas[0].getBandScanned()).thenReturn(wifiBand);
        doAnswer(new AnswerWithArguments() {
            public void answer(ScanSettings settings, ScanListener listener) throws Exception {
                if (mScanData != null && mScanData.getResults() != null) {
                    for (int i = 0; i < mScanData.getResults().length; i++) {
                        listener.onFullResult(
                                mScanData.getResults()[i]);
                    }
                }
                listener.onResults(scanDatas);
            }
        }).when(scanner).startScan(anyObject(), anyObject());

        return scanner;
    }


    private void makeNetworkConnectionExample() {
        mWifiScoreCard.noteConnectionAttempt(mWifiInfo, -53, mWifiInfo.getSSID());
        millisecondsPass(5000);
        mWifiInfo.setRssi(-55);
        mWifiScoreCard.noteValidationSuccess(mWifiInfo);
        millisecondsPass(1000);
        mWifiScoreCard.noteSignalPoll(mWifiInfo);
        millisecondsPass(2000);
        int disconnectionReason = 0;
        mWifiScoreCard.noteNonlocalDisconnect(disconnectionReason);
        millisecondsPass(10);
        mWifiScoreCard.resetConnectionState();
    }

    private void makeRecentStatsWithSufficientConnectionAttempt() {
        for (int i = 0; i < DEFAULT_HEALTH_MONITOR_MIN_NUM_CONNECTION_ATTEMPT; i++) {
            makeNetworkConnectionExample();
        }
    }

    private byte[] makeSerializedExample() {
        // Install a dummy memoryStore
        // trigger extractCurrentSoftwareBuildInfo() call to update currSoftwareBuildInfo
        mWifiHealthMonitor.installMemoryStoreSetUpDetectionAlarm(mMemoryStore);
        mWifiHealthMonitor.setWifiEnabled(true);
        assertEquals(MODULE_VERSION, mWifiHealthMonitor.getWifiStackVersion());
        millisecondsPass(5000);
        mWifiScanner.startScan(mScanSettings, mScanListener);
        mAlarmManager.dispatch(WifiHealthMonitor.POST_BOOT_DETECTION_TIMER_TAG);
        mLooper.dispatchAll();
        // serialized now has currSoftwareBuildInfo and scan results
        return mWifiHealthMonitor.getWifiSystemInfoStats().toSystemInfoStats().toByteArray();
    }

    private void makeSwBuildChangeExample(String firmwareVersion) {
        byte[] serialized = makeSerializedExample();
        // Install a real MemoryStore object, which records read requests
        mWifiHealthMonitor.installMemoryStoreSetUpDetectionAlarm(new WifiScoreCard.MemoryStore() {
            @Override
            public void read(String key, String name, WifiScoreCard.BlobListener listener) {
                mBlobListeners.add(listener);
            }

            @Override
            public void write(String key, String name, byte[] value) {
                mKeys.add(key);
                mBlobs.add(value);
            }

            @Override
            public void setCluster(String key, String cluster) {
            }

            @Override
            public void removeCluster(String cluster) {
            }
        });
        mBlobListeners.get(0).onBlobRetrieved(serialized);

        // Change current FW version
        when(mWifiNative.getFirmwareVersion()).thenReturn(firmwareVersion);
    }

    /**
     * Test read and write around SW change.
     */
    @Test
    public void testReadWriteAndSWChange() throws Exception {
        String firmwareVersion = "HW 1.2";
        makeSwBuildChangeExample(firmwareVersion);
        mAlarmManager.dispatch(WifiHealthMonitor.POST_BOOT_DETECTION_TIMER_TAG);
        mLooper.dispatchAll();
        // Now it should detect SW change, disable WiFi to trigger write
        mWifiHealthMonitor.setWifiEnabled(false);

        // Check current and previous FW version of WifiSystemInfoStats
        WifiSystemInfoStats wifiSystemInfoStats = mWifiHealthMonitor.getWifiSystemInfoStats();
        assertEquals(firmwareVersion, wifiSystemInfoStats.getCurrSoftwareBuildInfo()
                .getWifiFirmwareVersion());
        assertEquals(mFirmwareVersion, wifiSystemInfoStats.getPrevSoftwareBuildInfo()
                .getWifiFirmwareVersion());
        assertEquals(MODULE_VERSION, mWifiHealthMonitor.getWifiStackVersion());

        // Check write
        String writtenHex = hexStringFromByteArray(mBlobs.get(mKeys.size() - 1));
        String currFirmwareVersionHex = hexStringFromByteArray(
                firmwareVersion.getBytes(StandardCharsets.UTF_8));
        String prevFirmwareVersionHex = hexStringFromByteArray(
                mFirmwareVersion.getBytes(StandardCharsets.UTF_8));
        assertTrue(writtenHex, writtenHex.contains(currFirmwareVersionHex));
        assertTrue(writtenHex, writtenHex.contains(prevFirmwareVersionHex));
    }

    /**
     * Test serialization and deserialization of WifiSystemInfoStats.
     */
    @Test
    public void testSerializationDeserialization() throws Exception  {
        // Install a dummy memoryStore
        // trigger extractCurrentSoftwareBuildInfo() call to update currSoftwareBuildInfo
        mWifiHealthMonitor.installMemoryStoreSetUpDetectionAlarm(mMemoryStore);
        mWifiHealthMonitor.setWifiEnabled(true);
        millisecondsPass(5000);
        mWifiScanner.startScan(mScanSettings, mScanListener);
        mAlarmManager.dispatch(WifiHealthMonitor.POST_BOOT_DETECTION_TIMER_TAG);
        mLooper.dispatchAll();
        WifiSystemInfoStats wifiSystemInfoStats = mWifiHealthMonitor.getWifiSystemInfoStats();
        // serialized now has currSoftwareBuildInfo and recent scan info
        byte[] serialized = wifiSystemInfoStats.toSystemInfoStats().toByteArray();
        SystemInfoStats systemInfoStats = SystemInfoStats.parseFrom(serialized);
        WifiSoftwareBuildInfo currSoftwareBuildInfoFromMemory = wifiSystemInfoStats
                .fromSoftwareBuildInfo(systemInfoStats.getCurrSoftwareBuildInfo());
        assertEquals(MODULE_VERSION, currSoftwareBuildInfoFromMemory.getWifiStackVersion());
        assertEquals(mDriverVersion, currSoftwareBuildInfoFromMemory.getWifiDriverVersion());
        assertEquals(mFirmwareVersion, currSoftwareBuildInfoFromMemory.getWifiFirmwareVersion());
        assertEquals(Build.DISPLAY, currSoftwareBuildInfoFromMemory.getOsBuildVersion());
        assertEquals(1_500_000_005_000L, systemInfoStats.getLastScanTimeMs());
        assertEquals(2, systemInfoStats.getNumBssidLastScan2G());
        assertEquals(2, systemInfoStats.getNumBssidLastScanAbove2G());
    }

    /**
     * Check alarm timing of a multi-day run.
     */
    @Test
    public void testTimerMultiDayRun() throws Exception {
        long currentWallClockTimeMs = 23 * 3600_000;
        long currentElapsedTimeMs = CURRENT_ELAPSED_TIME_MS;
        Calendar calendar = Calendar.getInstance();
        calendar.setTimeInMillis(currentWallClockTimeMs);
        int expectedWaitHours = WifiHealthMonitor.DAILY_DETECTION_HOUR
                - calendar.get(Calendar.HOUR_OF_DAY);
        if (expectedWaitHours <= 0) expectedWaitHours += 24;

        // day 1
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentElapsedTimeMs);
        when(mClock.getWallClockMillis()).thenReturn(currentWallClockTimeMs);
        mWifiHealthMonitor.installMemoryStoreSetUpDetectionAlarm(mMemoryStore);
        long waitTimeMs = mAlarmManager
                .getTriggerTimeMillis(WifiHealthMonitor.DAILY_DETECTION_TIMER_TAG)
                - currentElapsedTimeMs;
        assertEquals(expectedWaitHours * 3600_000, waitTimeMs);
        currentElapsedTimeMs += 24 * 3600_000 + 1;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentElapsedTimeMs);
        mAlarmManager.dispatch(WifiHealthMonitor.DAILY_DETECTION_TIMER_TAG);
        mLooper.dispatchAll();
        waitTimeMs = mAlarmManager
                .getTriggerTimeMillis(WifiHealthMonitor.DAILY_DETECTION_TIMER_TAG)
                - currentElapsedTimeMs;
        assertEquals(24 * 3600_000, waitTimeMs);
        // day 2
        currentElapsedTimeMs += 24 * 3600_000 - 1;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentElapsedTimeMs);
        mAlarmManager.dispatch(WifiHealthMonitor.DAILY_DETECTION_TIMER_TAG);
        mLooper.dispatchAll();
        waitTimeMs = mAlarmManager
                .getTriggerTimeMillis(WifiHealthMonitor.DAILY_DETECTION_TIMER_TAG)
                - currentElapsedTimeMs;
        assertEquals(24 * 3600_000, waitTimeMs);
    }

    /**
     * Check the alarm timing with a different wall clock time
     */
    @Test
    public void testTimerWith() throws Exception {
        long currentWallClockTimeMs = 7 * 3600_000;
        long currentElapsedTimeMs = CURRENT_ELAPSED_TIME_MS;
        Calendar calendar = Calendar.getInstance();
        calendar.setTimeInMillis(currentWallClockTimeMs);
        int expectedWaitHours = WifiHealthMonitor.DAILY_DETECTION_HOUR
                - calendar.get(Calendar.HOUR_OF_DAY);
        if (expectedWaitHours <= 0) expectedWaitHours += 24;

        // day 1
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentElapsedTimeMs);
        when(mClock.getWallClockMillis()).thenReturn(currentWallClockTimeMs);
        mWifiHealthMonitor.installMemoryStoreSetUpDetectionAlarm(mMemoryStore);
        long waitTimeMs = mAlarmManager
                .getTriggerTimeMillis(WifiHealthMonitor.DAILY_DETECTION_TIMER_TAG)
                - currentElapsedTimeMs;
        assertEquals(expectedWaitHours * 3600_000, waitTimeMs);
    }

    /**
     * Check stats with two daily detections.
     */
    @Test
    public void testTwoDailyDetections() throws Exception {
        mWifiHealthMonitor.installMemoryStoreSetUpDetectionAlarm(mMemoryStore);
        // day 1
        makeRecentStatsWithSufficientConnectionAttempt();
        mAlarmManager.dispatch(WifiHealthMonitor.DAILY_DETECTION_TIMER_TAG);
        mLooper.dispatchAll();
        // day 2
        makeRecentStatsWithSufficientConnectionAttempt();
        mAlarmManager.dispatch(WifiHealthMonitor.DAILY_DETECTION_TIMER_TAG);
        mLooper.dispatchAll();

        PerNetwork perNetwork = mWifiScoreCard.fetchByNetwork(mWifiInfo.getSSID());
        assertEquals(DEFAULT_HEALTH_MONITOR_MIN_NUM_CONNECTION_ATTEMPT * 2,
                perNetwork.getStatsCurrBuild().getCount(WifiScoreCard.CNT_CONNECTION_ATTEMPT));
    }

    /**
     * Check proto after one daily detection with high non-local disconnection rate
     */
    @Test
    public void testBuildProto() throws Exception {
        mWifiHealthMonitor.installMemoryStoreSetUpDetectionAlarm(mMemoryStore);
        makeRecentStatsWithSufficientConnectionAttempt();
        mAlarmManager.dispatch(WifiHealthMonitor.DAILY_DETECTION_TIMER_TAG);
        mLooper.dispatchAll();

        // First call of buildProto
        HealthMonitorMetrics healthMetrics = mWifiHealthMonitor.buildProto();
        assertEquals(0, healthMetrics.failureStatsIncrease.cntAssocRejection);
        assertEquals(0, healthMetrics.failureStatsIncrease.cntAssocTimeout);
        assertEquals(0, healthMetrics.failureStatsIncrease.cntAuthFailure);
        assertEquals(0, healthMetrics.failureStatsIncrease.cntConnectionFailure);
        assertEquals(0, healthMetrics.failureStatsIncrease.cntDisconnectionNonlocal);
        assertEquals(0, healthMetrics.failureStatsIncrease.cntShortConnectionNonlocal);
        assertEquals(0, healthMetrics.failureStatsHigh.cntAssocRejection);
        assertEquals(0, healthMetrics.failureStatsHigh.cntAssocTimeout);
        assertEquals(0, healthMetrics.failureStatsHigh.cntAuthFailure);
        assertEquals(0, healthMetrics.failureStatsHigh.cntConnectionFailure);
        assertEquals(1, healthMetrics.failureStatsHigh.cntDisconnectionNonlocal);
        assertEquals(1, healthMetrics.failureStatsHigh.cntShortConnectionNonlocal);
        assertEquals(1, healthMetrics.numNetworkSufficientRecentStatsOnly);
        assertEquals(0, healthMetrics.numNetworkSufficientRecentPrevStats);

        // Second call of buildProto
        healthMetrics = mWifiHealthMonitor.buildProto();
        // Second call should result in an empty proto
        assertEquals(null, healthMetrics);
    }

    /**
     * Test FailureStats class
     */
    @Test
    public void testFailureStats() throws Exception {
        WifiHealthMonitor.FailureStats failureStats = new WifiHealthMonitor.FailureStats();
        failureStats.setCount(WifiHealthMonitor.REASON_AUTH_FAILURE, 10);
        failureStats.incrementCount(WifiHealthMonitor.REASON_AUTH_FAILURE);

        String expectedString = "authentication failure: 11 ";
        String unexpectedString =
                WifiHealthMonitor.FAILURE_REASON_NAME[WifiHealthMonitor.REASON_ASSOC_REJECTION];
        assertEquals(11, failureStats.getCount(WifiHealthMonitor.REASON_AUTH_FAILURE));
        assertEquals(true, failureStats.toString().contains(expectedString));
        assertEquals(false, failureStats.toString().contains(unexpectedString));
    }

    /**
     * Check statsd logging after one daily detection with high non-local disconnection rate
     */
    @Test
    public void testWifiStatsLogWrite() throws Exception {
        // static mocking for WifiStatsLog
        mSession = ExtendedMockito.mockitoSession()
                .strictness(Strictness.LENIENT)
                .mockStatic(WifiStatsLog.class)
                .startMocking();

        mWifiHealthMonitor.installMemoryStoreSetUpDetectionAlarm(mMemoryStore);
        makeRecentStatsWithSufficientConnectionAttempt();
        mAlarmManager.dispatch(WifiHealthMonitor.DAILY_DETECTION_TIMER_TAG);
        mLooper.dispatchAll();

        ExtendedMockito.verify(() -> WifiStatsLog.write(
                WifiStatsLog.WIFI_FAILURE_STAT_REPORTED,
                WifiStatsLog.WIFI_FAILURE_STAT_REPORTED__ABNORMALITY_TYPE__SIMPLY_HIGH,
                WifiStatsLog.WIFI_FAILURE_STAT_REPORTED__FAILURE_TYPE__FAILURE_NON_LOCAL_DISCONNECTION,
                1));

        ExtendedMockito.verify(() -> WifiStatsLog.write(
                WifiStatsLog.WIFI_FAILURE_STAT_REPORTED,
                WifiStatsLog.WIFI_FAILURE_STAT_REPORTED__ABNORMALITY_TYPE__SIMPLY_HIGH,
                WifiStatsLog.WIFI_FAILURE_STAT_REPORTED__FAILURE_TYPE__FAILURE_SHORT_CONNECTION_DUE_TO_NON_LOCAL_DISCONNECTION,
                1));
        mSession.finishMocking();
    }

    /**
     * test stats after a SW build change
     */
    @Test
    public void testAfterSwBuildChange() throws Exception {
        // Day 1
        mWifiHealthMonitor.installMemoryStoreSetUpDetectionAlarm(mMemoryStore);
        makeRecentStatsWithSufficientConnectionAttempt();
        mAlarmManager.dispatch(WifiHealthMonitor.DAILY_DETECTION_TIMER_TAG);
        mLooper.dispatchAll();

        // Day 2
        String firmwareVersion = "HW 1.2";
        makeSwBuildChangeExample(firmwareVersion);
        // Disable WiFi before post-boot-detection
        mWifiHealthMonitor.setWifiEnabled(false);
        mAlarmManager.dispatch(WifiHealthMonitor.POST_BOOT_DETECTION_TIMER_TAG);
        mLooper.dispatchAll();
        // Skip SW build change detection
        PerNetwork perNetwork = mWifiScoreCard.fetchByNetwork(mWifiInfo.getSSID());
        assertEquals(DEFAULT_HEALTH_MONITOR_MIN_NUM_CONNECTION_ATTEMPT * 1,
                perNetwork.getStatsCurrBuild().getCount(WifiScoreCard.CNT_CONNECTION_ATTEMPT));
        assertEquals(DEFAULT_HEALTH_MONITOR_MIN_NUM_CONNECTION_ATTEMPT * 0,
                perNetwork.getStatsPrevBuild().getCount(WifiScoreCard.CNT_CONNECTION_ATTEMPT));

        // Day 3
        mWifiHealthMonitor.setWifiEnabled(true);
        mAlarmManager.dispatch(WifiHealthMonitor.POST_BOOT_DETECTION_TIMER_TAG);
        mLooper.dispatchAll();
        // Finally detect SW build change
        assertEquals(0,
                perNetwork.getStatsCurrBuild().getCount(WifiScoreCard.CNT_CONNECTION_ATTEMPT));
        assertEquals(DEFAULT_HEALTH_MONITOR_MIN_NUM_CONNECTION_ATTEMPT * 1,
                perNetwork.getStatsPrevBuild().getCount(WifiScoreCard.CNT_CONNECTION_ATTEMPT));
    }

    /**
     * Installing a MemoryStore after startup should issue reads.
     */
    @Test
    public void testReadAfterDelayedMemoryStoreInstallation() throws Exception {
        makeNetworkConnectionExample();
        assertEquals(2, mConfiguredNetworks.size());
        mWifiScoreCard.installMemoryStore(mMemoryStore);
        mWifiHealthMonitor.installMemoryStoreSetUpDetectionAlarm(mMemoryStore);

        // 1 for WifiSystemInfoStats, 1 for requestReadBssid and 2 for requestReadNetwork
        verify(mMemoryStore, times(4)).read(any(), any(), any());
    }

    /**
     * Installing a MemoryStore during startup should issue a proper number of reads.
     */
    @Test
    public void testReadAfterStartupMemoryStoreInstallation() throws Exception {
        mWifiScoreCard.installMemoryStore(mMemoryStore);
        mWifiHealthMonitor.installMemoryStoreSetUpDetectionAlarm(mMemoryStore);
        makeNetworkConnectionExample();
        assertEquals(2, mConfiguredNetworks.size());

        // 1 for WifiSystemInfoStats, 1 for requestReadBssid and 2 for requestReadNetwork
        verify(mMemoryStore, times(4)).read(any(), any(), any());
    }

    /**
     * Installing a MemoryStore twice should not cause crash.
     */
    @Test
    public void testInstallMemoryStoreTwiceNoCrash() throws Exception {
        mWifiHealthMonitor.installMemoryStoreSetUpDetectionAlarm(mMemoryStore);
        makeNetworkConnectionExample();
        mWifiHealthMonitor.installMemoryStoreSetUpDetectionAlarm(mMemoryStore);
    }

    /**
     * Check if scan results are reported correctly after full band scan.
     */
    @Test
    public void testFullBandScan() throws Exception {
        millisecondsPass(5000);
        mWifiHealthMonitor.setWifiEnabled(true);
        mWifiScanner.startScan(mScanSettings, mScanListener);
        ScanStats scanStats = mWifiHealthMonitor.getWifiSystemInfoStats().getCurrScanStats();
        assertEquals(1_500_000_005_000L, scanStats.getLastScanTimeMs());
        assertEquals(2, scanStats.getNumBssidLastScanAbove2g());
        assertEquals(2, scanStats.getNumBssidLastScan2g());
    }

    /**
     * Check if scan results are reported correctly after 2G only scan.
     */
    @Test
    public void test2GScan() throws Exception {
        mWifiScanner = mockWifiScanner(WifiScanner.WIFI_BAND_24_GHZ);
        when(mWifiInjector.getWifiScanner()).thenReturn(mWifiScanner);
        millisecondsPass(5000);
        mWifiHealthMonitor.setWifiEnabled(true);
        mWifiScanner.startScan(mScanSettings, mScanListener);
        ScanStats scanStats = mWifiHealthMonitor.getWifiSystemInfoStats().getCurrScanStats();
        assertEquals(TS_NONE, scanStats.getLastScanTimeMs());
        assertEquals(0, scanStats.getNumBssidLastScanAbove2g());
        assertEquals(0, scanStats.getNumBssidLastScan2g());
    }

    @Test
    public void testClearReallyDoesClearTheState() throws Exception {
        byte[] init = mWifiHealthMonitor.getWifiSystemInfoStats()
                .toSystemInfoStats().toByteArray();
        byte[] serialized = makeSerializedExample();
        assertNotEquals(0, serialized.length);
        mWifiHealthMonitor.clear();
        byte[] leftovers = mWifiHealthMonitor.getWifiSystemInfoStats()
                .toSystemInfoStats().toByteArray();
        assertEquals(init.length, leftovers.length);
    }

    @Test
    public void testPostBootAbnormalScanDetection() throws Exception {
        // Serialized has the last scan result
        byte [] serialized = makeSerializedExample();
        // Startup DUT again to mimic reboot
        setUp();
        // Install a real MemoryStore object, which records read requests
        mWifiHealthMonitor.installMemoryStoreSetUpDetectionAlarm(new WifiScoreCard.MemoryStore() {
            @Override
            public void read(String key, String name, WifiScoreCard.BlobListener listener) {
                mBlobListeners.add(listener);
            }

            @Override
            public void write(String key, String name, byte[] value) {
                mKeys.add(key);
                mBlobs.add(value);
            }

            @Override
            public void setCluster(String key, String cluster) {
            }

            @Override
            public void removeCluster(String cluster) {
            }
        });
        mBlobListeners.get(0).onBlobRetrieved(serialized);

        SystemInfoStats systemInfoStats = SystemInfoStats.parseFrom(serialized);
        assertEquals(1_500_000_005_000L, systemInfoStats.getLastScanTimeMs());
        assertEquals(2, systemInfoStats.getNumBssidLastScan2G());
        assertEquals(2, systemInfoStats.getNumBssidLastScanAbove2G());

        // Add Above2G only scan data
        mScanData = mockScanDataAbove2GOnly();
        mWifiScanner = mockWifiScanner(WifiScanner.WIFI_BAND_ALL);
        when(mWifiInjector.getWifiScanner()).thenReturn(mWifiScanner);
        millisecondsPass(5000);
        mWifiHealthMonitor.setWifiEnabled(true);
        mWifiScanner.startScan(mScanSettings, mScanListener);

        mAlarmManager.dispatch(WifiHealthMonitor.POST_BOOT_DETECTION_TIMER_TAG);
        mLooper.dispatchAll();

        // It should detect abnormal scan failure now.
        assertEquals(4, mWifiHealthMonitor.getWifiSystemInfoStats().getScanFailure());
    }

    /**
     * Test when remove a saved network will remove network from the WifiScoreCard.
     */
    @Test
    public void testRemoveSavedNetwork() {
        makeNetworkConnectionExample();
        PerNetwork perNetwork = mWifiScoreCard.fetchByNetwork(mWifiInfo.getSSID());
        assertNotNull(perNetwork);

        // Now remove the network
        mWifiConfigManager.removeNetwork(1, 1, "some package");
        perNetwork = mWifiScoreCard.fetchByNetwork(mWifiInfo.getSSID());
        assertNull(perNetwork);
    }

    /**
     * Test when remove a suggestion network will not remove network from the WifiScoreCard.
     */
    @Test
    public void testRemoveSuggestionNetwork() throws Exception {
        mWifiConfig.fromWifiNetworkSuggestion = true;
        makeNetworkConnectionExample();
        PerNetwork perNetwork = mWifiScoreCard.fetchByNetwork(mWifiInfo.getSSID());
        assertNotNull(perNetwork);

        // Now remove the network
        mWifiConfigManager.removeNetwork(1, 1, "some package");
        perNetwork = mWifiScoreCard.fetchByNetwork(mWifiInfo.getSSID());
        assertNotNull(perNetwork);
    }

    @Test
    public void testAddNetwork() throws Exception {
        PerNetwork perNetwork = mWifiScoreCard.fetchByNetwork(mWifiInfo.getSSID());
        assertNull(perNetwork);

        // Now add network
        mWifiConfigManager.addOrUpdateNetwork(mWifiConfig, 1);
        perNetwork = mWifiScoreCard.fetchByNetwork(mWifiInfo.getSSID());
        assertNotNull(perNetwork);
    }
}
