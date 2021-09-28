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

import static com.android.server.wifi.ClientModeImpl.WIFI_WORK_SOURCE;
import static com.android.server.wifi.WifiConfigurationTestUtil.generateWifiConfig;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;
import static org.mockito.Mockito.argThat;

import android.app.test.MockAnswerUtil.AnswerWithArguments;
import android.app.test.TestAlarmManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.net.MacAddress;
import android.net.NetworkScoreManager;
import android.net.wifi.ScanResult;
import android.net.wifi.ScanResult.InformationElement;
import android.net.wifi.SupplicantState;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiNetworkSuggestion;
import android.net.wifi.WifiScanner;
import android.net.wifi.WifiScanner.PnoScanListener;
import android.net.wifi.WifiScanner.PnoSettings;
import android.net.wifi.WifiScanner.ScanData;
import android.net.wifi.WifiScanner.ScanListener;
import android.net.wifi.WifiScanner.ScanSettings;
import android.net.wifi.WifiSsid;
import android.net.wifi.hotspot2.PasspointConfiguration;
import android.os.Handler;
import android.os.Process;
import android.os.SystemClock;
import android.os.WorkSource;
import android.os.test.TestLooper;
import android.util.LocalLog;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.hotspot2.PasspointManager;
import com.android.server.wifi.util.LruConnectionTracker;
import com.android.server.wifi.util.ScanResultUtil;
import com.android.wifi.resources.R;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Executor;
import java.util.stream.Collectors;

/**
 * Unit tests for {@link com.android.server.wifi.WifiConnectivityManager}.
 */
@SmallTest
public class WifiConnectivityManagerTest extends WifiBaseTest {
    /**
     * Called before each test
     */
    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mResources = new MockResources();
        setUpResources(mResources);
        mAlarmManager = new TestAlarmManager();
        mContext = mockContext();
        mLocalLog = new LocalLog(512);
        mClientModeImpl = mockClientModeImpl();
        mWifiConfigManager = mockWifiConfigManager();
        mWifiInfo = getWifiInfo();
        mScanData = mockScanData();
        mWifiScanner = mockWifiScanner();
        mWifiConnectivityHelper = mockWifiConnectivityHelper();
        mWifiNS = mockWifiNetworkSelector();
        when(mWifiInjector.getWifiScanner()).thenReturn(mWifiScanner);
        when(mWifiNetworkSuggestionsManager.retrieveHiddenNetworkList())
                .thenReturn(new ArrayList<>());
        when(mWifiNetworkSuggestionsManager.getAllApprovedNetworkSuggestions())
                .thenReturn(new HashSet<>());
        when(mWifiInjector.getBssidBlocklistMonitor()).thenReturn(mBssidBlocklistMonitor);
        when(mWifiInjector.getWifiChannelUtilizationScan()).thenReturn(mWifiChannelUtilization);
        when(mWifiInjector.getWifiScoreCard()).thenReturn(mWifiScoreCard);
        when(mWifiInjector.getWifiNetworkSuggestionsManager())
                .thenReturn(mWifiNetworkSuggestionsManager);
        when(mWifiInjector.getPasspointManager()).thenReturn(mPasspointManager);
        when(mPasspointManager.getProviderConfigs(anyInt(), anyBoolean()))
                .thenReturn(new ArrayList<>());
        mWifiConnectivityManager = createConnectivityManager();
        verify(mWifiConfigManager).addOnNetworkUpdateListener(
                mNetworkUpdateListenerCaptor.capture());
        verify(mWifiNetworkSuggestionsManager).addOnSuggestionUpdateListener(
                mSuggestionUpdateListenerCaptor.capture());
        mWifiConnectivityManager.setTrustedConnectionAllowed(true);
        mWifiConnectivityManager.setWifiEnabled(true);
        when(mClock.getElapsedSinceBootMillis()).thenReturn(SystemClock.elapsedRealtime());
        mMinPacketRateActiveTraffic = mResources.getInteger(
                R.integer.config_wifiFrameworkMinPacketPerSecondActiveTraffic);
        when(mWifiLastResortWatchdog.shouldIgnoreBssidUpdate(anyString())).thenReturn(false);
        mLruConnectionTracker = new LruConnectionTracker(100, mContext);
        Comparator<WifiConfiguration> comparator =
                Comparator.comparingInt(mLruConnectionTracker::getAgeIndexOfNetwork);
        when(mWifiConfigManager.getScanListComparator()).thenReturn(comparator);
    }

    private void setUpResources(MockResources resources) {
        resources.setBoolean(
                R.bool.config_wifi_framework_enable_associated_network_selection, true);
        resources.setInteger(
                R.integer.config_wifi_framework_wifi_score_good_rssi_threshold_24GHz, -60);
        resources.setInteger(
                R.integer.config_wifiFrameworkMinPacketPerSecondActiveTraffic, 16);
        resources.setIntArray(
                R.array.config_wifiConnectedScanIntervalScheduleSec,
                VALID_CONNECTED_SINGLE_SCAN_SCHEDULE_SEC);
        resources.setIntArray(
                R.array.config_wifiDisconnectedScanIntervalScheduleSec,
                VALID_DISCONNECTED_SINGLE_SCAN_SCHEDULE_SEC);
        resources.setIntArray(
                R.array.config_wifiSingleSavedNetworkConnectedScanIntervalScheduleSec,
                SCHEDULE_EMPTY_SEC);
        resources.setInteger(
                R.integer.config_wifiHighMovementNetworkSelectionOptimizationScanDelayMs,
                HIGH_MVMT_SCAN_DELAY_MS);
        resources.setInteger(
                R.integer.config_wifiHighMovementNetworkSelectionOptimizationRssiDelta,
                HIGH_MVMT_RSSI_DELTA);
        resources.setInteger(R.integer.config_wifiInitialPartialScanChannelCacheAgeMins,
                CHANNEL_CACHE_AGE_MINS);
        resources.setInteger(R.integer.config_wifiMovingPnoScanIntervalMillis,
                MOVING_PNO_SCAN_INTERVAL_MILLIS);
        resources.setInteger(R.integer.config_wifiStationaryPnoScanIntervalMillis,
                STATIONARY_PNO_SCAN_INTERVAL_MILLIS);
    }

    /**
     * Called after each test
     */
    @After
    public void cleanup() {
        verify(mScoringParams, atLeast(0)).getEntryRssi(anyInt());
        verifyNoMoreInteractions(mScoringParams);
        validateMockitoUsage();
    }

    private Context mContext;
    private TestAlarmManager mAlarmManager;
    private TestLooper mLooper = new TestLooper();
    private WifiConnectivityManager mWifiConnectivityManager;
    private WifiNetworkSelector mWifiNS;
    private ClientModeImpl mClientModeImpl;
    private WifiScanner mWifiScanner;
    private WifiConnectivityHelper mWifiConnectivityHelper;
    private ScanData mScanData;
    private WifiConfigManager mWifiConfigManager;
    private WifiInfo mWifiInfo;
    private LocalLog mLocalLog;
    private LruConnectionTracker mLruConnectionTracker;
    @Mock private WifiInjector mWifiInjector;
    @Mock private NetworkScoreManager mNetworkScoreManager;
    @Mock private Clock mClock;
    @Mock private WifiLastResortWatchdog mWifiLastResortWatchdog;
    @Mock private OpenNetworkNotifier mOpenNetworkNotifier;
    @Mock private WifiMetrics mWifiMetrics;
    @Mock private WifiNetworkScoreCache mScoreCache;
    @Mock private WifiNetworkSuggestionsManager mWifiNetworkSuggestionsManager;
    @Mock private BssidBlocklistMonitor mBssidBlocklistMonitor;
    @Mock private WifiChannelUtilization mWifiChannelUtilization;
    @Mock private ScoringParams mScoringParams;
    @Mock private WifiScoreCard mWifiScoreCard;
    @Mock private PasspointManager mPasspointManager;
    @Mock private WifiScoreCard.PerNetwork mPerNetwork;
    @Mock private WifiScoreCard.PerNetwork mPerNetwork1;
    @Mock private PasspointConfiguration mPasspointConfiguration;
    @Mock private WifiConfiguration mSuggestionConfig;
    @Mock private WifiNetworkSuggestion mWifiNetworkSuggestion;
    @Mock WifiCandidates.Candidate mCandidate1;
    @Mock WifiCandidates.Candidate mCandidate2;
    private List<WifiCandidates.Candidate> mCandidateList;
    @Captor ArgumentCaptor<ScanResult> mCandidateScanResultCaptor;
    @Captor ArgumentCaptor<ArrayList<String>> mBssidBlacklistCaptor;
    @Captor ArgumentCaptor<ArrayList<String>> mSsidWhitelistCaptor;
    @Captor ArgumentCaptor<WifiConfigManager.OnNetworkUpdateListener>
            mNetworkUpdateListenerCaptor;
    @Captor ArgumentCaptor<WifiNetworkSuggestionsManager.OnSuggestionUpdateListener>
            mSuggestionUpdateListenerCaptor;
    private MockResources mResources;
    private int mMinPacketRateActiveTraffic;

    private static final int CANDIDATE_NETWORK_ID = 0;
    private static final String CANDIDATE_SSID = "\"AnSsid\"";
    private static final String CANDIDATE_BSSID = "6c:f3:7f:ae:8c:f3";
    private static final String CANDIDATE_BSSID_2 = "6c:f3:7f:ae:8d:f3";
    private static final String INVALID_SCAN_RESULT_BSSID = "6c:f3:7f:ae:8c:f4";
    private static final int TEST_FREQUENCY = 2420;
    private static final long CURRENT_SYSTEM_TIME_MS = 1000;
    private static final int MAX_BSSID_BLACKLIST_SIZE = 16;
    private static final int[] VALID_CONNECTED_SINGLE_SCAN_SCHEDULE_SEC = {10, 30, 50};
    private static final int[] VALID_CONNECTED_SINGLE_SAVED_NETWORK_SCHEDULE_SEC = {15, 35, 55};
    private static final int[] VALID_DISCONNECTED_SINGLE_SCAN_SCHEDULE_SEC = {25, 40, 60};
    private static final int[] SCHEDULE_EMPTY_SEC = {};
    private static final int[] INVALID_SCHEDULE_NEGATIVE_VALUES_SEC = {10, -10, 20};
    private static final int[] INVALID_SCHEDULE_ZERO_VALUES_SEC = {10, 0, 20};
    private static final int MAX_SCAN_INTERVAL_IN_SCHEDULE_SEC = 60;
    private static final int[] DEFAULT_SINGLE_SCAN_SCHEDULE_SEC = {20, 40, 80, 160};
    private static final int MAX_SCAN_INTERVAL_IN_DEFAULT_SCHEDULE_SEC = 160;
    private static final int TEST_FREQUENCY_1 = 2412;
    private static final int TEST_FREQUENCY_2 = 5180;
    private static final int TEST_FREQUENCY_3 = 5240;
    private static final int TEST_CURRENT_CONNECTED_FREQUENCY = 2427;
    private static final int HIGH_MVMT_SCAN_DELAY_MS = 10000;
    private static final int HIGH_MVMT_RSSI_DELTA = 10;
    private static final String TEST_FQDN = "FQDN";
    private static final String TEST_SSID = "SSID";
    private static final int TEMP_BSSID_BLOCK_DURATION_MS = 10 * 1000; // 10 seconds
    private static final int TEST_CONNECTED_NETWORK_ID = 55;
    private static final int CHANNEL_CACHE_AGE_MINS = 14400;
    private static final int MOVING_PNO_SCAN_INTERVAL_MILLIS = 20_000;
    private static final int STATIONARY_PNO_SCAN_INTERVAL_MILLIS = 60_000;

    Context mockContext() {
        Context context = mock(Context.class);

        when(context.getResources()).thenReturn(mResources);
        when(context.getSystemService(Context.ALARM_SERVICE)).thenReturn(
                mAlarmManager.getAlarmManager());
        when(context.getPackageManager()).thenReturn(mock(PackageManager.class));

        return context;
    }

    ScanData mockScanData() {
        ScanData scanData = mock(ScanData.class);

        when(scanData.getBandScanned()).thenReturn(WifiScanner.WIFI_BAND_ALL);

        return scanData;
    }

    WifiScanner mockWifiScanner() {
        WifiScanner scanner = mock(WifiScanner.class);
        ArgumentCaptor<ScanListener> allSingleScanListenerCaptor =
                ArgumentCaptor.forClass(ScanListener.class);

        doNothing().when(scanner).registerScanListener(
                any(), allSingleScanListenerCaptor.capture());

        ScanData[] scanDatas = new ScanData[1];
        scanDatas[0] = mScanData;

        // do a synchronous answer for the ScanListener callbacks
        doAnswer(new AnswerWithArguments() {
            public void answer(ScanSettings settings, ScanListener listener,
                    WorkSource workSource) throws Exception {
                listener.onResults(scanDatas);
            }}).when(scanner).startBackgroundScan(anyObject(), anyObject(), anyObject());

        doAnswer(new AnswerWithArguments() {
            public void answer(ScanSettings settings, Executor executor, ScanListener listener,
                    WorkSource workSource) throws Exception {
                listener.onResults(scanDatas);
                // WCM processes scan results received via onFullResult (even though they're the
                // same as onResult for single scans).
                if (mScanData != null && mScanData.getResults() != null) {
                    for (int i = 0; i < mScanData.getResults().length; i++) {
                        allSingleScanListenerCaptor.getValue().onFullResult(
                                mScanData.getResults()[i]);
                    }
                }
                allSingleScanListenerCaptor.getValue().onResults(scanDatas);
            }}).when(scanner).startScan(anyObject(), anyObject(), anyObject(), anyObject());

        // This unfortunately needs to be a somewhat valid scan result, otherwise
        // |ScanDetailUtil.toScanDetail| raises exceptions.
        final ScanResult[] scanResults = new ScanResult[1];
        scanResults[0] = new ScanResult(WifiSsid.createFromAsciiEncoded(CANDIDATE_SSID),
                CANDIDATE_SSID, CANDIDATE_BSSID, 1245, 0, "some caps",
                -78, 2450, 1025, 22, 33, 20, 0, 0, true);
        scanResults[0].informationElements = new InformationElement[1];
        scanResults[0].informationElements[0] = new InformationElement();
        scanResults[0].informationElements[0].id = InformationElement.EID_SSID;
        scanResults[0].informationElements[0].bytes =
            CANDIDATE_SSID.getBytes(StandardCharsets.UTF_8);

        doAnswer(new AnswerWithArguments() {
            public void answer(ScanSettings settings, PnoSettings pnoSettings,
                    Executor executor, PnoScanListener listener) throws Exception {
                listener.onPnoNetworkFound(scanResults);
            }}).when(scanner).startDisconnectedPnoScan(
                    anyObject(), anyObject(), anyObject(), anyObject());

        return scanner;
    }

    WifiConnectivityHelper mockWifiConnectivityHelper() {
        WifiConnectivityHelper connectivityHelper = mock(WifiConnectivityHelper.class);

        when(connectivityHelper.isFirmwareRoamingSupported()).thenReturn(false);
        when(connectivityHelper.getMaxNumBlacklistBssid()).thenReturn(MAX_BSSID_BLACKLIST_SIZE);

        return connectivityHelper;
    }

    ClientModeImpl mockClientModeImpl() {
        ClientModeImpl stateMachine = mock(ClientModeImpl.class);

        when(stateMachine.isConnected()).thenReturn(false);
        when(stateMachine.isDisconnected()).thenReturn(true);
        when(stateMachine.isSupplicantTransientState()).thenReturn(false);

        return stateMachine;
    }

    WifiNetworkSelector mockWifiNetworkSelector() {
        WifiNetworkSelector ns = mock(WifiNetworkSelector.class);

        WifiConfiguration candidate = generateWifiConfig(
                0, CANDIDATE_NETWORK_ID, CANDIDATE_SSID, false, true, null, null);
        candidate.BSSID = ClientModeImpl.SUPPLICANT_BSSID_ANY;
        ScanResult candidateScanResult = new ScanResult();
        candidateScanResult.SSID = CANDIDATE_SSID;
        candidateScanResult.BSSID = CANDIDATE_BSSID;
        candidate.getNetworkSelectionStatus().setCandidate(candidateScanResult);

        when(mWifiConfigManager.getConfiguredNetwork(CANDIDATE_NETWORK_ID)).thenReturn(candidate);
        MacAddress macAddress = MacAddress.fromString(CANDIDATE_BSSID);
        WifiCandidates.Key key = new WifiCandidates.Key(mock(ScanResultMatchInfo.class),
                macAddress, 0);
        when(mCandidate1.getKey()).thenReturn(key);
        when(mCandidate1.getScanRssi()).thenReturn(-40);
        when(mCandidate1.getFrequency()).thenReturn(TEST_FREQUENCY);
        when(mCandidate2.getKey()).thenReturn(key);
        when(mCandidate2.getScanRssi()).thenReturn(-60);
        mCandidateList = new ArrayList<WifiCandidates.Candidate>();
        mCandidateList.add(mCandidate1);
        when(ns.getCandidatesFromScan(any(), any(), any(), anyBoolean(), anyBoolean(),
                anyBoolean())).thenReturn(mCandidateList);
        when(ns.selectNetwork(any()))
                .then(new AnswerWithArguments() {
                    public WifiConfiguration answer(List<WifiCandidates.Candidate> candidateList) {
                        if (candidateList == null || candidateList.size() == 0) {
                            return null;
                        }
                        return candidate;
                    }
                });
        return ns;
    }

    WifiInfo getWifiInfo() {
        WifiInfo wifiInfo = new WifiInfo();

        wifiInfo.setNetworkId(WifiConfiguration.INVALID_NETWORK_ID);
        wifiInfo.setBSSID(null);
        wifiInfo.setSupplicantState(SupplicantState.DISCONNECTED);

        return wifiInfo;
    }

    WifiConfigManager mockWifiConfigManager() {
        WifiConfigManager wifiConfigManager = mock(WifiConfigManager.class);
        WifiConfiguration config = WifiConfigurationTestUtil.createOpenNetwork();
        List<WifiConfiguration> networkList = new ArrayList<>();
        networkList.add(config);
        when(wifiConfigManager.getConfiguredNetwork(anyInt())).thenReturn(null);
        when(wifiConfigManager.getSavedNetworks(anyInt())).thenReturn(networkList);

        return wifiConfigManager;
    }

    WifiConnectivityManager createConnectivityManager() {
        return new WifiConnectivityManager(mContext,
                mScoringParams,
                mClientModeImpl, mWifiInjector, mWifiConfigManager, mWifiNetworkSuggestionsManager,
                mWifiInfo, mWifiNS, mWifiConnectivityHelper,
                mWifiLastResortWatchdog, mOpenNetworkNotifier,
                mWifiMetrics, new Handler(mLooper.getLooper()), mClock,
                mLocalLog, mWifiScoreCard);
    }

    void setWifiStateConnected() {
        // Prep for setting WiFi to connected state
        WifiConfiguration connectedWifiConfiguration = new WifiConfiguration();
        connectedWifiConfiguration.networkId = TEST_CONNECTED_NETWORK_ID;
        when(mClientModeImpl.getCurrentWifiConfiguration()).thenReturn(connectedWifiConfiguration);

        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_CONNECTED);
    }

    /**
     *  Wifi enters disconnected state while screen is on.
     *
     * Expected behavior: WifiConnectivityManager calls
     * ClientModeImpl.startConnectToNetwork() with the
     * expected candidate network ID and BSSID.
     */
    @Test
    public void enterWifiDisconnectedStateWhenScreenOn() {
        // Set screen to on
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Set WiFi to disconnected state
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        verify(mClientModeImpl).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
    }

    /**
     *  Wifi enters connected state while screen is on.
     *
     * Expected behavior: WifiConnectivityManager calls
     * ClientModeImpl.startConnectToNetwork() with the
     * expected candidate network ID and BSSID.
     */
    @Test
    public void enterWifiConnectedStateWhenScreenOn() {
        // Set screen to on
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Set WiFi to connected state
        setWifiStateConnected();
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_CONNECTED);

        verify(mClientModeImpl).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
    }

    /**
     *  Screen turned on while WiFi in disconnected state.
     *
     * Expected behavior: WifiConnectivityManager calls
     * ClientModeImpl.startConnectToNetwork() with the
     * expected candidate network ID and BSSID.
     */
    @Test
    public void turnScreenOnWhenWifiInDisconnectedState() {
        // Set WiFi to disconnected state
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        // Set screen to on
        mWifiConnectivityManager.handleScreenStateChanged(true);

        verify(mClientModeImpl, atLeastOnce()).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
    }

    /**
     *  Screen turned on while WiFi in connected state.
     *
     * Expected behavior: WifiConnectivityManager calls
     * ClientModeImpl.startConnectToNetwork() with the
     * expected candidate network ID and BSSID.
     */
    @Test
    public void turnScreenOnWhenWifiInConnectedState() {
        // Set WiFi to connected state
        setWifiStateConnected();

        // Set screen to on
        mWifiConnectivityManager.handleScreenStateChanged(true);

        verify(mClientModeImpl, atLeastOnce()).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
    }

    /**
     *  Screen turned on while WiFi in connected state but
     *  auto roaming is disabled.
     *
     * Expected behavior: WifiConnectivityManager doesn't invoke
     * ClientModeImpl.startConnectToNetwork() because roaming
     * is turned off.
     */
    @Test
    public void turnScreenOnWhenWifiInConnectedStateRoamingDisabled() {
        // Turn off auto roaming
        mResources.setBoolean(
                R.bool.config_wifi_framework_enable_associated_network_selection, false);
        mWifiConnectivityManager = createConnectivityManager();
        mWifiConnectivityManager.setTrustedConnectionAllowed(true);

        // Set WiFi to connected state
        setWifiStateConnected();

        // Set screen to on
        mWifiConnectivityManager.handleScreenStateChanged(true);

        verify(mClientModeImpl, times(0)).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
    }

    /**
     * Multiple back to back connection attempts within the rate interval should be rate limited.
     *
     * Expected behavior: WifiConnectivityManager calls ClientModeImpl.startConnectToNetwork()
     * with the expected candidate network ID and BSSID for only the expected number of times within
     * the given interval.
     */
    @Test
    public void connectionAttemptRateLimitedWhenScreenOff() {
        int maxAttemptRate = WifiConnectivityManager.MAX_CONNECTION_ATTEMPTS_RATE;
        int timeInterval = WifiConnectivityManager.MAX_CONNECTION_ATTEMPTS_TIME_INTERVAL_MS;
        int numAttempts = 0;
        int connectionAttemptIntervals = timeInterval / maxAttemptRate;

        mWifiConnectivityManager.handleScreenStateChanged(false);

        // First attempt the max rate number of connections within the rate interval.
        long currentTimeStamp = 0;
        for (int attempt = 0; attempt < maxAttemptRate; attempt++) {
            currentTimeStamp += connectionAttemptIntervals;
            when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);
            // Set WiFi to disconnected state to trigger PNO scan
            mWifiConnectivityManager.handleConnectionStateChanged(
                    WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
            numAttempts++;
        }
        // Now trigger another connection attempt before the rate interval, this should be
        // skipped because we've crossed rate limit.
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);
        // Set WiFi to disconnected state to trigger PNO scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        // Verify that we attempt to connect upto the rate.
        verify(mClientModeImpl, times(numAttempts)).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
    }

    /**
     * Multiple back to back connection attempts outside the rate interval should not be rate
     * limited.
     *
     * Expected behavior: WifiConnectivityManager calls ClientModeImpl.startConnectToNetwork()
     * with the expected candidate network ID and BSSID for only the expected number of times within
     * the given interval.
     */
    @Test
    public void connectionAttemptNotRateLimitedWhenScreenOff() {
        int maxAttemptRate = WifiConnectivityManager.MAX_CONNECTION_ATTEMPTS_RATE;
        int timeInterval = WifiConnectivityManager.MAX_CONNECTION_ATTEMPTS_TIME_INTERVAL_MS;
        int numAttempts = 0;
        int connectionAttemptIntervals = timeInterval / maxAttemptRate;

        mWifiConnectivityManager.handleScreenStateChanged(false);

        // First attempt the max rate number of connections within the rate interval.
        long currentTimeStamp = 0;
        for (int attempt = 0; attempt < maxAttemptRate; attempt++) {
            currentTimeStamp += connectionAttemptIntervals;
            when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);
            // Set WiFi to disconnected state to trigger PNO scan
            mWifiConnectivityManager.handleConnectionStateChanged(
                    WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
            numAttempts++;
        }
        // Now trigger another connection attempt after the rate interval, this should not be
        // skipped because we should've evicted the older attempt.
        when(mClock.getElapsedSinceBootMillis()).thenReturn(
                currentTimeStamp + connectionAttemptIntervals * 2);
        // Set WiFi to disconnected state to trigger PNO scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
        numAttempts++;

        // Verify that all the connection attempts went through
        verify(mClientModeImpl, times(numAttempts)).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
    }

    /**
     * Multiple back to back connection attempts after a force connectivity scan should not be rate
     * limited.
     *
     * Expected behavior: WifiConnectivityManager calls ClientModeImpl.startConnectToNetwork()
     * with the expected candidate network ID and BSSID for only the expected number of times within
     * the given interval.
     */
    @Test
    public void connectionAttemptNotRateLimitedWhenScreenOffForceConnectivityScan() {
        int maxAttemptRate = WifiConnectivityManager.MAX_CONNECTION_ATTEMPTS_RATE;
        int timeInterval = WifiConnectivityManager.MAX_CONNECTION_ATTEMPTS_TIME_INTERVAL_MS;
        int numAttempts = 0;
        int connectionAttemptIntervals = timeInterval / maxAttemptRate;

        mWifiConnectivityManager.handleScreenStateChanged(false);

        // First attempt the max rate number of connections within the rate interval.
        long currentTimeStamp = 0;
        for (int attempt = 0; attempt < maxAttemptRate; attempt++) {
            currentTimeStamp += connectionAttemptIntervals;
            when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);
            // Set WiFi to disconnected state to trigger PNO scan
            mWifiConnectivityManager.handleConnectionStateChanged(
                    WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
            numAttempts++;
        }

        mWifiConnectivityManager.forceConnectivityScan(new WorkSource());

        for (int attempt = 0; attempt < maxAttemptRate; attempt++) {
            currentTimeStamp += connectionAttemptIntervals;
            when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);
            // Set WiFi to disconnected state to trigger PNO scan
            mWifiConnectivityManager.handleConnectionStateChanged(
                    WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
            numAttempts++;
        }

        // Verify that all the connection attempts went through
        verify(mClientModeImpl, times(numAttempts)).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
    }

    /**
     * Multiple back to back connection attempts after a user selection should not be rate limited.
     *
     * Expected behavior: WifiConnectivityManager calls ClientModeImpl.startConnectToNetwork()
     * with the expected candidate network ID and BSSID for only the expected number of times within
     * the given interval.
     */
    @Test
    public void connectionAttemptNotRateLimitedWhenScreenOffAfterUserSelection() {
        int maxAttemptRate = WifiConnectivityManager.MAX_CONNECTION_ATTEMPTS_RATE;
        int timeInterval = WifiConnectivityManager.MAX_CONNECTION_ATTEMPTS_TIME_INTERVAL_MS;
        int numAttempts = 0;
        int connectionAttemptIntervals = timeInterval / maxAttemptRate;

        mWifiConnectivityManager.handleScreenStateChanged(false);

        // First attempt the max rate number of connections within the rate interval.
        long currentTimeStamp = 0;
        for (int attempt = 0; attempt < maxAttemptRate; attempt++) {
            currentTimeStamp += connectionAttemptIntervals;
            when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);
            // Set WiFi to disconnected state to trigger PNO scan
            mWifiConnectivityManager.handleConnectionStateChanged(
                    WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
            numAttempts++;
        }

        mWifiConnectivityManager.setUserConnectChoice(CANDIDATE_NETWORK_ID);
        mWifiConnectivityManager.prepareForForcedConnection(CANDIDATE_NETWORK_ID);

        for (int attempt = 0; attempt < maxAttemptRate; attempt++) {
            currentTimeStamp += connectionAttemptIntervals;
            when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);
            // Set WiFi to disconnected state to trigger PNO scan
            mWifiConnectivityManager.handleConnectionStateChanged(
                    WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
            numAttempts++;
        }

        // Verify that all the connection attempts went through
        verify(mClientModeImpl, times(numAttempts)).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
    }

    /**
     *  PNO retry for low RSSI networks.
     *
     * Expected behavior: WifiConnectivityManager doubles the low RSSI
     * network retry delay value after QNS skips the PNO scan results
     * because of their low RSSI values.
     */
    @Test
    public void pnoRetryForLowRssiNetwork() {
        when(mWifiNS.selectNetwork(any())).thenReturn(null);

        // Set screen to off
        mWifiConnectivityManager.handleScreenStateChanged(false);

        // Get the current retry delay value
        int lowRssiNetworkRetryDelayStartValue = mWifiConnectivityManager
                .getLowRssiNetworkRetryDelay();

        // Set WiFi to disconnected state to trigger PNO scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        // Get the retry delay value after QNS didn't select a
        // network candicate from the PNO scan results.
        int lowRssiNetworkRetryDelayAfterPnoValue = mWifiConnectivityManager
                .getLowRssiNetworkRetryDelay();

        assertEquals(lowRssiNetworkRetryDelayStartValue * 2,
                lowRssiNetworkRetryDelayAfterPnoValue);
    }

    /**
     * Ensure that the watchdog bite increments the "Pno bad" metric.
     *
     * Expected behavior: WifiConnectivityManager detects that the PNO scan failed to find
     * a candidate while watchdog single scan did.
     */
    @Test
    public void watchdogBitePnoBadIncrementsMetrics() {
        // Set screen to off
        mWifiConnectivityManager.handleScreenStateChanged(false);

        // Set WiFi to disconnected state to trigger PNO scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        // Now fire the watchdog alarm and verify the metrics were incremented.
        mAlarmManager.dispatch(WifiConnectivityManager.WATCHDOG_TIMER_TAG);
        mLooper.dispatchAll();

        verify(mWifiMetrics).incrementNumConnectivityWatchdogPnoBad();
        verify(mWifiMetrics, never()).incrementNumConnectivityWatchdogPnoGood();
    }

    /**
     * Ensure that the watchdog bite increments the "Pno good" metric.
     *
     * Expected behavior: WifiConnectivityManager detects that the PNO scan failed to find
     * a candidate which was the same with watchdog single scan.
     */
    @Test
    public void watchdogBitePnoGoodIncrementsMetrics() {
        // Qns returns no candidate after watchdog single scan.
        when(mWifiNS.selectNetwork(any())).thenReturn(null);

        // Set screen to off
        mWifiConnectivityManager.handleScreenStateChanged(false);

        // Set WiFi to disconnected state to trigger PNO scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        // Now fire the watchdog alarm and verify the metrics were incremented.
        mAlarmManager.dispatch(WifiConnectivityManager.WATCHDOG_TIMER_TAG);
        mLooper.dispatchAll();

        verify(mWifiMetrics).incrementNumConnectivityWatchdogPnoGood();
        verify(mWifiMetrics, never()).incrementNumConnectivityWatchdogPnoBad();
    }

    /**
     * Verify that 2 scans that are sufficiently far apart are required to initiate a connection
     * when the high mobility scanning optimization is enabled.
     */
    @Test
    public void testHighMovementNetworkSelection() {
        when(mClock.getElapsedSinceBootMillis()).thenReturn(0L);
        // Enable high movement optimization
        mResources.setBoolean(R.bool.config_wifiHighMovementNetworkSelectionOptimizationEnabled,
                true);
        mWifiConnectivityManager.setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_HIGH_MVMT);

        // Set WiFi to disconnected state to trigger scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
        mLooper.dispatchAll();

        // Verify there is no connection due to currently having no cached candidates.
        verify(mClientModeImpl, never()).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);

        // Move time forward but do not cross HIGH_MVMT_SCAN_DELAY_MS yet.
        when(mClock.getElapsedSinceBootMillis()).thenReturn(HIGH_MVMT_SCAN_DELAY_MS - 1L);
        // Set WiFi to disconnected state to trigger scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
        mLooper.dispatchAll();

        // Verify we still don't connect because not enough time have passed since the candidates
        // were cached.
        verify(mClientModeImpl, never()).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);

        // Move time past HIGH_MVMT_SCAN_DELAY_MS.
        when(mClock.getElapsedSinceBootMillis()).thenReturn((long) HIGH_MVMT_SCAN_DELAY_MS);
        // Set WiFi to disconnected state to trigger scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
        mLooper.dispatchAll();

        // Verify a candidate if found this time.
        verify(mClientModeImpl).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
        verify(mWifiMetrics, times(2)).incrementNumHighMovementConnectionSkipped();
        verify(mWifiMetrics).incrementNumHighMovementConnectionStarted();
    }

    /**
     * Verify that the device is initiating partial scans to verify AP stability in the high
     * movement mobility state.
     */
    @Test
    public void testHighMovementTriggerPartialScan() {
        when(mClock.getElapsedSinceBootMillis()).thenReturn(0L);
        // Enable high movement optimization
        mResources.setBoolean(R.bool.config_wifiHighMovementNetworkSelectionOptimizationEnabled,
                true);
        mWifiConnectivityManager.setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_HIGH_MVMT);

        // Set WiFi to disconnected state to trigger scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
        mLooper.dispatchAll();
        // Verify there is no connection due to currently having no cached candidates.
        verify(mClientModeImpl, never()).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);

        // Move time forward and verify that a delayed partial scan is scheduled.
        when(mClock.getElapsedSinceBootMillis()).thenReturn(HIGH_MVMT_SCAN_DELAY_MS + 1L);
        mAlarmManager.dispatch(WifiConnectivityManager.DELAYED_PARTIAL_SCAN_TIMER_TAG);
        mLooper.dispatchAll();

        verify(mWifiScanner).startScan((ScanSettings) argThat(new WifiPartialScanSettingMatcher()),
                any(), any(), any());
    }

    private class WifiPartialScanSettingMatcher implements ArgumentMatcher<ScanSettings> {
        @Override
        public boolean matches(ScanSettings scanSettings) {
            return scanSettings.band == WifiScanner.WIFI_BAND_UNSPECIFIED
                    && scanSettings.channels[0].frequency == TEST_FREQUENCY;
        }
    }

    /**
     * Verify that when there are we obtain more than one valid candidates from scan results and
     * network connection fails, connection is immediately retried on the remaining candidates.
     */
    @Test
    public void testRetryConnectionOnFailure() {
        // Setup WifiNetworkSelector to return 2 valid candidates from scan results
        MacAddress macAddress = MacAddress.fromString(CANDIDATE_BSSID_2);
        WifiCandidates.Key key = new WifiCandidates.Key(mock(ScanResultMatchInfo.class),
                macAddress, 0);
        WifiCandidates.Candidate otherCandidate = mock(WifiCandidates.Candidate.class);
        when(otherCandidate.getKey()).thenReturn(key);
        List<WifiCandidates.Candidate> candidateList = new ArrayList<>();
        candidateList.add(mCandidate1);
        candidateList.add(otherCandidate);
        when(mWifiNS.getCandidatesFromScan(any(), any(), any(), anyBoolean(), anyBoolean(),
                anyBoolean())).thenReturn(candidateList);

        // Set WiFi to disconnected state to trigger scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
        mLooper.dispatchAll();
        // Verify a connection starting
        verify(mWifiNS).selectNetwork((List<WifiCandidates.Candidate>)
                argThat(new WifiCandidatesListSizeMatcher(2)));
        verify(mClientModeImpl).startConnectToNetwork(anyInt(), anyInt(), any());

        // Simulate the connection failing
        mWifiConnectivityManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_ASSOCIATION_REJECTION, CANDIDATE_BSSID,
                CANDIDATE_SSID);
        // Verify the failed BSSID is added to blocklist
        verify(mBssidBlocklistMonitor).blockBssidForDurationMs(eq(CANDIDATE_BSSID),
                eq(CANDIDATE_SSID), anyLong(), anyInt(), anyInt());
        // Verify another connection starting
        verify(mWifiNS).selectNetwork((List<WifiCandidates.Candidate>)
                argThat(new WifiCandidatesListSizeMatcher(1)));
        verify(mClientModeImpl, times(2)).startConnectToNetwork(anyInt(), anyInt(), any());

        // Simulate the second connection also failing
        mWifiConnectivityManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_ASSOCIATION_REJECTION, CANDIDATE_BSSID_2,
                CANDIDATE_SSID);
        // Verify there are no more connections
        verify(mWifiNS).selectNetwork((List<WifiCandidates.Candidate>)
                argThat(new WifiCandidatesListSizeMatcher(0)));
        verify(mClientModeImpl, times(2)).startConnectToNetwork(anyInt(), anyInt(), any());
    }

    private class WifiCandidatesListSizeMatcher implements
            ArgumentMatcher<List<WifiCandidates.Candidate>> {
        int mSize;
        WifiCandidatesListSizeMatcher(int size) {
            mSize = size;
        }
        @Override
        public boolean matches(List<WifiCandidates.Candidate> candidateList) {
            return candidateList.size() == mSize;
        }
    }

    /**
     * Verify that the cached candidates become cleared after a period of time.
     */
    @Test
    public void testRetryConnectionOnFailureCacheTimeout() {
        // Setup WifiNetworkSelector to return 2 valid candidates from scan results
        when(mClock.getElapsedSinceBootMillis()).thenReturn(0L);
        MacAddress macAddress = MacAddress.fromString(CANDIDATE_BSSID_2);
        WifiCandidates.Key key = new WifiCandidates.Key(mock(ScanResultMatchInfo.class),
                macAddress, 0);
        WifiCandidates.Candidate otherCandidate = mock(WifiCandidates.Candidate.class);
        when(otherCandidate.getKey()).thenReturn(key);
        List<WifiCandidates.Candidate> candidateList = new ArrayList<>();
        candidateList.add(mCandidate1);
        candidateList.add(otherCandidate);
        when(mWifiNS.getCandidatesFromScan(any(), any(), any(), anyBoolean(), anyBoolean(),
                anyBoolean())).thenReturn(candidateList);

        // Set WiFi to disconnected state to trigger scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
        mLooper.dispatchAll();
        // Verify a connection starting
        verify(mWifiNS).selectNetwork((List<WifiCandidates.Candidate>)
                argThat(new WifiCandidatesListSizeMatcher(2)));
        verify(mClientModeImpl).startConnectToNetwork(anyInt(), anyInt(), any());

        // Simulate the connection failing after the cache timeout period.
        when(mClock.getElapsedSinceBootMillis()).thenReturn(TEMP_BSSID_BLOCK_DURATION_MS + 1L);
        mWifiConnectivityManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_ASSOCIATION_REJECTION, CANDIDATE_BSSID,
                CANDIDATE_SSID);
        // verify there are no additional connections.
        verify(mClientModeImpl).startConnectToNetwork(anyInt(), anyInt(), any());
    }

    /**
     * Verify that in the high movement mobility state, when the RSSI delta of a BSSID from
     * 2 consecutive scans becomes greater than a threshold, the candidate get ignored from
     * network selection.
     */
    @Test
    public void testHighMovementRssiFilter() {
        when(mClock.getElapsedSinceBootMillis()).thenReturn(0L);
        // Enable high movement optimization
        mResources.setBoolean(R.bool.config_wifiHighMovementNetworkSelectionOptimizationEnabled,
                true);
        mWifiConnectivityManager.setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_HIGH_MVMT);

        // Set WiFi to disconnected state to trigger scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
        mLooper.dispatchAll();

        // Verify there is no connection due to currently having no cached candidates.
        verify(mClientModeImpl, never()).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);

        // Move time past HIGH_MVMT_SCAN_DELAY_MS.
        when(mClock.getElapsedSinceBootMillis()).thenReturn((long) HIGH_MVMT_SCAN_DELAY_MS);

        // Mock the current Candidate to have RSSI over the filter threshold
        mCandidateList.clear();
        mCandidateList.add(mCandidate2);

        // Set WiFi to disconnected state to trigger scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
        mLooper.dispatchAll();

        // Verify connect is not started.
        verify(mClientModeImpl, never()).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
        verify(mWifiMetrics, times(2)).incrementNumHighMovementConnectionSkipped();
    }

    /**
     * {@link OpenNetworkNotifier} handles scan results on network selection.
     *
     * Expected behavior: ONA handles scan results
     */
    @Test
    public void wifiDisconnected_noConnectionCandidate_openNetworkNotifierScanResultsHandled() {
        // no connection candidate selected
        when(mWifiNS.selectNetwork(any())).thenReturn(null);

        List<ScanDetail> expectedOpenNetworks = new ArrayList<>();
        expectedOpenNetworks.add(
                new ScanDetail(
                        new ScanResult(WifiSsid.createFromAsciiEncoded(CANDIDATE_SSID),
                                CANDIDATE_SSID, CANDIDATE_BSSID, 1245, 0, "some caps", -78, 2450,
                                1025, 22, 33, 20, 0, 0, true), null));

        when(mWifiNS.getFilteredScanDetailsForOpenUnsavedNetworks())
                .thenReturn(expectedOpenNetworks);

        // Set WiFi to disconnected state to trigger PNO scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        verify(mOpenNetworkNotifier).handleScanResults(expectedOpenNetworks);
    }

    /**
     * When wifi is connected, {@link OpenNetworkNotifier} handles the Wi-Fi connected behavior.
     *
     * Expected behavior: ONA handles connected behavior
     */
    @Test
    public void wifiConnected_openNetworkNotifierHandlesConnection() {
        // Set WiFi to connected state
        mWifiInfo.setSSID(WifiSsid.createFromAsciiEncoded(CANDIDATE_SSID));
        mWifiConnectivityManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, CANDIDATE_BSSID, CANDIDATE_SSID);
        verify(mOpenNetworkNotifier).handleWifiConnected(CANDIDATE_SSID);
    }

    /**
     * When wifi is connected, {@link OpenNetworkNotifier} handles connection state
     * change.
     *
     * Expected behavior: ONA does not clear pending notification.
     */
    @Test
    public void wifiDisconnected_openNetworkNotifierDoesNotClearPendingNotification() {
        // Set WiFi to disconnected state
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        verify(mOpenNetworkNotifier, never()).clearPendingNotification(anyBoolean());
    }

    /**
     * When a Wi-Fi connection attempt ends, {@link OpenNetworkNotifier} handles the connection
     * failure. A failure code that is not {@link WifiMetrics.ConnectionEvent#FAILURE_NONE}
     * represents a connection failure.
     *
     * Expected behavior: ONA handles connection failure.
     */
    @Test
    public void wifiConnectionEndsWithFailure_openNetworkNotifierHandlesConnectionFailure() {
        mWifiConnectivityManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_CONNECT_NETWORK_FAILED, CANDIDATE_BSSID,
                CANDIDATE_SSID);

        verify(mOpenNetworkNotifier).handleConnectionFailure();
    }

    /**
     * When a Wi-Fi connection attempt ends, {@link OpenNetworkNotifier} does not handle connection
     * failure after a successful connection. {@link WifiMetrics.ConnectionEvent#FAILURE_NONE}
     * represents a successful connection.
     *
     * Expected behavior: ONA does nothing.
     */
    @Test
    public void wifiConnectionEndsWithSuccess_openNetworkNotifierDoesNotHandleConnectionFailure() {
        mWifiConnectivityManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, CANDIDATE_BSSID, CANDIDATE_SSID);

        verify(mOpenNetworkNotifier, never()).handleConnectionFailure();
    }

    /**
     * When Wi-Fi is disabled, clear the pending notification and reset notification repeat delay.
     *
     * Expected behavior: clear pending notification and reset notification repeat delay
     * */
    @Test
    public void openNetworkNotifierClearsPendingNotificationOnWifiDisabled() {
        mWifiConnectivityManager.setWifiEnabled(false);

        verify(mOpenNetworkNotifier).clearPendingNotification(true /* resetRepeatDelay */);
    }

    /**
     * Verify that the ONA controller tracks screen state changes.
     */
    @Test
    public void openNetworkNotifierTracksScreenStateChanges() {
        mWifiConnectivityManager.handleScreenStateChanged(false);

        verify(mOpenNetworkNotifier).handleScreenStateChanged(false);

        mWifiConnectivityManager.handleScreenStateChanged(true);

        verify(mOpenNetworkNotifier).handleScreenStateChanged(true);
    }

    /**
     * Verify that if configuration for single scan schedule is empty, default
     * schedule is being used.
     */
    @Test
    public void checkPeriodicScanIntervalWhenDisconnectedWithEmptySchedule() throws Exception {
        mResources.setIntArray(R.array.config_wifiDisconnectedScanIntervalScheduleSec,
                SCHEDULE_EMPTY_SEC);

        checkWorkingWithDefaultSchedule();
    }

    /**
     * Verify that if configuration for single scan schedule has zero values, default
     * schedule is being used.
     */
    @Test
    public void checkPeriodicScanIntervalWhenDisconnectedWithZeroValuesSchedule() {
        mResources.setIntArray(
                R.array.config_wifiDisconnectedScanIntervalScheduleSec,
                INVALID_SCHEDULE_ZERO_VALUES_SEC);

        checkWorkingWithDefaultSchedule();
    }

    /**
     * Verify that if configuration for single scan schedule has negative values, default
     * schedule is being used.
     */
    @Test
    public void checkPeriodicScanIntervalWhenDisconnectedWithNegativeValuesSchedule() {
        mResources.setIntArray(
                R.array.config_wifiDisconnectedScanIntervalScheduleSec,
                INVALID_SCHEDULE_NEGATIVE_VALUES_SEC);

        checkWorkingWithDefaultSchedule();
    }

    private void checkWorkingWithDefaultSchedule() {
        long currentTimeStamp = CURRENT_SYSTEM_TIME_MS;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        mWifiConnectivityManager = createConnectivityManager();
        mWifiConnectivityManager.setTrustedConnectionAllowed(true);
        mWifiConnectivityManager.setWifiEnabled(true);

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Wait for max periodic scan interval so that any impact triggered
        // by screen state change can settle
        currentTimeStamp += MAX_SCAN_INTERVAL_IN_DEFAULT_SCHEDULE_SEC * 1000;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set WiFi to disconnected state to trigger periodic scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        // Get the first periodic scan interval
        long firstIntervalMs = mAlarmManager
                .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                - currentTimeStamp;
        assertEquals(DEFAULT_SINGLE_SCAN_SCHEDULE_SEC[0] * 1000, firstIntervalMs);

        currentTimeStamp += firstIntervalMs;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Now fire the first periodic scan alarm timer
        mAlarmManager.dispatch(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG);
        mLooper.dispatchAll();

        // Get the second periodic scan interval
        long secondIntervalMs = mAlarmManager
                .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                - currentTimeStamp;

        // Verify the intervals are exponential back off
        assertEquals(DEFAULT_SINGLE_SCAN_SCHEDULE_SEC[1] * 1000, secondIntervalMs);

        currentTimeStamp += secondIntervalMs;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Make sure we eventually stay at the maximum scan interval.
        long intervalMs = 0;
        for (int i = 0; i < 5; i++) {
            mAlarmManager.dispatch(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG);
            mLooper.dispatchAll();
            intervalMs = mAlarmManager
                    .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                    - currentTimeStamp;
            currentTimeStamp += intervalMs;
            when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);
        }

        assertEquals(DEFAULT_SINGLE_SCAN_SCHEDULE_SEC[DEFAULT_SINGLE_SCAN_SCHEDULE_SEC.length - 1]
                * 1000, intervalMs);
    }

    /**
     *  Verify that scan interval for screen on and wifi disconnected scenario
     *  is in the exponential backoff fashion.
     *
     * Expected behavior: WifiConnectivityManager doubles periodic
     * scan interval.
     */
    @Test
    public void checkPeriodicScanIntervalWhenDisconnected() {
        long currentTimeStamp = CURRENT_SYSTEM_TIME_MS;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Wait for max periodic scan interval so that any impact triggered
        // by screen state change can settle
        currentTimeStamp += MAX_SCAN_INTERVAL_IN_SCHEDULE_SEC * 1000;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set WiFi to disconnected state to trigger periodic scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        // Get the first periodic scan interval
        long firstIntervalMs = mAlarmManager
                .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                - currentTimeStamp;
        assertEquals(VALID_DISCONNECTED_SINGLE_SCAN_SCHEDULE_SEC[0] * 1000, firstIntervalMs);

        currentTimeStamp += firstIntervalMs;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Now fire the first periodic scan alarm timer
        mAlarmManager.dispatch(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG);
        mLooper.dispatchAll();

        // Get the second periodic scan interval
        long secondIntervalMs = mAlarmManager
                .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                - currentTimeStamp;

        // Verify the intervals are exponential back off
        assertEquals(VALID_DISCONNECTED_SINGLE_SCAN_SCHEDULE_SEC[1] * 1000, secondIntervalMs);

        currentTimeStamp += secondIntervalMs;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Make sure we eventually stay at the maximum scan interval.
        long intervalMs = 0;
        for (int i = 0; i < 5; i++) {
            mAlarmManager.dispatch(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG);
            mLooper.dispatchAll();
            intervalMs = mAlarmManager
                    .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                    - currentTimeStamp;
            currentTimeStamp += intervalMs;
            when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);
        }

        assertEquals(VALID_DISCONNECTED_SINGLE_SCAN_SCHEDULE_SEC[
                VALID_DISCONNECTED_SINGLE_SCAN_SCHEDULE_SEC.length - 1] * 1000, intervalMs);
    }

    /**
     *  Verify that scan interval for screen on and wifi connected scenario
     *  is in the exponential backoff fashion.
     *
     * Expected behavior: WifiConnectivityManager doubles periodic
     * scan interval.
     */
    @Test
    public void checkPeriodicScanIntervalWhenConnected() {
        long currentTimeStamp = CURRENT_SYSTEM_TIME_MS;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Wait for max scanning interval so that any impact triggered
        // by screen state change can settle
        currentTimeStamp += MAX_SCAN_INTERVAL_IN_SCHEDULE_SEC * 1000;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set WiFi to connected state to trigger periodic scan
        setWifiStateConnected();

        // Get the first periodic scan interval
        long firstIntervalMs = mAlarmManager
                .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                - currentTimeStamp;
        assertEquals(VALID_CONNECTED_SINGLE_SCAN_SCHEDULE_SEC[0] * 1000, firstIntervalMs);

        currentTimeStamp += firstIntervalMs;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Now fire the first periodic scan alarm timer
        mAlarmManager.dispatch(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG);
        mLooper.dispatchAll();

        // Get the second periodic scan interval
        long secondIntervalMs = mAlarmManager
                .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                - currentTimeStamp;

        // Verify the intervals are exponential back off
        assertEquals(VALID_CONNECTED_SINGLE_SCAN_SCHEDULE_SEC[1] * 1000, secondIntervalMs);

        currentTimeStamp += secondIntervalMs;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Make sure we eventually stay at the maximum scan interval.
        long intervalMs = 0;
        for (int i = 0; i < 5; i++) {
            mAlarmManager.dispatch(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG);
            mLooper.dispatchAll();
            intervalMs = mAlarmManager
                    .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                    - currentTimeStamp;
            currentTimeStamp += intervalMs;
            when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);
        }

        assertEquals(VALID_CONNECTED_SINGLE_SCAN_SCHEDULE_SEC[
                VALID_CONNECTED_SINGLE_SCAN_SCHEDULE_SEC.length - 1] * 1000, intervalMs);
    }

    /**
     * When screen on and single saved network schedule is set
     * If we have multiple saved networks, the regular connected state scan schedule is used
     */
    @Test
    public void checkScanScheduleForMultipleSavedNetwork() {
        long currentTimeStamp = CURRENT_SYSTEM_TIME_MS;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Wait for max scanning interval so that any impact triggered
        // by screen state change can settle
        currentTimeStamp += MAX_SCAN_INTERVAL_IN_SCHEDULE_SEC * 1000;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        mResources.setIntArray(
                R.array.config_wifiSingleSavedNetworkConnectedScanIntervalScheduleSec,
                VALID_CONNECTED_SINGLE_SAVED_NETWORK_SCHEDULE_SEC);

        WifiConfiguration wifiConfiguration1 = new WifiConfiguration();
        WifiConfiguration wifiConfiguration2 = new WifiConfiguration();
        wifiConfiguration1.status = WifiConfiguration.Status.CURRENT;
        List<WifiConfiguration> wifiConfigurationList = new ArrayList<WifiConfiguration>();
        wifiConfigurationList.add(wifiConfiguration1);
        wifiConfigurationList.add(wifiConfiguration2);
        when(mWifiConfigManager.getSavedNetworks(anyInt())).thenReturn(wifiConfigurationList);

        // Set firmware roaming to enabled
        when(mWifiConnectivityHelper.isFirmwareRoamingSupported()).thenReturn(true);

        // Set WiFi to connected state to trigger periodic scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_CONNECTED);

        // Get the first periodic scan interval
        long firstIntervalMs = mAlarmManager
                .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                - currentTimeStamp;
        assertEquals(VALID_CONNECTED_SINGLE_SCAN_SCHEDULE_SEC[0] * 1000, firstIntervalMs);
    }

    /**
     * When screen on and single saved network schedule is set
     * If we have a single saved network (connected network),
     * no passpoint or suggestion networks.
     * the single-saved-network connected state scan schedule is used
     */
    @Test
    public void checkScanScheduleForSingleSavedNetworkConnected() {
        long currentTimeStamp = CURRENT_SYSTEM_TIME_MS;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Wait for max scanning interval so that any impact triggered
        // by screen state change can settle
        currentTimeStamp += MAX_SCAN_INTERVAL_IN_SCHEDULE_SEC * 1000;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        mResources.setIntArray(
                R.array.config_wifiSingleSavedNetworkConnectedScanIntervalScheduleSec,
                VALID_CONNECTED_SINGLE_SAVED_NETWORK_SCHEDULE_SEC);

        WifiConfiguration wifiConfiguration = new WifiConfiguration();
        wifiConfiguration.networkId = TEST_CONNECTED_NETWORK_ID;
        List<WifiConfiguration> wifiConfigurationList = new ArrayList<WifiConfiguration>();
        wifiConfigurationList.add(wifiConfiguration);
        when(mWifiConfigManager.getSavedNetworks(anyInt())).thenReturn(wifiConfigurationList);

        // Set firmware roaming to enabled
        when(mWifiConnectivityHelper.isFirmwareRoamingSupported()).thenReturn(true);

        // Set WiFi to connected state to trigger periodic scan
        setWifiStateConnected();

        // Get the first periodic scan interval
        long firstIntervalMs = mAlarmManager
                .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                - currentTimeStamp;
        assertEquals(VALID_CONNECTED_SINGLE_SAVED_NETWORK_SCHEDULE_SEC[0] * 1000, firstIntervalMs);
    }

    /**
     * When screen on and single saved network schedule is set
     * If we have a single saved network (not connected network),
     * no passpoint or suggestion networks.
     * the regular connected state scan schedule is used
     */
    @Test
    public void checkScanScheduleForSingleSavedNetwork() {
        int testSavedNetworkId = TEST_CONNECTED_NETWORK_ID + 1;
        long currentTimeStamp = CURRENT_SYSTEM_TIME_MS;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Wait for max scanning interval so that any impact triggered
        // by screen state change can settle
        currentTimeStamp += MAX_SCAN_INTERVAL_IN_SCHEDULE_SEC * 1000;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        mResources.setIntArray(
                R.array.config_wifiSingleSavedNetworkConnectedScanIntervalScheduleSec,
                VALID_CONNECTED_SINGLE_SAVED_NETWORK_SCHEDULE_SEC);

        // Set firmware roaming to enabled
        when(mWifiConnectivityHelper.isFirmwareRoamingSupported()).thenReturn(true);

        WifiConfiguration wifiConfiguration = new WifiConfiguration();
        wifiConfiguration.status = WifiConfiguration.Status.ENABLED;
        wifiConfiguration.networkId = testSavedNetworkId;
        List<WifiConfiguration> wifiConfigurationList = new ArrayList<WifiConfiguration>();
        wifiConfigurationList.add(wifiConfiguration);
        when(mWifiConfigManager.getSavedNetworks(anyInt())).thenReturn(wifiConfigurationList);

        // Set WiFi to connected state to trigger periodic scan
        setWifiStateConnected();

        // Get the first periodic scan interval
        long firstIntervalMs = mAlarmManager
                .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                - currentTimeStamp;
        assertEquals(VALID_CONNECTED_SINGLE_SCAN_SCHEDULE_SEC[0] * 1000, firstIntervalMs);
    }

    /**
     * When screen on and single saved network schedule is set
     * If we have a single passpoint network (connected network),
     * and no saved or suggestion networks the single-saved-network
     * connected state scan schedule is used.
     */
    @Test
    public void checkScanScheduleForSinglePasspointNetworkConnected() {
        long currentTimeStamp = CURRENT_SYSTEM_TIME_MS;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Wait for max scanning interval so that any impact triggered
        // by screen state change can settle
        currentTimeStamp += MAX_SCAN_INTERVAL_IN_SCHEDULE_SEC * 1000;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        mResources.setIntArray(
                R.array.config_wifiSingleSavedNetworkConnectedScanIntervalScheduleSec,
                VALID_CONNECTED_SINGLE_SAVED_NETWORK_SCHEDULE_SEC);

        // Prepare for a single passpoint network
        WifiConfiguration config = new WifiConfiguration();
        config.networkId = TEST_CONNECTED_NETWORK_ID;
        String passpointKey = "PASSPOINT_KEY";
        when(mWifiConfigManager.getConfiguredNetwork(passpointKey)).thenReturn(config);
        List<PasspointConfiguration> passpointNetworks = new ArrayList<PasspointConfiguration>();
        passpointNetworks.add(mPasspointConfiguration);
        when(mPasspointConfiguration.getUniqueId()).thenReturn(passpointKey);
        when(mPasspointManager.getProviderConfigs(anyInt(), anyBoolean()))
                .thenReturn(passpointNetworks);

        // Prepare for no saved networks
        when(mWifiConfigManager.getSavedNetworks(anyInt())).thenReturn(new ArrayList<>());

        // Set firmware roaming to enabled
        when(mWifiConnectivityHelper.isFirmwareRoamingSupported()).thenReturn(true);

        // Set WiFi to connected state to trigger periodic scan
        setWifiStateConnected();

        // Get the first periodic scan interval
        long firstIntervalMs = mAlarmManager
                .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                - currentTimeStamp;
        assertEquals(VALID_CONNECTED_SINGLE_SAVED_NETWORK_SCHEDULE_SEC[0] * 1000, firstIntervalMs);
    }

    /**
     * When screen on and single saved network schedule is set
     * If we have a single suggestion network (connected network),
     * and no saved network or passpoint networks the single-saved-network
     * connected state scan schedule is used
     */
    @Test
    public void checkScanScheduleForSingleSuggestionsNetworkConnected() {
        long currentTimeStamp = CURRENT_SYSTEM_TIME_MS;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Wait for max scanning interval so that any impact triggered
        // by screen state change can settle
        currentTimeStamp += MAX_SCAN_INTERVAL_IN_SCHEDULE_SEC * 1000;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        mResources.setIntArray(
                R.array.config_wifiSingleSavedNetworkConnectedScanIntervalScheduleSec,
                VALID_CONNECTED_SINGLE_SAVED_NETWORK_SCHEDULE_SEC);

        // Prepare for a single suggestions network
        WifiConfiguration config = new WifiConfiguration();
        config.networkId = TEST_CONNECTED_NETWORK_ID;
        String networkKey = "NETWORK_KEY";
        when(mWifiConfigManager.getConfiguredNetwork(networkKey)).thenReturn(config);
        when(mSuggestionConfig.getKey()).thenReturn(networkKey);
        when(mWifiNetworkSuggestion.getWifiConfiguration()).thenReturn(mSuggestionConfig);
        Set<WifiNetworkSuggestion> suggestionNetworks = new HashSet<WifiNetworkSuggestion>();
        suggestionNetworks.add(mWifiNetworkSuggestion);
        when(mWifiNetworkSuggestionsManager.getAllApprovedNetworkSuggestions())
                .thenReturn(suggestionNetworks);

        // Prepare for no saved networks
        when(mWifiConfigManager.getSavedNetworks(anyInt())).thenReturn(new ArrayList<>());

        // Set firmware roaming to enabled
        when(mWifiConnectivityHelper.isFirmwareRoamingSupported()).thenReturn(true);

        // Set WiFi to connected state to trigger periodic scan
        setWifiStateConnected();

        // Get the first periodic scan interval
        long firstIntervalMs = mAlarmManager
                .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                - currentTimeStamp;
        assertEquals(VALID_CONNECTED_SINGLE_SAVED_NETWORK_SCHEDULE_SEC[0] * 1000, firstIntervalMs);
    }

    /**
     * When screen on and single saved network schedule is set
     * If we have a single suggestion network (connected network),
     * and saved network/passpoint networks the regular
     * connected state scan schedule is used
     */
    @Test
    public void checkScanScheduleForSavedPasspointSuggestionNetworkConnected() {
        long currentTimeStamp = CURRENT_SYSTEM_TIME_MS;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Wait for max scanning interval so that any impact triggered
        // by screen state change can settle
        currentTimeStamp += MAX_SCAN_INTERVAL_IN_SCHEDULE_SEC * 1000;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        mResources.setIntArray(
                R.array.config_wifiSingleSavedNetworkConnectedScanIntervalScheduleSec,
                VALID_CONNECTED_SINGLE_SAVED_NETWORK_SCHEDULE_SEC);

        // Prepare for a single suggestions network
        WifiConfiguration config = new WifiConfiguration();
        config.networkId = TEST_CONNECTED_NETWORK_ID;
        String networkKey = "NETWORK_KEY";
        when(mWifiConfigManager.getConfiguredNetwork(networkKey)).thenReturn(config);
        when(mSuggestionConfig.getKey()).thenReturn(networkKey);
        when(mWifiNetworkSuggestion.getWifiConfiguration()).thenReturn(mSuggestionConfig);
        Set<WifiNetworkSuggestion> suggestionNetworks = new HashSet<WifiNetworkSuggestion>();
        suggestionNetworks.add(mWifiNetworkSuggestion);
        when(mWifiNetworkSuggestionsManager.getAllApprovedNetworkSuggestions())
                .thenReturn(suggestionNetworks);

        // Prepare for a single passpoint network
        WifiConfiguration passpointConfig = new WifiConfiguration();
        String passpointKey = "PASSPOINT_KEY";
        when(mWifiConfigManager.getConfiguredNetwork(passpointKey)).thenReturn(passpointConfig);
        List<PasspointConfiguration> passpointNetworks = new ArrayList<PasspointConfiguration>();
        passpointNetworks.add(mPasspointConfiguration);
        when(mPasspointConfiguration.getUniqueId()).thenReturn(passpointKey);
        when(mPasspointManager.getProviderConfigs(anyInt(), anyBoolean()))
                .thenReturn(passpointNetworks);

        // Set firmware roaming to enabled
        when(mWifiConnectivityHelper.isFirmwareRoamingSupported()).thenReturn(true);

        // Set WiFi to connected state to trigger periodic scan
        setWifiStateConnected();

        // Get the first periodic scan interval
        long firstIntervalMs = mAlarmManager
                .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                - currentTimeStamp;
        assertEquals(VALID_CONNECTED_SINGLE_SCAN_SCHEDULE_SEC[0] * 1000, firstIntervalMs);
    }

    /**
     * Remove network will trigger update scan and meet single network requirement.
     * Verify before disconnect finished, will not trigger single network scan schedule.
     */
    @Test
    public void checkScanScheduleForCurrentConnectedNetworkIsNull() {
        long currentTimeStamp = CURRENT_SYSTEM_TIME_MS;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Wait for max scanning interval so that any impact triggered
        // by screen state change can settle
        currentTimeStamp += MAX_SCAN_INTERVAL_IN_SCHEDULE_SEC * 1000;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        mResources.setIntArray(
                R.array.config_wifiSingleSavedNetworkConnectedScanIntervalScheduleSec,
                VALID_CONNECTED_SINGLE_SAVED_NETWORK_SCHEDULE_SEC);

        // Set firmware roaming to enabled
        when(mWifiConnectivityHelper.isFirmwareRoamingSupported()).thenReturn(true);

        // Set up single saved network
        WifiConfiguration wifiConfiguration = new WifiConfiguration();
        wifiConfiguration.networkId = TEST_CONNECTED_NETWORK_ID;
        List<WifiConfiguration> wifiConfigurationList = new ArrayList<WifiConfiguration>();
        wifiConfigurationList.add(wifiConfiguration);
        when(mWifiConfigManager.getSavedNetworks(anyInt())).thenReturn(wifiConfigurationList);

        // Set WiFi to connected state.
        setWifiStateConnected();
        // Simulate remove network, disconnect not finished.
        when(mClientModeImpl.getCurrentWifiConfiguration()).thenReturn(null);
        mNetworkUpdateListenerCaptor.getValue().onNetworkRemoved(null);

        // Get the first periodic scan interval
        long firstIntervalMs = mAlarmManager
                .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG)
                - currentTimeStamp;
        assertEquals(VALID_CONNECTED_SINGLE_SCAN_SCHEDULE_SEC[0] * 1000, firstIntervalMs);
    }

    /**
     *  When screen on trigger a disconnected state change event then a connected state
     *  change event back to back to verify that the minium scan interval is enforced.
     *
     * Expected behavior: WifiConnectivityManager start the second periodic single
     * scan after the first one by first interval in connected scanning schedule.
     */
    @Test
    public void checkMinimumPeriodicScanIntervalWhenScreenOnAndConnected() {
        long currentTimeStamp = CURRENT_SYSTEM_TIME_MS;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Wait for max scanning interval in schedule so that any impact triggered
        // by screen state change can settle
        currentTimeStamp += MAX_SCAN_INTERVAL_IN_SCHEDULE_SEC * 1000;
        long scanForDisconnectedTimeStamp = currentTimeStamp;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set WiFi to disconnected state which triggers a scan immediately
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
        verify(mWifiScanner, times(1)).startScan(
                anyObject(), anyObject(), anyObject(), anyObject());

        // Set up time stamp for when entering CONNECTED state
        currentTimeStamp += 2000;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set WiFi to connected state to trigger its periodic scan
        setWifiStateConnected();

        // The very first scan triggered for connected state is actually via the alarm timer
        // and it obeys the minimum scan interval
        long firstScanForConnectedTimeStamp = mAlarmManager
                .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG);

        // Verify that the first scan for connected state is scheduled after the scan for
        // disconnected state by first interval in connected scanning schedule.
        assertEquals(scanForDisconnectedTimeStamp + VALID_CONNECTED_SINGLE_SCAN_SCHEDULE_SEC[0]
                * 1000, firstScanForConnectedTimeStamp);
    }

    /**
     *  When screen on trigger a connected state change event then a disconnected state
     *  change event back to back to verify that a scan is fired immediately for the
     *  disconnected state change event.
     *
     * Expected behavior: WifiConnectivityManager directly starts the periodic immediately
     * for the disconnected state change event. The second scan for disconnected state is
     * via alarm timer.
     */
    @Test
    public void scanImmediatelyWhenScreenOnAndDisconnected() {
        long currentTimeStamp = CURRENT_SYSTEM_TIME_MS;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Wait for maximum scanning interval in schedule so that any impact triggered
        // by screen state change can settle
        currentTimeStamp += MAX_SCAN_INTERVAL_IN_SCHEDULE_SEC * 1000;
        long scanForConnectedTimeStamp = currentTimeStamp;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set WiFi to connected state to trigger the periodic scan
        setWifiStateConnected();

        verify(mWifiScanner, times(1)).startScan(
                anyObject(), anyObject(), anyObject(), anyObject());

        // Set up the time stamp for when entering DISCONNECTED state
        currentTimeStamp += 2000;
        long enteringDisconnectedStateTimeStamp = currentTimeStamp;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set WiFi to disconnected state to trigger its periodic scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        // Verify the very first scan for DISCONNECTED state is fired immediately
        verify(mWifiScanner, times(2)).startScan(
                anyObject(), anyObject(), anyObject(), anyObject());
        long secondScanForDisconnectedTimeStamp = mAlarmManager
                .getTriggerTimeMillis(WifiConnectivityManager.PERIODIC_SCAN_TIMER_TAG);

        // Verify that the second scan is scheduled after entering DISCONNECTED state by first
        // interval in disconnected scanning schedule.
        assertEquals(enteringDisconnectedStateTimeStamp
                + VALID_DISCONNECTED_SINGLE_SCAN_SCHEDULE_SEC[0] * 1000,
                secondScanForDisconnectedTimeStamp);
    }

    /**
     *  When screen on trigger a connection state change event and a forced connectivity
     *  scan event back to back to verify that the minimum scan interval is not applied
     *  in this scenario.
     *
     * Expected behavior: WifiConnectivityManager starts the second periodic single
     * scan immediately.
     */
    @Test
    public void checkMinimumPeriodicScanIntervalNotEnforced() {
        long currentTimeStamp = CURRENT_SYSTEM_TIME_MS;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Wait for maximum interval in scanning schedule so that any impact triggered
        // by screen state change can settle
        currentTimeStamp += MAX_SCAN_INTERVAL_IN_SCHEDULE_SEC * 1000;
        long firstScanTimeStamp = currentTimeStamp;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Set WiFi to connected state to trigger the periodic scan
        setWifiStateConnected();

        // Set the second scan attempt time stamp
        currentTimeStamp += 2000;
        when(mClock.getElapsedSinceBootMillis()).thenReturn(currentTimeStamp);

        // Allow untrusted networks so WifiConnectivityManager starts a periodic scan
        // immediately.
        mWifiConnectivityManager.setUntrustedConnectionAllowed(true);

        // Get the second periodic scan actual time stamp. Note, this scan is not
        // started from the AlarmManager.
        long secondScanTimeStamp = mWifiConnectivityManager.getLastPeriodicSingleScanTimeStamp();

        // Verify that the second scan is fired immediately
        assertEquals(secondScanTimeStamp, currentTimeStamp);
    }

    /**
     * Verify that we perform full band scan in the following two cases
     * 1) Current RSSI is low, no active stream, network is insufficient
     * 2) Current RSSI is high, no active stream, and a long time since last network selection
     * 3) Current RSSI is high, no active stream, and a short time since last network selection,
     *  internet status is not acceptable
     *
     * Expected behavior: WifiConnectivityManager does full band scan in both cases
     */
    @Test
    public void verifyFullBandScanWhenConnected() {
        mResources.setInteger(
                R.integer.config_wifiConnectedHighRssiScanMinimumWindowSizeSec, 600);

        // Verify case 1
        when(mWifiNS.isNetworkSufficient(eq(mWifiInfo))).thenReturn(false);
        when(mWifiNS.hasActiveStream(eq(mWifiInfo))).thenReturn(false);
        when(mWifiNS.hasSufficientLinkQuality(eq(mWifiInfo))).thenReturn(false);
        when(mWifiNS.hasInternetOrExpectNoInternet(eq(mWifiInfo))).thenReturn(true);

        final List<Integer> channelList = new ArrayList<>();
        channelList.add(TEST_FREQUENCY_1);
        channelList.add(TEST_FREQUENCY_2);
        channelList.add(TEST_FREQUENCY_3);
        WifiConfiguration configuration = WifiConfigurationTestUtil.createOpenNetwork();
        configuration.networkId = TEST_CONNECTED_NETWORK_ID;
        when(mWifiConfigManager.getConfiguredNetwork(TEST_CONNECTED_NETWORK_ID))
                .thenReturn(configuration);
        when(mClientModeImpl.getCurrentWifiConfiguration())
                .thenReturn(configuration);
        when(mWifiScoreCard.lookupNetwork(configuration.SSID)).thenReturn(mPerNetwork);
        when(mPerNetwork.getFrequencies(anyLong())).thenReturn(new ArrayList<>());

        doAnswer(new AnswerWithArguments() {
            public void answer(ScanSettings settings, ScanListener listener,
                    WorkSource workSource) throws Exception {
                assertEquals(settings.band, WifiScanner.WIFI_BAND_ALL);
                assertNull(settings.channels);
            }}).when(mWifiScanner).startScan(anyObject(), anyObject(), anyObject());

        when(mClock.getElapsedSinceBootMillis()).thenReturn(0L);
        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Set WiFi to connected state to trigger periodic scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_CONNECTED);

        verify(mWifiScanner).startScan(anyObject(), anyObject(), anyObject(), anyObject());

        // Verify case 2
        when(mWifiNS.isNetworkSufficient(eq(mWifiInfo))).thenReturn(true);
        when(mWifiNS.hasActiveStream(eq(mWifiInfo))).thenReturn(false);
        when(mWifiNS.hasSufficientLinkQuality(eq(mWifiInfo))).thenReturn(true);
        when(mWifiNS.hasInternetOrExpectNoInternet(eq(mWifiInfo))).thenReturn(true);
        when(mClock.getElapsedSinceBootMillis()).thenReturn(600_000L + 1L);
        mWifiConnectivityManager.handleScreenStateChanged(true);
        // Set WiFi to connected state to trigger periodic scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_CONNECTED);
        verify(mWifiScanner, times(2)).startScan(anyObject(), anyObject(), anyObject(),
                anyObject());

        // Verify case 3
        when(mWifiNS.isNetworkSufficient(eq(mWifiInfo))).thenReturn(false);
        when(mWifiNS.hasActiveStream(eq(mWifiInfo))).thenReturn(false);
        when(mWifiNS.hasSufficientLinkQuality(eq(mWifiInfo))).thenReturn(true);
        when(mWifiNS.hasInternetOrExpectNoInternet(eq(mWifiInfo))).thenReturn(false);
        when(mClock.getElapsedSinceBootMillis()).thenReturn(0L);
        mWifiConnectivityManager.handleScreenStateChanged(true);
        // Set WiFi to connected state to trigger periodic scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_CONNECTED);
        verify(mWifiScanner, times(2)).startScan(anyObject(), anyObject(), anyObject(),
                anyObject());
    }

    /**
     * Verify that we perform partial scan when the current RSSI is low,
     * Tx/Rx success rates are high, and when the currently connected network is present
     * in scan cache in WifiConfigManager.
     * WifiConnectivityManager does partial scan only when firmware roaming is not supported.
     *
     * Expected behavior: WifiConnectivityManager does partial scan.
     */
    @Test
    public void checkPartialScanRequestedWithLowRssiAndActiveStreamWithoutFwRoaming() {
        when(mWifiNS.isNetworkSufficient(eq(mWifiInfo))).thenReturn(false);
        when(mWifiNS.hasActiveStream(eq(mWifiInfo))).thenReturn(true);
        when(mWifiNS.hasSufficientLinkQuality(eq(mWifiInfo))).thenReturn(false);
        when(mWifiNS.hasInternetOrExpectNoInternet(eq(mWifiInfo))).thenReturn(true);

        mResources.setInteger(
                R.integer.config_wifi_framework_associated_partial_scan_max_num_active_channels,
                10);

        WifiConfiguration configuration = WifiConfigurationTestUtil.createOpenNetwork();
        configuration.networkId = TEST_CONNECTED_NETWORK_ID;
        when(mWifiConfigManager.getConfiguredNetwork(TEST_CONNECTED_NETWORK_ID))
                .thenReturn(configuration);
        List<Integer> channelList = linkScoreCardFreqsToNetwork(configuration).get(0);
        when(mClientModeImpl.getCurrentWifiConfiguration())
                .thenReturn(configuration);

        when(mWifiConnectivityHelper.isFirmwareRoamingSupported()).thenReturn(false);

        doAnswer(new AnswerWithArguments() {
            public void answer(ScanSettings settings, Executor executor, ScanListener listener,
                    WorkSource workSource) throws Exception {
                assertEquals(settings.band, WifiScanner.WIFI_BAND_UNSPECIFIED);
                assertEquals(settings.channels.length, channelList.size());
                for (int chanIdx = 0; chanIdx < settings.channels.length; chanIdx++) {
                    assertTrue(channelList.contains(settings.channels[chanIdx].frequency));
                }
            }}).when(mWifiScanner).startScan(anyObject(), anyObject(), anyObject(), anyObject());

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Set WiFi to connected state to trigger periodic scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_CONNECTED);

        verify(mWifiScanner).startScan(anyObject(), anyObject(), anyObject(), anyObject());
    }

    /**
     * Verify that we perform partial scan when the current RSSI is high,
     * Tx/Rx success rates are low, and when the currently connected network is present
     * in scan cache in WifiConfigManager.
     * WifiConnectivityManager does partial scan only when firmware roaming is not supported.
     *
     * Expected behavior: WifiConnectivityManager does partial scan.
     */
    @Test
    public void checkPartialScanRequestedWithHighRssiNoActiveStreamWithoutFwRoaming() {
        when(mWifiNS.isNetworkSufficient(eq(mWifiInfo))).thenReturn(false);
        when(mWifiNS.hasActiveStream(eq(mWifiInfo))).thenReturn(false);
        when(mWifiNS.hasSufficientLinkQuality(eq(mWifiInfo))).thenReturn(true);
        when(mWifiNS.hasInternetOrExpectNoInternet(eq(mWifiInfo))).thenReturn(true);

        mResources.setInteger(
                R.integer.config_wifi_framework_associated_partial_scan_max_num_active_channels,
                10);

        WifiConfiguration configuration = WifiConfigurationTestUtil.createOpenNetwork();
        configuration.networkId = TEST_CONNECTED_NETWORK_ID;
        when(mWifiConfigManager.getConfiguredNetwork(TEST_CONNECTED_NETWORK_ID))
                .thenReturn(configuration);
        List<Integer> channelList = linkScoreCardFreqsToNetwork(configuration).get(0);

        when(mClientModeImpl.getCurrentWifiConfiguration())
                .thenReturn(configuration);
        when(mWifiConnectivityHelper.isFirmwareRoamingSupported()).thenReturn(false);

        doAnswer(new AnswerWithArguments() {
            public void answer(ScanSettings settings, Executor executor, ScanListener listener,
                               WorkSource workSource) throws Exception {
                assertEquals(settings.band, WifiScanner.WIFI_BAND_UNSPECIFIED);
                assertEquals(settings.channels.length, channelList.size());
                for (int chanIdx = 0; chanIdx < settings.channels.length; chanIdx++) {
                    assertTrue(channelList.contains(settings.channels[chanIdx].frequency));
                }
            }}).when(mWifiScanner).startScan(anyObject(), anyObject(), anyObject(), anyObject());

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Set WiFi to connected state to trigger periodic scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_CONNECTED);

        verify(mWifiScanner).startScan(anyObject(), anyObject(), anyObject(), anyObject());
    }


    /**
     * Verify that we fall back to full band scan when the currently connected network's tx/rx
     * success rate is high, RSSI is also high but the currently connected network
     * is not present in scan cache in WifiConfigManager.
     * This is simulated by returning an empty hashset in |makeChannelList|.
     *
     * Expected behavior: WifiConnectivityManager does full band scan.
     */
    @Test
    public void checkSingleScanSettingsWhenConnectedWithHighDataRateNotInCache() {
        when(mWifiNS.isNetworkSufficient(eq(mWifiInfo))).thenReturn(true);
        when(mWifiNS.hasActiveStream(eq(mWifiInfo))).thenReturn(true);
        when(mWifiNS.hasSufficientLinkQuality(eq(mWifiInfo))).thenReturn(true);
        when(mWifiNS.hasInternetOrExpectNoInternet(eq(mWifiInfo))).thenReturn(true);

        WifiConfiguration configuration = WifiConfigurationTestUtil.createOpenNetwork();
        configuration.networkId = TEST_CONNECTED_NETWORK_ID;
        when(mWifiConfigManager.getConfiguredNetwork(TEST_CONNECTED_NETWORK_ID))
                .thenReturn(configuration);
        List<Integer> channelList = linkScoreCardFreqsToNetwork(configuration).get(0);

        when(mClientModeImpl.getCurrentWifiConfiguration())
                .thenReturn(new WifiConfiguration());

        doAnswer(new AnswerWithArguments() {
            public void answer(ScanSettings settings, Executor executor, ScanListener listener,
                    WorkSource workSource) throws Exception {
                assertEquals(settings.band, WifiScanner.WIFI_BAND_ALL);
                assertNull(settings.channels);
            }}).when(mWifiScanner).startScan(anyObject(), anyObject(), anyObject(), anyObject());

        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Set WiFi to connected state to trigger periodic scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_CONNECTED);

        verify(mWifiScanner).startScan(anyObject(), anyObject(), anyObject(), anyObject());
    }

    /**
     *  Verify that we retry connectivity scan up to MAX_SCAN_RESTART_ALLOWED times
     *  when Wifi somehow gets into a bad state and fails to scan.
     *
     * Expected behavior: WifiConnectivityManager schedules connectivity scan
     * MAX_SCAN_RESTART_ALLOWED times.
     */
    @Test
    public void checkMaximumScanRetry() {
        // Set screen to ON
        mWifiConnectivityManager.handleScreenStateChanged(true);

        doAnswer(new AnswerWithArguments() {
            public void answer(ScanSettings settings, Executor executor, ScanListener listener,
                    WorkSource workSource) throws Exception {
                listener.onFailure(-1, "ScanFailure");
            }}).when(mWifiScanner).startScan(anyObject(), anyObject(), anyObject(), anyObject());

        // Set WiFi to disconnected state to trigger the single scan based periodic scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        // Fire the alarm timer 2x timers
        for (int i = 0; i < (WifiConnectivityManager.MAX_SCAN_RESTART_ALLOWED * 2); i++) {
            mAlarmManager.dispatch(WifiConnectivityManager.RESTART_SINGLE_SCAN_TIMER_TAG);
            mLooper.dispatchAll();
        }

        // Verify that the connectivity scan has been retried for MAX_SCAN_RESTART_ALLOWED
        // times. Note, WifiScanner.startScan() is invoked MAX_SCAN_RESTART_ALLOWED + 1 times.
        // The very first scan is the initial one, and the other MAX_SCAN_RESTART_ALLOWED
        // are the retrial ones.
        verify(mWifiScanner, times(WifiConnectivityManager.MAX_SCAN_RESTART_ALLOWED + 1)).startScan(
                anyObject(), anyObject(), anyObject(), anyObject());
    }

    /**
     * Verify that a successful scan result resets scan retry counter
     *
     * Steps
     * 1. Trigger a scan that fails
     * 2. Let the retry succeed
     * 3. Trigger a scan again and have it and all subsequent retries fail
     * 4. Verify that there are MAX_SCAN_RESTART_ALLOWED + 3 startScan calls. (2 are from the
     * original scans, and MAX_SCAN_RESTART_ALLOWED + 1 from retries)
     */
    @Test
    public void verifyScanFailureCountIsResetAfterOnResult() {
        // Setup WifiScanner to fail
        doAnswer(new AnswerWithArguments() {
            public void answer(ScanSettings settings, Executor executor, ScanListener listener,
                    WorkSource workSource) throws Exception {
                listener.onFailure(-1, "ScanFailure");
            }}).when(mWifiScanner).startScan(anyObject(), anyObject(), anyObject(), anyObject());

        mWifiConnectivityManager.forceConnectivityScan(null);
        // make the retry succeed
        doAnswer(new AnswerWithArguments() {
            public void answer(ScanSettings settings, Executor executor, ScanListener listener,
                    WorkSource workSource) throws Exception {
                listener.onResults(null);
            }}).when(mWifiScanner).startScan(anyObject(), anyObject(), anyObject(), anyObject());
        mAlarmManager.dispatch(WifiConnectivityManager.RESTART_SINGLE_SCAN_TIMER_TAG);
        mLooper.dispatchAll();

        // Verify that startScan is called once for the original scan, plus once for the retry.
        // The successful retry should have now cleared the restart count
        verify(mWifiScanner, times(2)).startScan(
                anyObject(), anyObject(), anyObject(), anyObject());

        // Now force a new scan and verify we retry MAX_SCAN_RESTART_ALLOWED times
        doAnswer(new AnswerWithArguments() {
            public void answer(ScanSettings settings, Executor executor, ScanListener listener,
                    WorkSource workSource) throws Exception {
                listener.onFailure(-1, "ScanFailure");
            }}).when(mWifiScanner).startScan(anyObject(), anyObject(), anyObject(), anyObject());
        mWifiConnectivityManager.forceConnectivityScan(null);
        // Fire the alarm timer 2x timers
        for (int i = 0; i < (WifiConnectivityManager.MAX_SCAN_RESTART_ALLOWED * 2); i++) {
            mAlarmManager.dispatch(WifiConnectivityManager.RESTART_SINGLE_SCAN_TIMER_TAG);
            mLooper.dispatchAll();
        }

        // Verify that the connectivity scan has been retried for MAX_SCAN_RESTART_ALLOWED + 3
        // times. Note, WifiScanner.startScan() is invoked 2 times by the first part of this test,
        // and additionally MAX_SCAN_RESTART_ALLOWED + 1 times from forceConnectivityScan and
        // subsequent retries.
        verify(mWifiScanner, times(WifiConnectivityManager.MAX_SCAN_RESTART_ALLOWED + 3)).startScan(
                anyObject(), anyObject(), anyObject(), anyObject());
    }

    /**
     * Listen to scan results not requested by WifiConnectivityManager and
     * act on them.
     *
     * Expected behavior: WifiConnectivityManager calls
     * ClientModeImpl.startConnectToNetwork() with the
     * expected candidate network ID and BSSID.
     */
    @Test
    public void listenToAllSingleScanResults() {
        ScanSettings settings = new ScanSettings();
        ScanListener scanListener = mock(ScanListener.class);

        // Request a single scan outside of WifiConnectivityManager.
        mWifiScanner.startScan(settings, mock(Executor.class), scanListener, WIFI_WORK_SOURCE);

        // Verify that WCM receives the scan results and initiates a connection
        // to the network.
        verify(mClientModeImpl).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
    }

    /**
     *  Verify that a forced connectivity scan waits for full band scan
     *  results.
     *
     * Expected behavior: WifiConnectivityManager doesn't invoke
     * ClientModeImpl.startConnectToNetwork() when full band scan
     * results are not available.
     */
    @Test
    public void waitForFullBandScanResults() {
        // Set WiFi to connected state.
        setWifiStateConnected();

        // Set up as partial scan results.
        when(mScanData.getBandScanned()).thenReturn(WifiScanner.WIFI_BAND_5_GHZ);

        // Force a connectivity scan which enables WifiConnectivityManager
        // to wait for full band scan results.
        mWifiConnectivityManager.forceConnectivityScan(WIFI_WORK_SOURCE);

        // No roaming because no full band scan results.
        verify(mClientModeImpl, times(0)).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);

        // Set up as full band scan results.
        when(mScanData.getBandScanned()).thenReturn(WifiScanner.WIFI_BAND_ALL);

        // Force a connectivity scan which enables WifiConnectivityManager
        // to wait for full band scan results.
        mWifiConnectivityManager.forceConnectivityScan(WIFI_WORK_SOURCE);

        // Roaming attempt because full band scan results are available.
        verify(mClientModeImpl).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
    }

    /**
     * Verify when new scanResults are available, UserDisabledList will be updated.
     */
    @Test
    public void verifyUserDisabledListUpdated() {
        mResources.setBoolean(
                R.bool.config_wifi_framework_use_single_radio_chain_scan_results_network_selection,
                true);
        verify(mWifiConfigManager, never()).updateUserDisabledList(anyList());
        Set<String> updateNetworks = new HashSet<>();
        mScanData = createScanDataWithDifferentRadioChainInfos();
        int i = 0;
        for (ScanResult scanResult : mScanData.getResults()) {
            scanResult.SSID = TEST_SSID + i;
            updateNetworks.add(ScanResultUtil.createQuotedSSID(scanResult.SSID));
            i++;
        }
        updateNetworks.add(TEST_FQDN);
        mScanData.getResults()[0].setFlag(ScanResult.FLAG_PASSPOINT_NETWORK);
        HashMap<String, Map<Integer, List<ScanResult>>> passpointNetworks = new HashMap<>();
        passpointNetworks.put(TEST_FQDN, new HashMap<>());
        when(mPasspointManager.getAllMatchingPasspointProfilesForScanResults(any()))
                .thenReturn(passpointNetworks);

        mWifiConnectivityManager.forceConnectivityScan(WIFI_WORK_SOURCE);
        ArgumentCaptor<ArrayList<String>> listArgumentCaptor =
                ArgumentCaptor.forClass(ArrayList.class);
        verify(mWifiConfigManager).updateUserDisabledList(listArgumentCaptor.capture());
        assertEquals(updateNetworks, new HashSet<>(listArgumentCaptor.getValue()));
    }

    /**
     *  Verify that a blacklisted BSSID becomes available only after
     *  BSSID_BLACKLIST_EXPIRE_TIME_MS.
     */
    @Test
    public void verifyBlacklistRefreshedAfterScanResults() {
        when(mWifiConnectivityHelper.isFirmwareRoamingSupported()).thenReturn(true);

        InOrder inOrder = inOrder(mBssidBlocklistMonitor);
        // Force a connectivity scan
        inOrder.verify(mBssidBlocklistMonitor, never())
                .updateAndGetBssidBlocklistForSsid(anyString());
        mWifiConnectivityManager.forceConnectivityScan(WIFI_WORK_SOURCE);

        inOrder.verify(mBssidBlocklistMonitor).tryEnablingBlockedBssids(any());
        inOrder.verify(mBssidBlocklistMonitor).updateAndGetBssidBlocklistForSsid(anyString());
    }

    /**
     *  Verify that BSSID blacklist gets cleared when exiting Wifi client mode.
     */
    @Test
    public void clearBssidBlocklistWhenExitingWifiClientMode() {
        when(mWifiConnectivityHelper.isFirmwareRoamingSupported()).thenReturn(true);

        // Verify the BSSID blacklist is cleared at start up.
        verify(mBssidBlocklistMonitor).clearBssidBlocklist();
        // Exit Wifi client mode.
        mWifiConnectivityManager.setWifiEnabled(false);

        // Verify the BSSID blacklist is cleared again.
        verify(mBssidBlocklistMonitor, times(2)).clearBssidBlocklist();
        // Verify WifiNetworkSelector is informed of the disable.
        verify(mWifiNS).resetOnDisable();
    }

    /**
     *  Verify that BSSID blacklist gets cleared when preparing for a forced connection
     *  initiated by user/app.
     */
    @Test
    public void clearBssidBlocklistWhenPreparingForForcedConnection() {
        when(mWifiConnectivityHelper.isFirmwareRoamingSupported()).thenReturn(true);
        // Prepare for a forced connection attempt.
        WifiConfiguration currentNetwork = generateWifiConfig(
                0, CANDIDATE_NETWORK_ID, CANDIDATE_SSID, false, true, null, null);
        when(mWifiConfigManager.getConfiguredNetwork(anyInt())).thenReturn(currentNetwork);
        mWifiConnectivityManager.prepareForForcedConnection(1);
        verify(mBssidBlocklistMonitor).clearBssidBlocklistForSsid(CANDIDATE_SSID);
    }

    /**
     * When WifiConnectivityManager is on and Wifi client mode is enabled, framework
     * queries firmware via WifiConnectivityHelper to check if firmware roaming is
     * supported and its capability.
     *
     * Expected behavior: WifiConnectivityManager#setWifiEnabled calls into
     * WifiConnectivityHelper#getFirmwareRoamingInfo
     */
    @Test
    public void verifyGetFirmwareRoamingInfoIsCalledWhenEnableWiFiAndWcmOn() {
        // WifiConnectivityManager is on by default
        mWifiConnectivityManager.setWifiEnabled(true);
        verify(mWifiConnectivityHelper).getFirmwareRoamingInfo();
    }

    /**
     * When WifiConnectivityManager is off,  verify that framework does not
     * query firmware via WifiConnectivityHelper to check if firmware roaming is
     * supported and its capability when enabling Wifi client mode.
     *
     * Expected behavior: WifiConnectivityManager#setWifiEnabled does not call into
     * WifiConnectivityHelper#getFirmwareRoamingInfo
     */
    @Test
    public void verifyGetFirmwareRoamingInfoIsNotCalledWhenEnableWiFiAndWcmOff() {
        reset(mWifiConnectivityHelper);
        mWifiConnectivityManager.setAutoJoinEnabledExternal(false);
        mWifiConnectivityManager.setWifiEnabled(true);
        verify(mWifiConnectivityHelper, times(0)).getFirmwareRoamingInfo();
    }

    /*
     * Firmware supports controlled roaming.
     * Connect to a network which doesn't have a config specified BSSID.
     *
     * Expected behavior: WifiConnectivityManager calls
     * ClientModeImpl.startConnectToNetwork() with the
     * expected candidate network ID, and the BSSID value should be
     * 'any' since firmware controls the roaming.
     */
    @Test
    public void useAnyBssidToConnectWhenFirmwareRoamingOnAndConfigHasNoBssidSpecified() {
        // Firmware controls roaming
        when(mWifiConnectivityHelper.isFirmwareRoamingSupported()).thenReturn(true);

        // Set screen to on
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Set WiFi to disconnected state
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        verify(mClientModeImpl).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, ClientModeImpl.SUPPLICANT_BSSID_ANY);
    }

    /*
     * Firmware supports controlled roaming.
     * Connect to a network which has a config specified BSSID.
     *
     * Expected behavior: WifiConnectivityManager calls
     * ClientModeImpl.startConnectToNetwork() with the
     * expected candidate network ID, and the BSSID value should be
     * the config specified one.
     */
    @Test
    public void useConfigSpecifiedBssidToConnectWhenFirmwareRoamingOn() {
        // Firmware controls roaming
        when(mWifiConnectivityHelper.isFirmwareRoamingSupported()).thenReturn(true);

        // Set up the candidate configuration such that it has a BSSID specified.
        WifiConfiguration candidate = generateWifiConfig(
                0, CANDIDATE_NETWORK_ID, CANDIDATE_SSID, false, true, null, null);
        candidate.BSSID = CANDIDATE_BSSID; // config specified
        ScanResult candidateScanResult = new ScanResult();
        candidateScanResult.SSID = CANDIDATE_SSID;
        candidateScanResult.BSSID = CANDIDATE_BSSID;
        candidate.getNetworkSelectionStatus().setCandidate(candidateScanResult);
        when(mWifiNS.selectNetwork(any())).thenReturn(candidate);

        // Set screen to on
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Set WiFi to disconnected state
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        verify(mClientModeImpl).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
    }

    /*
     * Firmware does not support controlled roaming.
     * Connect to a network which doesn't have a config specified BSSID.
     *
     * Expected behavior: WifiConnectivityManager calls
     * ClientModeImpl.startConnectToNetwork() with the expected candidate network ID,
     * and the BSSID value should be the candidate scan result specified.
     */
    @Test
    public void useScanResultBssidToConnectWhenFirmwareRoamingOffAndConfigHasNoBssidSpecified() {
        // Set screen to on
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Set WiFi to disconnected state
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        verify(mClientModeImpl).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
    }

    /*
     * Firmware does not support controlled roaming.
     * Connect to a network which has a config specified BSSID.
     *
     * Expected behavior: WifiConnectivityManager calls
     * ClientModeImpl.startConnectToNetwork() with the expected candidate network ID,
     * and the BSSID value should be the config specified one.
     */
    @Test
    public void useConfigSpecifiedBssidToConnectionWhenFirmwareRoamingOff() {
        // Set up the candidate configuration such that it has a BSSID specified.
        WifiConfiguration candidate = generateWifiConfig(
                0, CANDIDATE_NETWORK_ID, CANDIDATE_SSID, false, true, null, null);
        candidate.BSSID = CANDIDATE_BSSID; // config specified
        ScanResult candidateScanResult = new ScanResult();
        candidateScanResult.SSID = CANDIDATE_SSID;
        candidateScanResult.BSSID = CANDIDATE_BSSID;
        candidate.getNetworkSelectionStatus().setCandidate(candidateScanResult);
        when(mWifiNS.selectNetwork(any())).thenReturn(candidate);

        // Set screen to on
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Set WiFi to disconnected state
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        verify(mClientModeImpl).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
    }

    /**
     * Firmware does not support controlled roaming.
     * WiFi in connected state, framework triggers roaming.
     *
     * Expected behavior: WifiConnectivityManager invokes
     * ClientModeImpl.startRoamToNetwork().
     */
    @Test
    public void frameworkInitiatedRoaming() {
        // Mock the currently connected network which has the same networkID and
        // SSID as the one to be selected.
        WifiConfiguration currentNetwork = generateWifiConfig(
                0, CANDIDATE_NETWORK_ID, CANDIDATE_SSID, false, true, null, null);
        when(mWifiConfigManager.getConfiguredNetwork(anyInt())).thenReturn(currentNetwork);

        // Set WiFi to connected state
        setWifiStateConnected();

        // Set screen to on
        mWifiConnectivityManager.handleScreenStateChanged(true);

        verify(mClientModeImpl).startRoamToNetwork(eq(CANDIDATE_NETWORK_ID),
                mCandidateScanResultCaptor.capture());
        assertEquals(mCandidateScanResultCaptor.getValue().BSSID, CANDIDATE_BSSID);
    }

    /**
     * Firmware supports controlled roaming.
     * WiFi in connected state, framework does not trigger roaming
     * as it's handed off to the firmware.
     *
     * Expected behavior: WifiConnectivityManager doesn't invoke
     * ClientModeImpl.startRoamToNetwork().
     */
    @Test
    public void noFrameworkRoamingIfConnectedAndFirmwareRoamingSupported() {
        // Mock the currently connected network which has the same networkID and
        // SSID as the one to be selected.
        WifiConfiguration currentNetwork = generateWifiConfig(
                0, CANDIDATE_NETWORK_ID, CANDIDATE_SSID, false, true, null, null);
        when(mWifiConfigManager.getConfiguredNetwork(anyInt())).thenReturn(currentNetwork);

        // Firmware controls roaming
        when(mWifiConnectivityHelper.isFirmwareRoamingSupported()).thenReturn(true);

        // Set WiFi to connected state
        setWifiStateConnected();

        // Set screen to on
        mWifiConnectivityManager.handleScreenStateChanged(true);

        verify(mClientModeImpl, times(0)).startRoamToNetwork(anyInt(), anyObject());
    }

    /*
     * Wifi in disconnected state. Drop the connection attempt if the recommended
     * network configuration has a BSSID specified but the scan result BSSID doesn't
     * match it.
     *
     * Expected behavior: WifiConnectivityManager doesn't invoke
     * ClientModeImpl.startConnectToNetwork().
     */
    @Test
    public void dropConnectAttemptIfConfigSpecifiedBssidDifferentFromScanResultBssid() {
        // Set up the candidate configuration such that it has a BSSID specified.
        WifiConfiguration candidate = generateWifiConfig(
                0, CANDIDATE_NETWORK_ID, CANDIDATE_SSID, false, true, null, null);
        candidate.BSSID = CANDIDATE_BSSID; // config specified
        ScanResult candidateScanResult = new ScanResult();
        candidateScanResult.SSID = CANDIDATE_SSID;
        // Set up the scan result BSSID to be different from the config specified one.
        candidateScanResult.BSSID = INVALID_SCAN_RESULT_BSSID;
        candidate.getNetworkSelectionStatus().setCandidate(candidateScanResult);
        when(mWifiNS.selectNetwork(any())).thenReturn(candidate);

        // Set screen to on
        mWifiConnectivityManager.handleScreenStateChanged(true);

        // Set WiFi to disconnected state
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        verify(mClientModeImpl, times(0)).startConnectToNetwork(
                CANDIDATE_NETWORK_ID, Process.WIFI_UID, CANDIDATE_BSSID);
    }

    /*
     * Wifi in connected state. Drop the roaming attempt if the recommended
     * network configuration has a BSSID specified but the scan result BSSID doesn't
     * match it.
     *
     * Expected behavior: WifiConnectivityManager doesn't invoke
     * ClientModeImpl.startRoamToNetwork().
     */
    @Test
    public void dropRoamingAttemptIfConfigSpecifiedBssidDifferentFromScanResultBssid() {
        // Mock the currently connected network which has the same networkID and
        // SSID as the one to be selected.
        WifiConfiguration currentNetwork = generateWifiConfig(
                0, CANDIDATE_NETWORK_ID, CANDIDATE_SSID, false, true, null, null);
        when(mWifiConfigManager.getConfiguredNetwork(anyInt())).thenReturn(currentNetwork);

        // Set up the candidate configuration such that it has a BSSID specified.
        WifiConfiguration candidate = generateWifiConfig(
                0, CANDIDATE_NETWORK_ID, CANDIDATE_SSID, false, true, null, null);
        candidate.BSSID = CANDIDATE_BSSID; // config specified
        ScanResult candidateScanResult = new ScanResult();
        candidateScanResult.SSID = CANDIDATE_SSID;
        // Set up the scan result BSSID to be different from the config specified one.
        candidateScanResult.BSSID = INVALID_SCAN_RESULT_BSSID;
        candidate.getNetworkSelectionStatus().setCandidate(candidateScanResult);
        when(mWifiNS.selectNetwork(any())).thenReturn(candidate);

        // Set WiFi to connected state
        setWifiStateConnected();

        // Set screen to on
        mWifiConnectivityManager.handleScreenStateChanged(true);

        verify(mClientModeImpl, times(0)).startRoamToNetwork(anyInt(), anyObject());
    }

    /**
     *  Dump local log buffer.
     *
     * Expected behavior: Logs dumped from WifiConnectivityManager.dump()
     * contain the message we put in mLocalLog.
     */
    @Test
    public void dumpLocalLog() {
        final String localLogMessage = "This is a message from the test";
        mLocalLog.log(localLogMessage);

        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        mWifiConnectivityManager.dump(new FileDescriptor(), pw, new String[]{});
        assertTrue(sw.toString().contains(localLogMessage));
    }

    /**
     *  Dump ONA controller.
     *
     * Expected behavior: {@link OpenNetworkNotifier#dump(FileDescriptor, PrintWriter,
     * String[])} is invoked.
     */
    @Test
    public void dumpNotificationController() {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        mWifiConnectivityManager.dump(new FileDescriptor(), pw, new String[]{});

        verify(mOpenNetworkNotifier).dump(any(), any(), any());
    }

    /**
     * Create scan data with different radio chain infos:
     * First scan result has null radio chain info (No DBS support).
     * Second scan result has empty radio chain info (No DBS support).
     * Third scan result has 1 radio chain info (DBS scan).
     * Fourth scan result has 2 radio chain info (non-DBS scan).
     */
    private ScanData createScanDataWithDifferentRadioChainInfos() {
        // Create 4 scan results.
        ScanData[] scanDatas =
                ScanTestUtil.createScanDatas(new int[][]{{5150, 5175, 2412, 2400}}, new int[]{0});
        // WCM barfs if the scan result does not have an IE.
        scanDatas[0].getResults()[0].informationElements = new InformationElement[0];
        scanDatas[0].getResults()[1].informationElements = new InformationElement[0];
        scanDatas[0].getResults()[2].informationElements = new InformationElement[0];
        scanDatas[0].getResults()[3].informationElements = new InformationElement[0];
        scanDatas[0].getResults()[0].radioChainInfos = null;
        scanDatas[0].getResults()[1].radioChainInfos = new ScanResult.RadioChainInfo[0];
        scanDatas[0].getResults()[2].radioChainInfos = new ScanResult.RadioChainInfo[1];
        scanDatas[0].getResults()[3].radioChainInfos = new ScanResult.RadioChainInfo[2];

        return scanDatas[0];
    }

    /**
     * If |config_wifi_framework_use_single_radio_chain_scan_results_network_selection| flag is
     * false, WifiConnectivityManager should filter scan results which contain scans from a single
     * radio chain (i.e DBS scan).
     * Note:
     * a) ScanResult with no radio chain indicates a lack of DBS support on the device.
     * b) ScanResult with 2 radio chain info indicates a scan done using both the radio chains
     * on a DBS supported device.
     *
     * Expected behavior: WifiConnectivityManager invokes
     * {@link WifiNetworkSelector#selectNetwork(List, HashSet, WifiInfo, boolean, boolean, boolean)}
     * after filtering out the scan results obtained via DBS scan.
     */
    @Test
    public void filterScanResultsWithOneRadioChainInfoForNetworkSelectionIfConfigDisabled() {
        mResources.setBoolean(
                R.bool.config_wifi_framework_use_single_radio_chain_scan_results_network_selection,
                false);
        when(mWifiNS.selectNetwork(any())).thenReturn(null);
        mWifiConnectivityManager = createConnectivityManager();

        mScanData = createScanDataWithDifferentRadioChainInfos();

        // Capture scan details which were sent to network selector.
        final List<ScanDetail> capturedScanDetails = new ArrayList<>();
        doAnswer(new AnswerWithArguments() {
            public List<WifiCandidates.Candidate> answer(
                    List<ScanDetail> scanDetails, Set<String> bssidBlacklist, WifiInfo wifiInfo,
                    boolean connected, boolean disconnected, boolean untrustedNetworkAllowed)
                    throws Exception {
                capturedScanDetails.addAll(scanDetails);
                return null;
            }}).when(mWifiNS).getCandidatesFromScan(
                    any(), any(), any(), anyBoolean(), anyBoolean(), anyBoolean());

        mWifiConnectivityManager.setTrustedConnectionAllowed(true);
        // Set WiFi to disconnected state with screen on which triggers a scan immediately.
        mWifiConnectivityManager.setWifiEnabled(true);
        mWifiConnectivityManager.handleScreenStateChanged(true);
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        // We should have filtered out the 3rd scan result.
        assertEquals(3, capturedScanDetails.size());
        List<ScanResult> capturedScanResults =
                capturedScanDetails.stream().map(ScanDetail::getScanResult)
                        .collect(Collectors.toList());

        assertEquals(3, capturedScanResults.size());
        assertTrue(capturedScanResults.contains(mScanData.getResults()[0]));
        assertTrue(capturedScanResults.contains(mScanData.getResults()[1]));
        assertFalse(capturedScanResults.contains(mScanData.getResults()[2]));
        assertTrue(capturedScanResults.contains(mScanData.getResults()[3]));
    }

    /**
     * If |config_wifi_framework_use_single_radio_chain_scan_results_network_selection| flag is
     * true, WifiConnectivityManager should not filter scan results which contain scans from a
     * single radio chain (i.e DBS scan).
     * Note:
     * a) ScanResult with no radio chain indicates a lack of DBS support on the device.
     * b) ScanResult with 2 radio chain info indicates a scan done using both the radio chains
     * on a DBS supported device.
     *
     * Expected behavior: WifiConnectivityManager invokes
     * {@link WifiNetworkSelector#selectNetwork(List, HashSet, WifiInfo, boolean, boolean, boolean)}
     * after filtering out the scan results obtained via DBS scan.
     */
    @Test
    public void dontFilterScanResultsWithOneRadioChainInfoForNetworkSelectionIfConfigEnabled() {
        mResources.setBoolean(
                R.bool.config_wifi_framework_use_single_radio_chain_scan_results_network_selection,
                true);
        when(mWifiNS.selectNetwork(any())).thenReturn(null);
        mWifiConnectivityManager = createConnectivityManager();

        mScanData = createScanDataWithDifferentRadioChainInfos();

        // Capture scan details which were sent to network selector.
        final List<ScanDetail> capturedScanDetails = new ArrayList<>();
        doAnswer(new AnswerWithArguments() {
            public List<WifiCandidates.Candidate> answer(
                    List<ScanDetail> scanDetails, Set<String> bssidBlacklist, WifiInfo wifiInfo,
                    boolean connected, boolean disconnected, boolean untrustedNetworkAllowed)
                    throws Exception {
                capturedScanDetails.addAll(scanDetails);
                return null;
            }}).when(mWifiNS).getCandidatesFromScan(
                any(), any(), any(), anyBoolean(), anyBoolean(), anyBoolean());

        mWifiConnectivityManager.setTrustedConnectionAllowed(true);
        // Set WiFi to disconnected state with screen on which triggers a scan immediately.
        mWifiConnectivityManager.setWifiEnabled(true);
        mWifiConnectivityManager.handleScreenStateChanged(true);
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        // We should not filter any of the scan results.
        assertEquals(4, capturedScanDetails.size());
        List<ScanResult> capturedScanResults =
                capturedScanDetails.stream().map(ScanDetail::getScanResult)
                        .collect(Collectors.toList());

        assertEquals(4, capturedScanResults.size());
        assertTrue(capturedScanResults.contains(mScanData.getResults()[0]));
        assertTrue(capturedScanResults.contains(mScanData.getResults()[1]));
        assertTrue(capturedScanResults.contains(mScanData.getResults()[2]));
        assertTrue(capturedScanResults.contains(mScanData.getResults()[3]));
    }

    /**
     * Verify the various auto join enable/disable sequences when auto join is disabled externally.
     *
     * Expected behavior: Autojoin is turned on as a long as there is
     *  - Auto join is enabled externally
     *    And
     *  - No specific network request being processed.
     *    And
     *    - Pending generic Network request for trusted wifi connection.
     *      OR
     *    - Pending generic Network request for untrused wifi connection.
     */
    @Test
    public void verifyEnableAndDisableAutoJoinWhenExternalAutoJoinIsDisabled() {
        mWifiConnectivityManager = createConnectivityManager();

        // set wifi on & disconnected to trigger pno scans when auto-join is enabled.
        mWifiConnectivityManager.setWifiEnabled(true);
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        // Disable externally.
        mWifiConnectivityManager.setAutoJoinEnabledExternal(false);

        // Enable trusted connection. This should NOT trigger a pno scan for auto-join.
        mWifiConnectivityManager.setTrustedConnectionAllowed(true);
        verify(mWifiScanner, never()).startDisconnectedPnoScan(any(), any(), any(), any());

        // End of processing a specific request. This should NOT trigger a new pno scan for
        // auto-join.
        mWifiConnectivityManager.setSpecificNetworkRequestInProgress(false);
        verify(mWifiScanner, never()).startDisconnectedPnoScan(any(), any(), any(), any());

        // Enable untrusted connection. This should NOT trigger a pno scan for auto-join.
        mWifiConnectivityManager.setUntrustedConnectionAllowed(true);
        verify(mWifiScanner, never()).startDisconnectedPnoScan(any(), any(), any(), any());
    }

    /**
     * Verify the various auto join enable/disable sequences when auto join is enabled externally.
     *
     * Expected behavior: Autojoin is turned on as a long as there is
     *  - Auto join is enabled externally
     *    And
     *  - No specific network request being processed.
     *    And
     *    - Pending generic Network request for trusted wifi connection.
     *      OR
     *    - Pending generic Network request for untrused wifi connection.
     */
    @Test
    public void verifyEnableAndDisableAutoJoin() {
        mWifiConnectivityManager = createConnectivityManager();

        // set wifi on & disconnected to trigger pno scans when auto-join is enabled.
        mWifiConnectivityManager.setWifiEnabled(true);
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);

        // Enable trusted connection. This should trigger a pno scan for auto-join.
        mWifiConnectivityManager.setTrustedConnectionAllowed(true);
        verify(mWifiScanner).startDisconnectedPnoScan(any(), any(), any(), any());

        // Start of processing a specific request. This should stop any pno scan for auto-join.
        mWifiConnectivityManager.setSpecificNetworkRequestInProgress(true);
        verify(mWifiScanner).stopPnoScan(any());

        // End of processing a specific request. This should now trigger a new pno scan for
        // auto-join.
        mWifiConnectivityManager.setSpecificNetworkRequestInProgress(false);
        verify(mWifiScanner, times(2)).startDisconnectedPnoScan(any(), any(), any(), any());

        // Disable trusted connection. This should stop any pno scan for auto-join.
        mWifiConnectivityManager.setTrustedConnectionAllowed(false);
        verify(mWifiScanner, times(2)).stopPnoScan(any());

        // Enable untrusted connection. This should trigger a pno scan for auto-join.
        mWifiConnectivityManager.setUntrustedConnectionAllowed(true);
        verify(mWifiScanner, times(3)).startDisconnectedPnoScan(any(), any(), any(), any());
    }

    /**
     * Change device mobility state in the middle of a PNO scan. PNO scan should stop, then restart
     * with the updated scan period.
     */
    @Test
    public void changeDeviceMobilityStateDuringScan() {
        mWifiConnectivityManager.setWifiEnabled(true);

        // starts a PNO scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
        mWifiConnectivityManager.setTrustedConnectionAllowed(true);

        ArgumentCaptor<ScanSettings> scanSettingsCaptor = ArgumentCaptor.forClass(
                ScanSettings.class);
        InOrder inOrder = inOrder(mWifiScanner);

        inOrder.verify(mWifiScanner).startDisconnectedPnoScan(
                scanSettingsCaptor.capture(), any(), any(), any());
        assertEquals(scanSettingsCaptor.getValue().periodInMs, MOVING_PNO_SCAN_INTERVAL_MILLIS);

        // initial connectivity state uses moving PNO scan interval, now set it to stationary
        mWifiConnectivityManager.setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_STATIONARY);

        inOrder.verify(mWifiScanner).stopPnoScan(any());
        inOrder.verify(mWifiScanner).startDisconnectedPnoScan(
                scanSettingsCaptor.capture(), any(), any(), any());
        assertEquals(scanSettingsCaptor.getValue().periodInMs, STATIONARY_PNO_SCAN_INTERVAL_MILLIS);
        verify(mScoringParams, times(2)).getEntryRssi(ScanResult.BAND_6_GHZ_START_FREQ_MHZ);
        verify(mScoringParams, times(2)).getEntryRssi(ScanResult.BAND_5_GHZ_START_FREQ_MHZ);
        verify(mScoringParams, times(2)).getEntryRssi(ScanResult.BAND_24_GHZ_START_FREQ_MHZ);
    }

    /**
     * Change device mobility state in the middle of a PNO scan, but it is changed to another
     * mobility state with the same scan period. Original PNO scan should continue.
     */
    @Test
    public void changeDeviceMobilityStateDuringScanWithSameScanPeriod() {
        mWifiConnectivityManager.setWifiEnabled(true);

        // starts a PNO scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
        mWifiConnectivityManager.setTrustedConnectionAllowed(true);

        ArgumentCaptor<ScanSettings> scanSettingsCaptor = ArgumentCaptor.forClass(
                ScanSettings.class);
        InOrder inOrder = inOrder(mWifiScanner);
        inOrder.verify(mWifiScanner, never()).stopPnoScan(any());
        inOrder.verify(mWifiScanner).startDisconnectedPnoScan(
                scanSettingsCaptor.capture(), any(), any(), any());
        assertEquals(scanSettingsCaptor.getValue().periodInMs, MOVING_PNO_SCAN_INTERVAL_MILLIS);

        mWifiConnectivityManager.setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_LOW_MVMT);

        inOrder.verifyNoMoreInteractions();
    }

    /**
     * Device is already connected, setting device mobility state should do nothing since no PNO
     * scans are running. Then, when PNO scan is started afterwards, should use the new scan period.
     */
    @Test
    public void setDeviceMobilityStateBeforePnoScan() {
        // ensure no PNO scan running
        mWifiConnectivityManager.setWifiEnabled(true);
        setWifiStateConnected();

        // initial connectivity state uses moving PNO scan interval, now set it to stationary
        mWifiConnectivityManager.setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_STATIONARY);

        // no scans should start or stop because no PNO scan is running
        verify(mWifiScanner, never()).startDisconnectedPnoScan(any(), any(), any(), any());
        verify(mWifiScanner, never()).stopPnoScan(any());

        // starts a PNO scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
        mWifiConnectivityManager.setTrustedConnectionAllowed(true);

        ArgumentCaptor<ScanSettings> scanSettingsCaptor = ArgumentCaptor.forClass(
                ScanSettings.class);

        verify(mWifiScanner).startDisconnectedPnoScan(
                scanSettingsCaptor.capture(), any(), any(), any());
        // check that now the PNO scan uses the stationary interval, even though it was set before
        // the PNO scan started
        assertEquals(scanSettingsCaptor.getValue().periodInMs, STATIONARY_PNO_SCAN_INTERVAL_MILLIS);
    }

    /**
     * Tests the metrics collection of PNO scans through changes to device mobility state and
     * starting and stopping of PNO scans.
     */
    @Test
    public void deviceMobilityStateMetricsChangeStateAndStopStart() {
        InOrder inOrder = inOrder(mWifiMetrics);

        mWifiConnectivityManager = createConnectivityManager();
        mWifiConnectivityManager.setWifiEnabled(true);

        // change mobility state while no PNO scans running
        mWifiConnectivityManager.setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_LOW_MVMT);
        inOrder.verify(mWifiMetrics).enterDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_LOW_MVMT);

        // starts a PNO scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
        mWifiConnectivityManager.setTrustedConnectionAllowed(true);
        inOrder.verify(mWifiMetrics).logPnoScanStart();

        // change to High Movement, which has the same scan interval as Low Movement
        mWifiConnectivityManager.setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_HIGH_MVMT);
        inOrder.verify(mWifiMetrics).logPnoScanStop();
        inOrder.verify(mWifiMetrics).enterDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_HIGH_MVMT);
        inOrder.verify(mWifiMetrics).logPnoScanStart();

        // change to Stationary, which has a different scan interval from High Movement
        mWifiConnectivityManager.setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_STATIONARY);
        inOrder.verify(mWifiMetrics).logPnoScanStop();
        inOrder.verify(mWifiMetrics).enterDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_STATIONARY);
        inOrder.verify(mWifiMetrics).logPnoScanStart();

        // stops PNO scan
        mWifiConnectivityManager.setTrustedConnectionAllowed(false);
        inOrder.verify(mWifiMetrics).logPnoScanStop();

        // change mobility state while no PNO scans running
        mWifiConnectivityManager.setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_HIGH_MVMT);
        inOrder.verify(mWifiMetrics).enterDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_HIGH_MVMT);

        inOrder.verifyNoMoreInteractions();
    }

    /**
     *  Verify that WifiChannelUtilization is updated
     */
    @Test
    public void verifyWifiChannelUtilizationRefreshedAfterScanResults() {
        WifiLinkLayerStats llstats = new WifiLinkLayerStats();
        when(mClientModeImpl.getWifiLinkLayerStats()).thenReturn(llstats);

        // Force a connectivity scan
        mWifiConnectivityManager.forceConnectivityScan(WIFI_WORK_SOURCE);

        verify(mWifiChannelUtilization).refreshChannelStatsAndChannelUtilization(
                llstats, WifiChannelUtilization.UNKNOWN_FREQ);
    }

    /**
     *  Verify that WifiChannelUtilization is initialized properly
     */
    @Test
    public void verifyWifiChannelUtilizationInitAfterWifiToggle() {
        verify(mWifiChannelUtilization, times(1)).init(null);
        WifiLinkLayerStats llstats = new WifiLinkLayerStats();
        when(mClientModeImpl.getWifiLinkLayerStats()).thenReturn(llstats);

        mWifiConnectivityManager.setWifiEnabled(false);
        mWifiConnectivityManager.setWifiEnabled(true);
        verify(mWifiChannelUtilization, times(1)).init(llstats);
    }

    /**
     *  Verify that WifiChannelUtilization sets mobility state correctly
     */
    @Test
    public void verifyWifiChannelUtilizationSetMobilityState() {
        WifiLinkLayerStats llstats = new WifiLinkLayerStats();
        when(mClientModeImpl.getWifiLinkLayerStats()).thenReturn(llstats);

        mWifiConnectivityManager.setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_HIGH_MVMT);
        verify(mWifiChannelUtilization).setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_HIGH_MVMT);
        mWifiConnectivityManager.setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_STATIONARY);
        verify(mWifiChannelUtilization).setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_STATIONARY);
    }

    /**
     *  Verify that WifiNetworkSelector sets bluetoothConnected correctly
     */
    @Test
    public void verifyWifiNetworkSelectorSetBluetoothConnected() {
        mWifiConnectivityManager.setBluetoothConnected(true);
        verify(mWifiNS).setBluetoothConnected(true);
        mWifiConnectivityManager.setBluetoothConnected(false);
        verify(mWifiNS).setBluetoothConnected(false);
    }

    /**
     *  Verify that WifiChannelUtilization is updated
     */
    @Test
    public void verifyForceConnectivityScan() {
        // Auto-join enabled
        mWifiConnectivityManager.setAutoJoinEnabledExternal(true);
        mWifiConnectivityManager.forceConnectivityScan(WIFI_WORK_SOURCE);
        verify(mWifiScanner).startScan(any(), any(), any(), any());

        // Auto-join disabled
        mWifiConnectivityManager.setAutoJoinEnabledExternal(false);
        mWifiConnectivityManager.forceConnectivityScan(WIFI_WORK_SOURCE);
        verify(mWifiScanner, times(2)).startScan(any(), any(), any(), any());

        // Wifi disabled, no new scans
        mWifiConnectivityManager.setWifiEnabled(false);
        mWifiConnectivityManager.forceConnectivityScan(WIFI_WORK_SOURCE);
        verify(mWifiScanner, times(2)).startScan(any(), any(), any(), any());
    }

    /**
     * Verify no network is network selection disabled, auto-join disabled using.
     * {@link WifiConnectivityManager#retrievePnoNetworkList()}.
     */
    @Test
    public void testRetrievePnoList() {
        // Create and add 3 networks.
        WifiConfiguration network1 = WifiConfigurationTestUtil.createEapNetwork();
        WifiConfiguration network2 = WifiConfigurationTestUtil.createPskNetwork();
        WifiConfiguration network3 = WifiConfigurationTestUtil.createOpenHiddenNetwork();
        List<WifiConfiguration> networkList = new ArrayList<>();
        networkList.add(network1);
        networkList.add(network2);
        networkList.add(network3);
        mLruConnectionTracker.addNetwork(network3);
        mLruConnectionTracker.addNetwork(network2);
        mLruConnectionTracker.addNetwork(network1);
        when(mWifiConfigManager.getSavedNetworks(anyInt())).thenReturn(networkList);
        // Retrieve the Pno network list & verify.
        List<WifiScanner.PnoSettings.PnoNetwork> pnoNetworks =
                mWifiConnectivityManager.retrievePnoNetworkList();
        verify(mWifiNetworkSuggestionsManager).getAllScanOptimizationSuggestionNetworks();
        assertEquals(3, pnoNetworks.size());
        assertEquals(network1.SSID, pnoNetworks.get(0).ssid);
        assertEquals(network2.SSID, pnoNetworks.get(1).ssid);
        assertEquals(network3.SSID, pnoNetworks.get(2).ssid);

        // Now permanently disable |network3|. This should remove network 3 from the list.
        network3.getNetworkSelectionStatus().setNetworkSelectionStatus(
                WifiConfiguration.NetworkSelectionStatus.NETWORK_SELECTION_TEMPORARY_DISABLED);

        // Retrieve the Pno network list & verify.
        pnoNetworks = mWifiConnectivityManager.retrievePnoNetworkList();
        assertEquals(2, pnoNetworks.size());
        assertEquals(network1.SSID, pnoNetworks.get(0).ssid);
        assertEquals(network2.SSID, pnoNetworks.get(1).ssid);

        // Now set network1 autojoin disabled. This should remove network 1 from the list.
        network1.allowAutojoin = false;
        // Retrieve the Pno network list & verify.
        pnoNetworks = mWifiConnectivityManager.retrievePnoNetworkList();
        assertEquals(1, pnoNetworks.size());
        assertEquals(network2.SSID, pnoNetworks.get(0).ssid);
    }

    /**
     * Verifies frequencies are populated correctly for pno networks.
     * {@link WifiConnectivityManager#retrievePnoNetworkList()}.
     */
    @Test
    public void testRetrievePnoListFrequencies() {
        // Create 2 networks.
        WifiConfiguration network1 = WifiConfigurationTestUtil.createEapNetwork();
        WifiConfiguration network2 = WifiConfigurationTestUtil.createPskNetwork();
        List<WifiConfiguration> networkList = new ArrayList<>();
        networkList.add(network1);
        networkList.add(network2);
        mLruConnectionTracker.addNetwork(network2);
        mLruConnectionTracker.addNetwork(network1);
        when(mWifiConfigManager.getSavedNetworks(anyInt())).thenReturn(networkList);
        // Retrieve the Pno network list and verify.
        // Frequencies should be empty since no scan results have been received yet.
        List<WifiScanner.PnoSettings.PnoNetwork> pnoNetworks =
                mWifiConnectivityManager.retrievePnoNetworkList();
        assertEquals(2, pnoNetworks.size());
        assertEquals(network1.SSID, pnoNetworks.get(0).ssid);
        assertEquals(network2.SSID, pnoNetworks.get(1).ssid);
        assertEquals("frequencies should be empty", 0, pnoNetworks.get(0).frequencies.length);
        assertEquals("frequencies should be empty", 0, pnoNetworks.get(1).frequencies.length);

        //Set up wifiScoreCard to get frequency.
        List<Integer> channelList = Arrays
                .asList(TEST_FREQUENCY_1, TEST_FREQUENCY_2, TEST_FREQUENCY_3);
        when(mWifiScoreCard.lookupNetwork(network1.SSID)).thenReturn(mPerNetwork);
        when(mWifiScoreCard.lookupNetwork(network2.SSID)).thenReturn(mPerNetwork1);
        when(mPerNetwork.getFrequencies(anyLong())).thenReturn(channelList);
        when(mPerNetwork1.getFrequencies(anyLong())).thenReturn(new ArrayList<>());

        //Set config_wifiPnoFrequencyCullingEnabled false, should ignore get frequency.
        mResources.setBoolean(R.bool.config_wifiPnoFrequencyCullingEnabled, false);
        pnoNetworks = mWifiConnectivityManager.retrievePnoNetworkList();
        assertEquals(2, pnoNetworks.size());
        assertEquals(network1.SSID, pnoNetworks.get(0).ssid);
        assertEquals(network2.SSID, pnoNetworks.get(1).ssid);
        assertEquals("frequencies should be empty", 0, pnoNetworks.get(0).frequencies.length);
        assertEquals("frequencies should be empty", 0, pnoNetworks.get(1).frequencies.length);

        // Set config_wifiPnoFrequencyCullingEnabled false, should get the right frequency.
        mResources.setBoolean(R.bool.config_wifiPnoFrequencyCullingEnabled, true);
        pnoNetworks = mWifiConnectivityManager.retrievePnoNetworkList();
        assertEquals(2, pnoNetworks.size());
        assertEquals(network1.SSID, pnoNetworks.get(0).ssid);
        assertEquals(network2.SSID, pnoNetworks.get(1).ssid);
        assertEquals(3, pnoNetworks.get(0).frequencies.length);
        Arrays.sort(pnoNetworks.get(0).frequencies);
        assertEquals(TEST_FREQUENCY_1, pnoNetworks.get(0).frequencies[0]);
        assertEquals(TEST_FREQUENCY_2, pnoNetworks.get(0).frequencies[1]);
        assertEquals(TEST_FREQUENCY_3, pnoNetworks.get(0).frequencies[2]);
        assertEquals("frequencies should be empty", 0, pnoNetworks.get(1).frequencies.length);
    }


    /**
     * Verifies the ordering of network list generated using
     * {@link WifiConnectivityManager#retrievePnoNetworkList()}.
     */
    @Test
    public void testRetrievePnoListOrder() {
        //Create 3 networks.
        WifiConfiguration network1 = WifiConfigurationTestUtil.createEapNetwork();
        WifiConfiguration network2 = WifiConfigurationTestUtil.createPskNetwork();
        WifiConfiguration network3 = WifiConfigurationTestUtil.createOpenHiddenNetwork();
        mLruConnectionTracker.addNetwork(network1);
        mLruConnectionTracker.addNetwork(network2);
        mLruConnectionTracker.addNetwork(network3);
        List<WifiConfiguration> networkList = new ArrayList<>();
        networkList.add(network1);
        networkList.add(network2);
        networkList.add(network3);
        when(mWifiConfigManager.getSavedNetworks(anyInt())).thenReturn(networkList);
        List<WifiScanner.PnoSettings.PnoNetwork> pnoNetworks =
                mWifiConnectivityManager.retrievePnoNetworkList();
        assertEquals(3, pnoNetworks.size());
        assertEquals(network3.SSID, pnoNetworks.get(0).ssid);
        assertEquals(network2.SSID, pnoNetworks.get(1).ssid);
        assertEquals(network1.SSID, pnoNetworks.get(2).ssid);
    }

    private List<List<Integer>> linkScoreCardFreqsToNetwork(WifiConfiguration... configs) {
        List<List<Integer>> results = new ArrayList<>();
        int i = 0;
        for (WifiConfiguration config : configs) {
            List<Integer> channelList = Arrays.asList(TEST_FREQUENCY_1 + i, TEST_FREQUENCY_2 + i,
                    TEST_FREQUENCY_3 + i);
            WifiScoreCard.PerNetwork perNetwork = mock(WifiScoreCard.PerNetwork.class);
            when(mWifiScoreCard.lookupNetwork(config.SSID)).thenReturn(perNetwork);
            when(perNetwork.getFrequencies(anyLong())).thenReturn(channelList);
            results.add(channelList);
            i++;
        }
        return results;
    }

    /**
     * Verify that the length of frequency set will not exceed the provided max value
     */
    @Test
    public void testFetchChannelSetForPartialScanMaxCount() {
        WifiConfiguration configuration1 = WifiConfigurationTestUtil.createOpenNetwork();
        WifiConfiguration configuration2 = WifiConfigurationTestUtil.createOpenNetwork();
        when(mWifiConfigManager.getSavedNetworks(anyInt()))
                .thenReturn(Arrays.asList(configuration1, configuration2));

        List<List<Integer>> freqs = linkScoreCardFreqsToNetwork(configuration1, configuration2);

        mLruConnectionTracker.addNetwork(configuration2);
        mLruConnectionTracker.addNetwork(configuration1);

        assertEquals(new HashSet<>(freqs.get(0)), mWifiConnectivityManager
                .fetchChannelSetForPartialScan(3, CHANNEL_CACHE_AGE_MINS));
    }

    /**
     * Verifies the creation of channel list using
     * {@link WifiConnectivityManager#fetchChannelSetForNetworkForPartialScan(int)}.
     */
    @Test
    public void testFetchChannelSetForNetwork() {
        WifiConfiguration configuration = WifiConfigurationTestUtil.createOpenNetwork();
        configuration.networkId = TEST_CONNECTED_NETWORK_ID;
        when(mWifiConfigManager.getConfiguredNetwork(TEST_CONNECTED_NETWORK_ID))
                .thenReturn(configuration);
        List<List<Integer>> freqs = linkScoreCardFreqsToNetwork(configuration);

        assertEquals(new HashSet<>(freqs.get(0)), mWifiConnectivityManager
                .fetchChannelSetForNetworkForPartialScan(configuration.networkId));
    }

    /**
     * Verifies the creation of channel list using
     * {@link WifiConnectivityManager#fetchChannelSetForNetworkForPartialScan(int)} and
     * ensures that the frequenecy of the currently connected network is in the returned
     * channel set.
     */
    @Test
    public void testFetchChannelSetForNetworkIncludeCurrentNetwork() {
        WifiConfiguration configuration = WifiConfigurationTestUtil.createOpenNetwork();
        configuration.networkId = TEST_CONNECTED_NETWORK_ID;
        when(mWifiConfigManager.getConfiguredNetwork(TEST_CONNECTED_NETWORK_ID))
                .thenReturn(configuration);
        linkScoreCardFreqsToNetwork(configuration);

        mWifiInfo.setFrequency(TEST_CURRENT_CONNECTED_FREQUENCY);

        // Currently connected network frequency 2427 is not in the TEST_FREQ_LIST
        Set<Integer> freqs = mWifiConnectivityManager.fetchChannelSetForNetworkForPartialScan(
                configuration.networkId);

        assertTrue(freqs.contains(2427));
    }

    /**
     * Verifies the creation of channel list using
     * {@link WifiConnectivityManager#fetchChannelSetForNetworkForPartialScan(int)} and
     * ensures that the list size does not exceed the max configured for the device.
     */
    @Test
    public void testFetchChannelSetForNetworkIsLimitedToConfiguredSize() {
        // Need to recreate the WifiConfigManager instance for this test to modify the config
        // value which is read only in the constructor.
        int maxListSize = 2;
        mResources.setInteger(
                R.integer.config_wifi_framework_associated_partial_scan_max_num_active_channels,
                maxListSize);

        WifiConfiguration configuration = WifiConfigurationTestUtil.createOpenNetwork();
        configuration.networkId = TEST_CONNECTED_NETWORK_ID;
        when(mWifiConfigManager.getConfiguredNetwork(TEST_CONNECTED_NETWORK_ID))
                .thenReturn(configuration);
        List<List<Integer>> freqs = linkScoreCardFreqsToNetwork(configuration);
        // Ensure that the fetched list size is limited.
        Set<Integer> results = mWifiConnectivityManager.fetchChannelSetForNetworkForPartialScan(
                configuration.networkId);
        assertEquals(maxListSize, results.size());
        assertFalse(results.contains(freqs.get(0).get(2)));
    }

    @Test
    public void restartPnoScanForNetworkChanges() {
        mWifiConnectivityManager.setWifiEnabled(true);

        // starts a PNO scan
        mWifiConnectivityManager.handleConnectionStateChanged(
                WifiConnectivityManager.WIFI_STATE_DISCONNECTED);
        mWifiConnectivityManager.setTrustedConnectionAllowed(true);

        InOrder inOrder = inOrder(mWifiScanner);

        inOrder.verify(mWifiScanner).startDisconnectedPnoScan(any(), any(), any(), any());

        // Add or update suggestions.
        mSuggestionUpdateListenerCaptor.getValue().onSuggestionsAddedOrUpdated(
                Arrays.asList(mWifiNetworkSuggestion));
        // Ensure that we restarted PNO.
        inOrder.verify(mWifiScanner).stopPnoScan(any());
        inOrder.verify(mWifiScanner).startDisconnectedPnoScan(any(), any(), any(), any());

        // Add saved network
        mNetworkUpdateListenerCaptor.getValue().onNetworkAdded(new WifiConfiguration());
        // Ensure that we restarted PNO.
        inOrder.verify(mWifiScanner).stopPnoScan(any());
        inOrder.verify(mWifiScanner).startDisconnectedPnoScan(any(), any(), any(), any());
    }
}
