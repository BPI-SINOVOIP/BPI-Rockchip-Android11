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

import static android.net.wifi.WifiManager.WIFI_FEATURE_OWE;

import static com.android.server.wifi.WifiConfigurationTestUtil.SECURITY_EAP;
import static com.android.server.wifi.WifiConfigurationTestUtil.SECURITY_NONE;
import static com.android.server.wifi.WifiConfigurationTestUtil.SECURITY_PSK;
import static com.android.server.wifi.WifiNetworkSelector.experimentIdFromIdentifier;

import static org.hamcrest.Matchers.*;
import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import android.annotation.NonNull;
import android.content.Context;
import android.net.wifi.ScanResult;
import android.net.wifi.SupplicantState;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiConfiguration.NetworkSelectionStatus;
import android.net.wifi.WifiInfo;
import android.os.SystemClock;
import android.util.LocalLog;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.WifiNetworkSelectorTestUtil.ScanDetailsAndWifiConfigs;
import com.android.server.wifi.hotspot2.NetworkDetail;
import com.android.server.wifi.proto.nano.WifiMetricsProto;
import com.android.wifi.resources.R;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;

/**
 * Unit tests for {@link com.android.server.wifi.WifiNetworkSelector}.
 */
@SmallTest
public class WifiNetworkSelectorTest extends WifiBaseTest {
    private static final int RSSI_BUMP = 1;
    private static final int DUMMY_NOMINATOR_ID_1 = -2; // lowest index
    private static final int DUMMY_NOMINATOR_ID_2 = -1;
    private static final int WAIT_JUST_A_MINUTE = 60_000;
    private static final HashSet<String> EMPTY_BLACKLIST = new HashSet<>();

    /** Sets up test. */
    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        setupContext();
        setupResources();
        setupWifiConfigManager();
        setupWifiInfo();

        mScoringParams = new ScoringParams();
        setupThresholds();

        mLocalLog = new LocalLog(512);

        mWifiNetworkSelector = new WifiNetworkSelector(mContext,
                mWifiScoreCard,
                mScoringParams,
                mWifiConfigManager, mClock,
                mLocalLog,
                mWifiMetrics,
                mWifiNative,
                mThroughputPredictor);

        mWifiNetworkSelector.registerNetworkNominator(mDummyNominator);
        mDummyNominator.setNominatorToSelectCandidate(true);
        when(mClock.getElapsedSinceBootMillis()).thenReturn(SystemClock.elapsedRealtime());
        when(mWifiScoreCard.lookupBssid(any(), any())).thenReturn(mPerBssid);
        mCompatibilityScorer = new CompatibilityScorer(mScoringParams);
        mScoreCardBasedScorer = new ScoreCardBasedScorer(mScoringParams);
        mThroughputScorer = new ThroughputScorer(mScoringParams);
        when(mWifiNative.getClientInterfaceName()).thenReturn("wlan0");
        if (WifiNetworkSelector.PRESET_CANDIDATE_SCORER_NAME.equals(
                mThroughputScorer.getIdentifier())) {
            mWifiNetworkSelector.registerCandidateScorer(mThroughputScorer);
        } else {
            mWifiNetworkSelector.registerCandidateScorer(mCompatibilityScorer);
        }
    }

    /** Cleans up test. */
    @After
    public void cleanup() {
        validateMockitoUsage();
    }

    /**
     * Nominates all networks.
     */
    public class AllNetworkNominator implements WifiNetworkSelector.NetworkNominator {
        private static final String NAME = "AllNetworkNominator";
        private final ScanDetailsAndWifiConfigs mScanDetailsAndWifiConfigs;

        public AllNetworkNominator(ScanDetailsAndWifiConfigs scanDetailsAndWifiConfigs) {
            mScanDetailsAndWifiConfigs = scanDetailsAndWifiConfigs;
        }

        @Override
        public @NominatorId int getId() {
            return WifiNetworkSelector.NetworkNominator.NOMINATOR_ID_SAVED;
        }

        @Override
        public String getName() {
            return NAME;
        }

        @Override
        public void update(List<ScanDetail> scanDetails) {}

        @Override
        public void nominateNetworks(List<ScanDetail> scanDetails,
                WifiConfiguration currentNetwork, String currentBssid, boolean connected,
                boolean untrustedNetworkAllowed,
                @NonNull OnConnectableListener onConnectableListener) {
            List<ScanDetail> myScanDetails = mScanDetailsAndWifiConfigs.getScanDetails();
            WifiConfiguration[] configs = mScanDetailsAndWifiConfigs.getWifiConfigs();
            for (int i = 0; i < configs.length; i++) {
                onConnectableListener.onConnectable(myScanDetails.get(i), configs[i]);
            }
        }
    }


    /**
     * All this dummy network Nominator does is to pick the specified network in the scan results.
     */
    public class DummyNetworkNominator implements WifiNetworkSelector.NetworkNominator {
        private static final String NAME = "DummyNetworkNominator";

        private boolean mNominatorShouldSelectCandidate = true;

        private int mNetworkIndexToReturn;
        private int mNominatorIdToReturn;

        public DummyNetworkNominator(int networkIndexToReturn, int nominatorIdToReturn) {
            mNetworkIndexToReturn = networkIndexToReturn;
            mNominatorIdToReturn = nominatorIdToReturn;
        }

        public DummyNetworkNominator() {
            this(0, DUMMY_NOMINATOR_ID_1);
        }

        public int getNetworkIndexToReturn() {
            return mNetworkIndexToReturn;
        }

        public void setNetworkIndexToReturn(int networkIndexToReturn) {
            mNetworkIndexToReturn = networkIndexToReturn;
        }

        @Override
        public @NominatorId int getId() {
            return mNominatorIdToReturn;
        }

        @Override
        public String getName() {
            return NAME;
        }

        @Override
        public void update(List<ScanDetail> scanDetails) {}

        /**
         * Sets whether the nominator should return a candidate for connection or null.
         */
        public void setNominatorToSelectCandidate(boolean shouldSelectCandidate) {
            mNominatorShouldSelectCandidate = shouldSelectCandidate;
        }

        /**
         * This NetworkNominator can be configured to return a candidate or null.  If returning a
         * candidate, the first entry in the provided scanDetails will be selected. This requires
         * that the mock WifiConfigManager be set up to return a WifiConfiguration for the first
         * scanDetail entry, through
         * {@link WifiNetworkSelectorTestUtil#setupScanDetailsAndConfigStore}.
         */
        @Override
        public void nominateNetworks(List<ScanDetail> scanDetails,
                    WifiConfiguration currentNetwork, String currentBssid, boolean connected,
                    boolean untrustedNetworkAllowed,
                    @NonNull OnConnectableListener onConnectableListener) {
            if (!mNominatorShouldSelectCandidate) {
                return;
            }
            for (ScanDetail scanDetail : scanDetails) {
                WifiConfiguration config =
                        mWifiConfigManager.getConfiguredNetworkForScanDetailAndCache(scanDetail);
                mWifiConfigManager.setNetworkCandidateScanResult(
                        config.networkId, scanDetail.getScanResult(), 100);
            }
            ScanDetail scanDetailToReturn = scanDetails.get(mNetworkIndexToReturn);
            WifiConfiguration configToReturn  =
                    mWifiConfigManager.getConfiguredNetworkForScanDetailAndCache(
                            scanDetailToReturn);
            assertNotNull("Saved network must not be null", configToReturn);
            onConnectableListener.onConnectable(scanDetailToReturn, configToReturn);
        }
    }

    private WifiNetworkSelector mWifiNetworkSelector = null;
    private DummyNetworkNominator mDummyNominator = new DummyNetworkNominator();
    @Mock private WifiConfigManager mWifiConfigManager;
    @Mock private Context mContext;
    @Mock private WifiScoreCard mWifiScoreCard;
    @Mock private WifiScoreCard.PerBssid mPerBssid;
    @Mock private WifiCandidates.CandidateScorer mCandidateScorer;
    @Mock private WifiMetrics mWifiMetrics;
    @Mock private WifiNative mWifiNative;
    @Mock private WifiNetworkSelector.NetworkNominator mNetworkNominator;

    // For simulating the resources, we use a Spy on a MockResource
    // (which is really more of a stub than a mock, in spite if its name).
    // This is so that we get errors on any calls that we have not explicitly set up.
    @Spy private MockResources mResource = new MockResources();
    @Mock private WifiInfo mWifiInfo;
    @Mock private Clock mClock;
    @Mock private NetworkDetail mNetworkDetail;
    @Mock private ThroughputPredictor mThroughputPredictor;
    private ScoringParams mScoringParams;
    private LocalLog mLocalLog;
    private int mThresholdMinimumRssi2G;
    private int mThresholdMinimumRssi5G;
    private int mThresholdQualifiedRssi2G;
    private int mThresholdQualifiedRssi5G;
    private int mMinPacketRateActiveTraffic;
    private int mSufficientDurationAfterUserSelection;
    private CompatibilityScorer mCompatibilityScorer;
    private ScoreCardBasedScorer mScoreCardBasedScorer;
    private ThroughputScorer mThroughputScorer;

    private void setupContext() {
        when(mContext.getResources()).thenReturn(mResource);
    }

    private int setupIntegerResource(int resourceName, int value) {
        doReturn(value).when(mResource).getInteger(resourceName);
        return value;
    }

    private void setupResources() {
        doReturn(true).when(mResource).getBoolean(
                R.bool.config_wifi_framework_enable_associated_network_selection);
        mMinPacketRateActiveTraffic = setupIntegerResource(
                R.integer.config_wifiFrameworkMinPacketPerSecondActiveTraffic, 16);
        mSufficientDurationAfterUserSelection = setupIntegerResource(
                R.integer.config_wifiSufficientDurationAfterUserSelectionMilliseconds,
                WAIT_JUST_A_MINUTE);
        doReturn(false).when(mResource).getBoolean(R.bool.config_wifi11axSupportOverride);
    }

    private void setupThresholds() {
        mThresholdMinimumRssi2G = mScoringParams.getEntryRssi(
                ScanResult.BAND_24_GHZ_START_FREQ_MHZ);
        mThresholdMinimumRssi5G = mScoringParams.getEntryRssi(ScanResult.BAND_5_GHZ_START_FREQ_MHZ);

        mThresholdQualifiedRssi2G = mScoringParams.getSufficientRssi(
                ScanResult.BAND_24_GHZ_START_FREQ_MHZ);
        mThresholdQualifiedRssi5G = mScoringParams.getSufficientRssi(
                ScanResult.BAND_5_GHZ_START_FREQ_MHZ);
    }

    private void setupWifiInfo() {
        // simulate a disconnected state
        when(mWifiInfo.getSupplicantState()).thenReturn(SupplicantState.DISCONNECTED);
        when(mWifiInfo.is24GHz()).thenReturn(true);
        when(mWifiInfo.is5GHz()).thenReturn(false);
        when(mWifiInfo.getFrequency()).thenReturn(2400);
        when(mWifiInfo.getRssi()).thenReturn(-70);
        when(mWifiInfo.getNetworkId()).thenReturn(WifiConfiguration.INVALID_NETWORK_ID);
        when(mWifiInfo.getBSSID()).thenReturn(null);
    }

    private void setupWifiConfigManager() {
        setupWifiConfigManager(WifiConfiguration.INVALID_NETWORK_ID);
    }

    private void setupWifiConfigManager(int networkId) {
        when(mWifiConfigManager.getLastSelectedNetwork())
                .thenReturn(networkId);
    }

    /**
     * No network selection if scan result is empty.
     *
     * ClientModeImpl is in disconnected state.
     * scanDetails is empty.
     *
     * Expected behavior: no network recommended by Network Selector
     */
    @Test
    public void emptyScanResults() {
        String[] ssids = new String[0];
        String[] bssids = new String[0];
        int[] freqs = new int[0];
        String[] caps = new String[0];
        int[] levels = new int[0];
        int[] securities = new int[0];

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                    freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        HashSet<String> blacklist = new HashSet<String>();
        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        assertEquals("Expect null configuration", null, candidate);
        assertTrue(mWifiNetworkSelector.getConnectableScanDetails().isEmpty());
    }


    /**
     * No network selection if the RSSI values in scan result are too low.
     *
     * ClientModeImpl is in disconnected state.
     * scanDetails contains a 2.4GHz and a 5GHz network, but both with RSSI lower than
     * the threshold
     *
     * Expected behavior: no network recommended by Network Selector
     */
    @Test
    public void verifyMinimumRssiThreshold() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2437, 5180};
        String[] caps = {"[WPA2-PSK][ESS]", "[WPA2-EAP-CCMP][ESS]"};
        int[] levels = {mThresholdMinimumRssi2G - 1, mThresholdMinimumRssi5G - 1};
        int[] securities = {SECURITY_PSK, SECURITY_EAP};

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                    freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        HashSet<String> blacklist = new HashSet<String>();
        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        assertEquals("Expect null configuration", null, candidate);
        assertTrue(mWifiNetworkSelector.getConnectableScanDetails().isEmpty());
    }

    /**
     * No network selection if WiFi is connected and it is too short
     * from last network selection. Instead, update scanDetailCache.
     *
     * ClientModeImpl is in connected state.
     * scanDetails contains two valid networks.
     * Perform a network selection right after the first one.
     *
     * Expected behavior: no network recommended by Network Selector, update scanDetailCache instead
     */
    @Test
    public void verifyMinimumTimeGapWhenConnected() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2437, 5180};
        String[] caps = {"[WPA2-PSK][ESS]", "[WPA2-EAP-CCMP][ESS]"};
        int[] levels = {mThresholdMinimumRssi2G + RSSI_BUMP, mThresholdMinimumRssi5G + RSSI_BUMP};
        int[] securities = {SECURITY_PSK, SECURITY_EAP};

        // Make a network selection.
        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                        freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        HashSet<String> blacklist = new HashSet<String>();
        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);

        when(mClock.getElapsedSinceBootMillis()).thenReturn(SystemClock.elapsedRealtime()
                + WifiNetworkSelector.MINIMUM_NETWORK_SELECTION_INTERVAL_MS - 2000);

        // Do another network selection with CMI in CONNECTED state.
        candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, true, false, false);
        candidate = mWifiNetworkSelector.selectNetwork(candidates);

        assertEquals("Expect null configuration", null, candidate);
        assertTrue(mWifiNetworkSelector.getConnectableScanDetails().isEmpty());

        verify(mWifiConfigManager, atLeast(2)).updateScanDetailCacheFromScanDetail(any());
    }

    /**
     * Perform network selection if WiFi is disconnected even if it is too short from last
     * network selection.
     *
     * ClientModeImpl is in disconnected state.
     * scanDetails contains two valid networks.
     * Perform a network selection right after the first one.
     *
     * Expected behavior: the first network is recommended by Network Selector
     */
    @Test
    public void verifyNoMinimumTimeGapWhenDisconnected() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2437, 5180};
        String[] caps = {"[WPA2-EAP-CCMP][ESS]", "[WPA2-EAP-CCMP][ESS]"};
        int[] levels = {mThresholdMinimumRssi2G + RSSI_BUMP, mThresholdMinimumRssi5G + RSSI_BUMP};
        int[] securities = {SECURITY_EAP, SECURITY_EAP};

        // Make a network selection.
        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                    freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        WifiConfiguration[] savedConfigs = scanDetailsAndConfigs.getWifiConfigs();
        HashSet<String> blacklist = new HashSet<String>();
        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        WifiConfigurationTestUtil.assertConfigurationEqual(savedConfigs[0], candidate);

        when(mClock.getElapsedSinceBootMillis()).thenReturn(SystemClock.elapsedRealtime()
                + WifiNetworkSelector.MINIMUM_NETWORK_SELECTION_INTERVAL_MS - 2000);

        // Do another network selection with CMI in DISCONNECTED state.
        candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        candidate = mWifiNetworkSelector.selectNetwork(candidates);

        ScanResult chosenScanResult = scanDetails.get(0).getScanResult();
        WifiConfigurationTestUtil.assertConfigurationEqual(savedConfigs[0], candidate);
        WifiNetworkSelectorTestUtil.verifySelectedScanResult(mWifiConfigManager,
                chosenScanResult, candidate);
    }

    /**
     * New network selection is performed if the currently connected network
     * has low RSSI value.
     *
     * ClientModeImpl is connected to a low RSSI 5GHz network.
     * scanDetails contains a valid networks.
     * Perform a network selection after the first one.
     *
     * Expected behavior: the first network is recommended by Network Selector
     */
    @Test
    public void lowRssi5GNetworkIsNotSufficient() {
        String[] ssids = {"\"test1\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3"};
        int[] freqs = {5180};
        String[] caps = {"[WPA2-PSK][ESS]"};
        int[] levels = {mThresholdQualifiedRssi5G - 2};
        int[] securities = {SECURITY_PSK};

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                    freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        HashSet<String> blacklist = new HashSet<String>();
        WifiConfiguration[] savedConfigs = scanDetailsAndConfigs.getWifiConfigs();

        // connect to test1
        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        when(mWifiInfo.getSupplicantState()).thenReturn(SupplicantState.COMPLETED);
        when(mWifiInfo.getNetworkId()).thenReturn(0);
        when(mWifiInfo.getBSSID()).thenReturn(bssids[0]);
        when(mWifiInfo.is24GHz()).thenReturn(false);
        when(mWifiInfo.is5GHz()).thenReturn(true);
        when(mWifiInfo.getFrequency()).thenReturn(5000);
        when(mWifiInfo.getRssi()).thenReturn(levels[0]);

        when(mClock.getElapsedSinceBootMillis()).thenReturn(SystemClock.elapsedRealtime()
                + WifiNetworkSelector.MINIMUM_NETWORK_SELECTION_INTERVAL_MS + 2000);

        // Do another network selection.
        candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, true, false, false);
        candidate = mWifiNetworkSelector.selectNetwork(candidates);

        ScanResult chosenScanResult = scanDetails.get(0).getScanResult();
        WifiNetworkSelectorTestUtil.verifySelectedScanResult(mWifiConfigManager,
                chosenScanResult, candidate);
    }

    /**
     * New network selection is performed if the currently connected network
     * has no internet access although it has active traffic and high RSSI
     *
     * ClientModeImpl is connected to a network with no internet connectivity.
     * scanDetails contains a valid networks.
     * Perform a network selection after the first one.
     *
     * Expected behavior: the first network is recommended by Network Selector
     */
    @Test
    public void noInternetAccessNetworkIsNotSufficient() {
        String[] ssids = {"\"test1\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3"};
        int[] freqs = {5180};
        String[] caps = {"[WPA2-PSK][ESS]"};
        int[] levels = {mThresholdQualifiedRssi5G + 5};
        int[] securities = {SECURITY_PSK};

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                        freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        HashSet<String> blacklist = new HashSet<String>();
        WifiConfiguration[] savedConfigs = scanDetailsAndConfigs.getWifiConfigs();

        // connect to test1
        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        when(mWifiInfo.getSupplicantState()).thenReturn(SupplicantState.COMPLETED);
        when(mWifiInfo.getNetworkId()).thenReturn(0);
        when(mWifiInfo.getBSSID()).thenReturn(bssids[0]);
        when(mWifiInfo.is24GHz()).thenReturn(false);
        when(mWifiInfo.is5GHz()).thenReturn(true);
        when(mWifiInfo.getFrequency()).thenReturn(5000);
        when(mWifiInfo.getRssi()).thenReturn(levels[0]);
        when(mWifiInfo.getSuccessfulTxPacketsPerSecond())
                .thenReturn(mMinPacketRateActiveTraffic - 1.0);
        when(mWifiInfo.getSuccessfulRxPacketsPerSecond())
                .thenReturn(mMinPacketRateActiveTraffic + 1.0);

        when(mClock.getElapsedSinceBootMillis()).thenReturn(SystemClock.elapsedRealtime()
                + WifiNetworkSelector.MINIMUM_NETWORK_SELECTION_INTERVAL_MS + 2000);

        // Increment the network's no internet access reports.
        savedConfigs[0].numNoInternetAccessReports = 5;

        // Do another network selection.
        candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, true, false, false);
        candidate = mWifiNetworkSelector.selectNetwork(candidates);

        ScanResult chosenScanResult = scanDetails.get(0).getScanResult();
        WifiNetworkSelectorTestUtil.verifySelectedScanResult(mWifiConfigManager,
                chosenScanResult, candidate);
    }
    /**
     * Ensure that network selector update's network selection status for all configured
     * networks before performing network selection.
     *
     * Expected behavior: the first network is recommended by Network Selector
     */
    @Test
    public void updateConfiguredNetworks() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2437, 2457};
        String[] caps = {"[WPA2-EAP-CCMP][ESS]", "[WPA2-PSK][ESS]"};
        int[] levels = {mThresholdMinimumRssi2G + 20, mThresholdMinimumRssi2G + RSSI_BUMP};
        int[] securities = {SECURITY_EAP, SECURITY_PSK};

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                        freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        HashSet<String> blacklist = new HashSet<String>();
        WifiConfiguration[] savedConfigs = scanDetailsAndConfigs.getWifiConfigs();

        // Do network selection.
        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, true, false, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        verify(mWifiMetrics).incrementNetworkSelectionFilteredBssidCount(0);

        verify(mWifiConfigManager).getConfiguredNetworks();
        verify(mWifiConfigManager, times(savedConfigs.length)).tryEnableNetwork(anyInt());
        verify(mWifiConfigManager, times(savedConfigs.length))
                .clearNetworkCandidateScanResult(anyInt());
        verify(mWifiMetrics, atLeastOnce()).addMeteredStat(any(), anyBoolean());
    }

    /**
     * Blacklisted BSSID is filtered out for network selection.
     *
     * ClientModeImpl is disconnected.
     * scanDetails contains a network which is blacklisted.
     *
     * Expected behavior: no network recommended by Network Selector
     */
    @Test
    public void filterOutBlacklistedBssid() {
        String[] ssids = {"\"test1\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3"};
        int[] freqs = {5180};
        String[] caps = {"[WPA2-PSK][ESS]"};
        int[] levels = {mThresholdQualifiedRssi5G + 8};
        int[] securities = {SECURITY_PSK};

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                    freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        HashSet<String> blacklist = new HashSet<String>();
        blacklist.add(bssids[0]);

        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        verify(mWifiMetrics).incrementNetworkSelectionFilteredBssidCount(1);
        assertEquals("Expect null configuration", null, candidate);
        assertTrue(mWifiNetworkSelector.getConnectableScanDetails().isEmpty());
    }

    /**
     * Wifi network selector doesn't recommend any network if the currently connected one
     * doesn't show up in the scan results.
     *
     * ClientModeImpl is under connected state and 2.4GHz test1 is connected.
     * The second scan results contains only test2 which now has a stronger RSSI than test1.
     * Test1 is not in the second scan results.
     *
     * Expected behavior: no network recommended by Network Selector
     */
    @Test
    public void noSelectionWhenCurrentNetworkNotInScanResults() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2437, 2457};
        String[] caps = {"[WPA2-EAP-CCMP][ESS]", "[WPA2-PSK][ESS]"};
        int[] levels = {mThresholdMinimumRssi2G + 20, mThresholdMinimumRssi2G + RSSI_BUMP};
        int[] securities = {SECURITY_EAP, SECURITY_PSK};

        // Make a network selection to connect to test1.
        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                    freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        HashSet<String> blacklist = new HashSet<String>();
        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);

        when(mWifiInfo.getSupplicantState()).thenReturn(SupplicantState.COMPLETED);
        when(mWifiInfo.getNetworkId()).thenReturn(0);
        when(mWifiInfo.getBSSID()).thenReturn(bssids[0]);
        when(mWifiInfo.is24GHz()).thenReturn(true);
        when(mWifiInfo.getScore()).thenReturn(ConnectedScore.WIFI_TRANSITION_SCORE);
        when(mWifiInfo.is5GHz()).thenReturn(false);
        when(mWifiInfo.getFrequency()).thenReturn(2400);
        when(mWifiInfo.getRssi()).thenReturn(levels[0]);
        when(mClock.getElapsedSinceBootMillis()).thenReturn(SystemClock.elapsedRealtime()
                + WifiNetworkSelector.MINIMUM_NETWORK_SELECTION_INTERVAL_MS + 2000);

        // Prepare the second scan results which have no test1.
        String[] ssidsNew = {"\"test2\""};
        String[] bssidsNew = {"6c:f3:7f:ae:8c:f4"};
        int[] freqsNew = {2457};
        String[] capsNew = {"[WPA2-EAP-CCMP][ESS]"};
        int[] levelsNew = {mThresholdMinimumRssi2G + 40};
        scanDetails = WifiNetworkSelectorTestUtil.buildScanDetails(ssidsNew, bssidsNew,
                freqsNew, capsNew, levelsNew, mClock);
        candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, true, false, false);
        candidate = mWifiNetworkSelector.selectNetwork(candidates);

        // The second network selection is skipped since current connected network is
        // missing from the scan results.
        assertEquals("Expect null configuration", null, candidate);
        assertTrue(mWifiNetworkSelector.getConnectableScanDetails().isEmpty());
        verify(mWifiConfigManager, atLeast(1)).updateScanDetailCacheFromScanDetail(any());
    }

    /**
     * Ensures that setting the user connect choice updates the
     * NetworkSelectionStatus#mConnectChoice for all other WifiConfigurations in range in the last
     * round of network selection.
     *
     * Expected behavior: WifiConfiguration.NetworkSelectionStatus#mConnectChoice is set to
     *                    test1's configkey for test2. test3's WifiConfiguration is unchanged.
     */
    @Test
    public void setUserConnectChoice() {
        String[] ssids = {"\"test1\"", "\"test2\"", "\"test3\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4", "6c:f3:7f:ae:8c:f5"};
        int[] freqs = {2437, 5180, 5181};
        String[] caps = {"[WPA2-PSK][ESS]", "[WPA2-EAP-CCMP][ESS]", "[WPA2-PSK][ESS]"};
        int[] levels = {mThresholdMinimumRssi2G + RSSI_BUMP, mThresholdMinimumRssi5G + RSSI_BUMP,
                mThresholdMinimumRssi5G + RSSI_BUMP};
        int[] securities = {SECURITY_PSK, SECURITY_EAP, SECURITY_PSK};

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                        freqs, caps, levels, securities, mWifiConfigManager, mClock);

        WifiConfiguration selectedWifiConfig = scanDetailsAndConfigs.getWifiConfigs()[0];
        selectedWifiConfig.getNetworkSelectionStatus()
                .setCandidate(scanDetailsAndConfigs.getScanDetails().get(0).getScanResult());
        selectedWifiConfig.getNetworkSelectionStatus().setNetworkSelectionStatus(
                NetworkSelectionStatus.NETWORK_SELECTION_PERMANENTLY_DISABLED);
        selectedWifiConfig.getNetworkSelectionStatus().setConnectChoice("bogusKey");

        WifiConfiguration configInLastNetworkSelection = scanDetailsAndConfigs.getWifiConfigs()[1];
        configInLastNetworkSelection.getNetworkSelectionStatus()
                .setSeenInLastQualifiedNetworkSelection(true);

        WifiConfiguration configNotInLastNetworkSelection =
                scanDetailsAndConfigs.getWifiConfigs()[2];

        assertTrue(mWifiNetworkSelector.setUserConnectChoice(selectedWifiConfig.networkId));

        verify(mWifiConfigManager).updateNetworkSelectionStatus(selectedWifiConfig.networkId,
                NetworkSelectionStatus.DISABLED_NONE);
        verify(mWifiConfigManager).clearNetworkConnectChoice(selectedWifiConfig.networkId);
        verify(mWifiConfigManager).setNetworkConnectChoice(configInLastNetworkSelection.networkId,
                selectedWifiConfig.getKey());
        verify(mWifiConfigManager, never()).setNetworkConnectChoice(
                configNotInLastNetworkSelection.networkId, selectedWifiConfig.getKey());
    }

    /**
     * Wifi network selector stays with current network if current network is not nominated
     * but has the higher score
     */
    @Test
    public void includeCurrentNetworkWhenCurrentNetworkNotNominated() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2437, 5120};
        String[] caps = {"[WPA2-EAP-CCMP][ESS]", "[WPA2-PSK][ESS]"};
        int[] levels = {mThresholdMinimumRssi2G + 10, mThresholdMinimumRssi2G + 20};
        int[] securities = {SECURITY_EAP, SECURITY_PSK};
        // VHT cap IE
        byte[] iesBytes = {(byte) 0x92, (byte) 0x01, (byte) 0x80, (byte) 0x33, (byte) 0xaa,
                (byte) 0xff, (byte) 0x00, (byte) 0x00, (byte) 0xaa, (byte) 0xff, (byte) 0x00,
                (byte) 0x00};
        byte[][] iesByteStream = {iesBytes, iesBytes};
        // Make a network selection to connect to test1.
        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                        freqs, caps, levels, securities, mWifiConfigManager, mClock, iesByteStream);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        assertEquals(2, scanDetails.size());
        HashSet<String> blocklist = new HashSet<String>();
        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blocklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);

        when(mWifiInfo.getSupplicantState()).thenReturn(SupplicantState.COMPLETED);
        when(mWifiInfo.getNetworkId()).thenReturn(0); // 0 is current network
        when(mWifiInfo.getBSSID()).thenReturn(bssids[0]);
        when(mWifiInfo.is24GHz()).thenReturn(true);
        when(mWifiInfo.getScore()).thenReturn(ConnectedScore.WIFI_TRANSITION_SCORE);
        when(mWifiInfo.is5GHz()).thenReturn(false);
        when(mWifiInfo.getFrequency()).thenReturn(2400);
        when(mWifiInfo.getRssi()).thenReturn(levels[0]);
        when(mClock.getElapsedSinceBootMillis()).thenReturn(SystemClock.elapsedRealtime()
                + WifiNetworkSelector.MINIMUM_NETWORK_SELECTION_INTERVAL_MS + 2000);

        when(mThroughputPredictor.predictThroughput(any(), anyInt(), anyInt(), anyInt(),
                anyInt(), anyInt(), anyInt(), anyInt(), anyBoolean())).thenReturn(100);
        // Force to return 2nd network in the network nominator
        mDummyNominator.setNetworkIndexToReturn(1);

        candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blocklist, mWifiInfo, true, false, false);
        assertEquals(2, candidates.size());
        assertEquals(100, candidates.get(0).getPredictedThroughputMbps());
    }

    /**
     * If two qualified networks, test1 and test2, are in range when the user selects test2 over
     * test1, WifiNetworkSelector will override the NetworkSelector's choice to connect to test1
     * with test2.
     *
     * Expected behavior: test2 is the recommended network
     */
    @Test
    public void userConnectChoiceOverridesNetworkNominators() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2437, 5180};
        String[] caps = {"[WPA2-PSK][ESS]", "[WPA2-PSK][ESS]"};
        int[] levels = {mThresholdMinimumRssi2G + RSSI_BUMP, mThresholdMinimumRssi5G + RSSI_BUMP};
        int[] securities = {SECURITY_PSK, SECURITY_PSK};

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                        freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        HashSet<String> blacklist = new HashSet<String>();

        // DummyNominator always selects the first network in the list.
        WifiConfiguration networkSelectorChoice = scanDetailsAndConfigs.getWifiConfigs()[0];
        networkSelectorChoice.getNetworkSelectionStatus()
                .setSeenInLastQualifiedNetworkSelection(true);

        WifiConfiguration userChoice = scanDetailsAndConfigs.getWifiConfigs()[1];
        userChoice.getNetworkSelectionStatus()
                .setCandidate(scanDetailsAndConfigs.getScanDetails().get(1).getScanResult());

        // With no user choice set, networkSelectorChoice should be chosen.
        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);

        ArgumentCaptor<Integer> nominatorIdCaptor = ArgumentCaptor.forClass(int.class);
        verify(mWifiMetrics, atLeastOnce()).setNominatorForNetwork(eq(candidate.networkId),
                nominatorIdCaptor.capture());
        // unknown because DummyNominator does not have a nominator ID
        // getValue() returns the argument from the *last* call
        assertEquals(WifiMetricsProto.ConnectionEvent.NOMINATOR_UNKNOWN,
                nominatorIdCaptor.getValue().intValue());

        WifiConfigurationTestUtil.assertConfigurationEqual(networkSelectorChoice, candidate);

        when(mClock.getElapsedSinceBootMillis()).thenReturn(SystemClock.elapsedRealtime()
                + WifiNetworkSelector.MINIMUM_NETWORK_SELECTION_INTERVAL_MS + 2000);

        assertTrue(mWifiNetworkSelector.setUserConnectChoice(userChoice.networkId));

        // After user connect choice is set, userChoice should override networkSelectorChoice.
        candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        candidate = mWifiNetworkSelector.selectNetwork(candidates);

        verify(mWifiMetrics, atLeastOnce()).setNominatorForNetwork(eq(candidate.networkId),
                nominatorIdCaptor.capture());
        // getValue() returns the argument from the *last* call
        assertEquals(WifiMetricsProto.ConnectionEvent.NOMINATOR_SAVED_USER_CONNECT_CHOICE,
                nominatorIdCaptor.getValue().intValue());
        WifiConfigurationTestUtil.assertConfigurationEqual(userChoice, candidate);
    }

    /**
     * Tests when multiple Nominators nominate the same candidate, any one of the nominator IDs is
     * acceptable.
     */
    @Test
    public void testMultipleNominatorsSetsNominatorIdCorrectly() {
        // first dummy Nominator is registered in setup, returns index 0
        // register a second network Nominator that also returns index 0, but with a different ID
        mWifiNetworkSelector.registerNetworkNominator(new DummyNetworkNominator(0,
                WifiNetworkSelector.NetworkNominator.NOMINATOR_ID_SCORED));
        // register a third network Nominator that also returns index 0, but with a different ID
        mWifiNetworkSelector.registerNetworkNominator(new DummyNetworkNominator(0,
                WifiNetworkSelector.NetworkNominator.NOMINATOR_ID_SAVED));

        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2437, 5180};
        String[] caps = {"[WPA2-PSK][ESS]", "[WPA2-PSK][ESS]"};
        int[] levels = {mThresholdMinimumRssi2G + RSSI_BUMP, mThresholdMinimumRssi5G + RSSI_BUMP};
        int[] securities = {SECURITY_PSK, SECURITY_PSK};

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                        freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        HashSet<String> blacklist = new HashSet<>();

        // DummyNominator always selects the first network in the list.
        WifiConfiguration networkSelectorChoice = scanDetailsAndConfigs.getWifiConfigs()[0];
        networkSelectorChoice.getNetworkSelectionStatus()
                .setSeenInLastQualifiedNetworkSelection(true);

        WifiConfiguration userChoice = scanDetailsAndConfigs.getWifiConfigs()[1];
        userChoice.getNetworkSelectionStatus()
                .setCandidate(scanDetailsAndConfigs.getScanDetails().get(1).getScanResult());

        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);

        ArgumentCaptor<Integer> nominatorIdCaptor = ArgumentCaptor.forClass(int.class);
        verify(mWifiMetrics, atLeastOnce()).setNominatorForNetwork(eq(candidate.networkId),
                nominatorIdCaptor.capture());

        for (int nominatorId : nominatorIdCaptor.getAllValues()) {
            assertThat(nominatorId, is(oneOf(
                    WifiMetricsProto.ConnectionEvent.NOMINATOR_UNKNOWN,
                    WifiMetricsProto.ConnectionEvent.NOMINATOR_EXTERNAL_SCORED,
                    WifiMetricsProto.ConnectionEvent.NOMINATOR_SAVED)));
        }
        verify(mWifiMetrics, atLeastOnce()).setNetworkSelectorExperimentId(anyInt());
    }

    /**
     * Wifi network selector performs network selection when current network has high
     * quality but no active stream
     *
     * Expected behavior: network selection is performed
     */
    @Test
    public void testNoActiveStream() {
        // Rssi after connected.
        when(mWifiInfo.getRssi()).thenReturn(mThresholdQualifiedRssi2G + 1);
        when(mWifiInfo.getSuccessfulTxPacketsPerSecond()).thenReturn(0.0);
        when(mWifiInfo.getSuccessfulRxPacketsPerSecond()).thenReturn(0.0);

        testStayOrTryToSwitch(
                // Parameters for network1:
                mThresholdQualifiedRssi2G + 1 /* rssi before connected */,
                false /* not a 5G network */,
                false /* not open network */,
                false /* not a osu */,
                // Parameters for network2:
                mThresholdQualifiedRssi5G + 1 /* rssi */,
                true /* a 5G network */,
                false /* not open network */,
                // Should try to switch.
                true);
    }

    /**
     * Wifi network selector skips network selection when current network is osu and has low RSSI
     *
     * Expected behavior: network selection is skipped
     */
    @Test
    public void testOsuIsSufficient() {
        // Rssi after connected.
        when(mWifiInfo.getRssi()).thenReturn(mThresholdQualifiedRssi5G - 1);
        when(mWifiInfo.getSuccessfulTxPacketsPerSecond()).thenReturn(0.0);
        when(mWifiInfo.getSuccessfulRxPacketsPerSecond()).thenReturn(0.0);

        testStayOrTryToSwitch(
                // Parameters for network1:
                mThresholdQualifiedRssi5G - 1 /* rssi before connected */,
                false /* not a 5G network */,
                false /* not open network */,
                true /* osu */,
                // Parameters for network2:
                mThresholdQualifiedRssi5G + 1 /* rssi */,
                true /* a 5G network */,
                false /* not open network */,
                // Should not try to switch.
                false);
    }

    /**
     * Wifi network selector will not perform network selection when current network has high
     * quality and active stream
     *
     *
     * Expected behavior: network selection is not performed
     */
    @Test
    public void testSufficientLinkQualityActiveStream() {
        // Rssi after connected.
        when(mWifiInfo.getRssi()).thenReturn(mThresholdQualifiedRssi5G + 1);
        when(mWifiInfo.getSuccessfulTxPacketsPerSecond())
                .thenReturn(mMinPacketRateActiveTraffic - 1.0);
        when(mWifiInfo.getSuccessfulRxPacketsPerSecond())
                .thenReturn(mMinPacketRateActiveTraffic * 2.0);

        testStayOrTryToSwitch(
                mThresholdQualifiedRssi5G + 1 /* rssi before connected */,
                true /* a 5G network */,
                false /* not open network */,
                // Should not try to switch.
                false);
    }

    /**
     * New network selection is not performed if the currently connected network
     * was recently selected.
     */
    @Test
    public void networkIsSufficientWhenRecentlyUserSelected() {
        // Approximate mClock.getElapsedSinceBootMillis value mocked by testStayOrTryToSwitch
        long millisSinceBoot = SystemClock.elapsedRealtime()
                + WifiNetworkSelector.MINIMUM_NETWORK_SELECTION_INTERVAL_MS + 2000;
        when(mWifiConfigManager.getLastSelectedTimeStamp())
                .thenReturn(millisSinceBoot
                        - WAIT_JUST_A_MINUTE
                        + 1000);
        setupWifiConfigManager(0); // testStayOrTryToSwitch first connects to network 0
        // Rssi after connected.
        when(mWifiInfo.getRssi()).thenReturn(mThresholdQualifiedRssi2G + 1);
        // No streaming traffic.
        when(mWifiInfo.getSuccessfulTxPacketsPerSecond()).thenReturn(0.0);
        when(mWifiInfo.getSuccessfulRxPacketsPerSecond()).thenReturn(0.0);

        testStayOrTryToSwitch(
                mThresholdQualifiedRssi2G + 1 /* rssi before connected */,
                false /* not a 5G network */,
                false /* not open network */,
                // Should not try to switch.
                false);
    }

    /**
     * New network selection is performed if the currently connected network has bad rssi.
     *
     * Expected behavior: Network Selector perform network selection after connected
     * to the first one.
     */
    @Test
    public void testBadRssi() {
        // Rssi after connected.
        when(mWifiInfo.getRssi()).thenReturn(mThresholdQualifiedRssi2G - 1);
        when(mWifiInfo.getSuccessfulTxPacketsPerSecond())
                .thenReturn(mMinPacketRateActiveTraffic + 1.0);
        when(mWifiInfo.getSuccessfulRxPacketsPerSecond())
                .thenReturn(mMinPacketRateActiveTraffic - 1.0);

        testStayOrTryToSwitch(
                mThresholdQualifiedRssi2G + 1 /* rssi before connected */,
                false /* not a 5G network */,
                false /* not open network */,
                // Should try to switch.
                true);
    }

    /**
     * This is a meta-test that given two scan results of various types, will
     * determine whether or not network selection should be performed.
     *
     * It sets up two networks, connects to the first, and then ensures that
     * both are available in the scan results for the NetworkSelector.
     */
    private void testStayOrTryToSwitch(
            int rssiNetwork1, boolean is5GHzNetwork1, boolean isOpenNetwork1,
            boolean isFirstNetworkOsu,
            int rssiNetwork2, boolean is5GHzNetwork2, boolean isOpenNetwork2,
            boolean shouldSelect) {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {is5GHzNetwork1 ? 5180 : 2437, is5GHzNetwork2 ? 5180 : 2437};
        String[] caps = {isOpenNetwork1 ? "[ESS]" : "[WPA2-PSK][ESS]",
                         isOpenNetwork2 ? "[ESS]" : "[WPA2-PSK][ESS]"};
        int[] levels = {rssiNetwork1, rssiNetwork2};
        int[] securities = {isOpenNetwork1 ? SECURITY_NONE : SECURITY_PSK,
                            isOpenNetwork2 ? SECURITY_NONE : SECURITY_PSK};
        testStayOrTryToSwitchImpl(ssids, bssids, freqs, caps, levels, securities, isFirstNetworkOsu,
                shouldSelect);
    }

    /**
     * This is a meta-test that given one scan results, will
     * determine whether or not network selection should be performed.
     *
     * It sets up one network, connects to it, and then ensures that it is in
     * the scan results for the NetworkSelector.
     */
    private void testStayOrTryToSwitch(
            int rssi, boolean is5GHz, boolean isOpenNetwork,
            boolean shouldSelect) {
        String[] ssids = {"\"test1\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3"};
        int[] freqs = {is5GHz ? 5180 : 2437};
        String[] caps = {isOpenNetwork ? "[ESS]" : "[WPA2-PSK][ESS]"};
        int[] levels = {rssi};
        int[] securities = {isOpenNetwork ? SECURITY_NONE : SECURITY_PSK};
        testStayOrTryToSwitchImpl(ssids, bssids, freqs, caps, levels, securities, false,
                shouldSelect);
    }

    private void testStayOrTryToSwitchImpl(String[] ssids, String[] bssids, int[] freqs,
            String[] caps, int[] levels, int[] securities, boolean isFirstNetworkOsu,
            boolean shouldSelect) {
        // Make a network selection to connect to test1.
        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                        freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        HashSet<String> blacklist = new HashSet<String>();
        // DummyNetworkNominator always return the first network in the scan results
        // for connection, so this should connect to the first network.
        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, true);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        assertNotNull("Result should be not null", candidate);
        WifiNetworkSelectorTestUtil.verifySelectedScanResult(mWifiConfigManager,
                scanDetails.get(0).getScanResult(), candidate);

        when(mWifiInfo.getSupplicantState()).thenReturn(SupplicantState.COMPLETED);
        when(mWifiInfo.getNetworkId()).thenReturn(0);
        when(mWifiInfo.getBSSID()).thenReturn(bssids[0]);
        when(mWifiInfo.is24GHz()).thenReturn(!ScanResult.is5GHz(freqs[0]));
        when(mWifiInfo.is5GHz()).thenReturn(ScanResult.is5GHz(freqs[0]));
        when(mWifiInfo.getFrequency()).thenReturn(freqs[0]);
        if (isFirstNetworkOsu) {
            WifiConfiguration[] configs = scanDetailsAndConfigs.getWifiConfigs();
            // Force 1st network to OSU
            configs[0].osu = true;
            when(mWifiConfigManager.getConfiguredNetwork(mWifiInfo.getNetworkId()))
                    .thenReturn(configs[0]);
        }

        when(mClock.getElapsedSinceBootMillis()).thenReturn(SystemClock.elapsedRealtime()
                + WifiNetworkSelector.MINIMUM_NETWORK_SELECTION_INTERVAL_MS + 2000);

        candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, true, false, false);
        candidate = mWifiNetworkSelector.selectNetwork(candidates);

        // DummyNetworkNominator always return the first network in the scan results
        // for connection, so if network selection is performed, the first network should
        // be returned as candidate.
        if (shouldSelect) {
            assertNotNull("Result should be not null", candidate);
            WifiNetworkSelectorTestUtil.verifySelectedScanResult(mWifiConfigManager,
                    scanDetails.get(0).getScanResult(), candidate);
        } else {
            assertEquals("Expect null configuration", null, candidate);
        }
    }

    /**
     * {@link WifiNetworkSelector#getFilteredScanDetailsForOpenUnsavedNetworks()} should filter out
     * networks that are not open after network selection is made.
     *
     * Expected behavior: return open networks only
     */
    @Test
    public void getfilterOpenUnsavedNetworks_filtersForOpenNetworks() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2437, 5180};
        String[] caps = {"[WPA2-EAP-CCMP][ESS]", "[ESS]"};
        int[] levels = {mThresholdMinimumRssi2G + RSSI_BUMP, mThresholdMinimumRssi5G + RSSI_BUMP};
        mDummyNominator.setNominatorToSelectCandidate(false);

        List<ScanDetail> scanDetails = WifiNetworkSelectorTestUtil.buildScanDetails(
                ssids, bssids, freqs, caps, levels, mClock);
        HashSet<String> blacklist = new HashSet<>();

        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        List<ScanDetail> expectedOpenUnsavedNetworks = new ArrayList<>();
        expectedOpenUnsavedNetworks.add(scanDetails.get(1));
        assertEquals("Expect open unsaved networks",
                expectedOpenUnsavedNetworks,
                mWifiNetworkSelector.getFilteredScanDetailsForOpenUnsavedNetworks());
    }

    /**
     * {@link WifiNetworkSelector#getFilteredScanDetailsForOpenUnsavedNetworks()} should filter out
     * saved networks after network selection is made. This should return an empty list when there
     * are no unsaved networks available.
     *
     * Expected behavior: return unsaved networks only. Return empty list if there are no unsaved
     * networks.
     */
    @Test
    public void getfilterOpenUnsavedNetworks_filtersOutSavedNetworks() {
        String[] ssids = {"\"test1\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3"};
        int[] freqs = {2437, 5180};
        String[] caps = {"[ESS]"};
        int[] levels = {mThresholdMinimumRssi2G + RSSI_BUMP};
        int[] securities = {SECURITY_NONE};
        mDummyNominator.setNominatorToSelectCandidate(false);

        List<ScanDetail> unSavedScanDetails = WifiNetworkSelectorTestUtil.buildScanDetails(
                ssids, bssids, freqs, caps, levels, mClock);
        HashSet<String> blacklist = new HashSet<>();

        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                unSavedScanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        assertEquals("Expect open unsaved networks",
                unSavedScanDetails,
                mWifiNetworkSelector.getFilteredScanDetailsForOpenUnsavedNetworks());

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                        freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> savedScanDetails = scanDetailsAndConfigs.getScanDetails();

        candidates = mWifiNetworkSelector.getCandidatesFromScan(
                savedScanDetails, blacklist, mWifiInfo, false, true, false);
        candidate = mWifiNetworkSelector.selectNetwork(candidates);
        // Saved networks are filtered out.
        assertTrue(mWifiNetworkSelector.getFilteredScanDetailsForOpenUnsavedNetworks().isEmpty());
    }

    /**
     * {@link WifiNetworkSelector#getFilteredScanDetailsForOpenUnsavedNetworks()} should filter out
     * bssid blacklisted networks.
     *
     * Expected behavior: do not return blacklisted network
     */
    @Test
    public void getfilterOpenUnsavedNetworks_filtersOutBlacklistedNetworks() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2437, 5180};
        String[] caps = {"[ESS]", "[ESS]"};
        int[] levels = {mThresholdMinimumRssi2G + RSSI_BUMP, mThresholdMinimumRssi5G + RSSI_BUMP};
        mDummyNominator.setNominatorToSelectCandidate(false);

        List<ScanDetail> scanDetails = WifiNetworkSelectorTestUtil.buildScanDetails(
                ssids, bssids, freqs, caps, levels, mClock);
        HashSet<String> blacklist = new HashSet<>();
        blacklist.add(bssids[0]);

        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        List<ScanDetail> expectedOpenUnsavedNetworks = new ArrayList<>();
        expectedOpenUnsavedNetworks.add(scanDetails.get(1));
        assertEquals("Expect open unsaved networks",
                expectedOpenUnsavedNetworks,
                mWifiNetworkSelector.getFilteredScanDetailsForOpenUnsavedNetworks());
    }

    /**
     * {@link WifiNetworkSelector#getFilteredScanDetailsForOpenUnsavedNetworks()} should return
     * empty list when there are no open networks after network selection is made.
     *
     * Expected behavior: return empty list
     */
    @Test
    public void getfilterOpenUnsavedNetworks_returnsEmptyListWhenNoOpenNetworksPresent() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2437, 5180};
        String[] caps = {"[WPA2-EAP-CCMP][ESS]", "[WPA2-EAP-CCMP][ESS]"};
        int[] levels = {mThresholdMinimumRssi2G + RSSI_BUMP, mThresholdMinimumRssi5G + RSSI_BUMP};
        mDummyNominator.setNominatorToSelectCandidate(false);

        List<ScanDetail> scanDetails = WifiNetworkSelectorTestUtil.buildScanDetails(
                ssids, bssids, freqs, caps, levels, mClock);
        HashSet<String> blacklist = new HashSet<>();

        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        assertTrue(mWifiNetworkSelector.getFilteredScanDetailsForOpenUnsavedNetworks().isEmpty());
    }

    /**
     * {@link WifiNetworkSelector#getFilteredScanDetailsForOpenUnsavedNetworks()} should return
     * empty list when no network selection has been made.
     *
     * Expected behavior: return empty list
     */
    @Test
    public void getfilterOpenUnsavedNetworks_returnsEmptyListWhenNoNetworkSelectionMade() {
        assertTrue(mWifiNetworkSelector.getFilteredScanDetailsForOpenUnsavedNetworks().isEmpty());
    }

    /**
     * {@link WifiNetworkSelector#getFilteredScanDetailsForOpenUnsavedNetworks()} for device that
     * supports enhanced open networks, should filter out networks that are not open and not
     * enhanced open after network selection is made.
     *
     * Expected behavior: return open and enhanced open networks only
     */
    @Test
    public void getfilterOpenUnsavedNetworks_filtersForOpenAndOweNetworksOweSupported() {
        String[] ssids = {"\"test1\"", "\"test2\"", "\"test3\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4", "6c:f3:7f:ae:8c:f5"};
        int[] freqs = {2437, 5180, 2414};
        String[] caps = {"[WPA2-EAP-CCMP][ESS]", "[ESS]", "[RSN-OWE-CCMP][ESS]"};
        int[] levels = {mThresholdMinimumRssi2G, mThresholdMinimumRssi5G + RSSI_BUMP,
                mThresholdMinimumRssi2G + RSSI_BUMP};
        mDummyNominator.setNominatorToSelectCandidate(false);
        when(mWifiNative.getSupportedFeatureSet(anyString()))
                .thenReturn(new Long(WIFI_FEATURE_OWE));

        List<ScanDetail> scanDetails = WifiNetworkSelectorTestUtil.buildScanDetails(
                ssids, bssids, freqs, caps, levels, mClock);
        HashSet<String> blacklist = new HashSet<>();

        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        List<ScanDetail> expectedOpenUnsavedNetworks = new ArrayList<>();
        expectedOpenUnsavedNetworks.add(scanDetails.get(1));
        expectedOpenUnsavedNetworks.add(scanDetails.get(2));
        assertEquals("Expect open unsaved networks",
                expectedOpenUnsavedNetworks,
                mWifiNetworkSelector.getFilteredScanDetailsForOpenUnsavedNetworks());
    }

    /**
     * {@link WifiNetworkSelector#getFilteredScanDetailsForOpenUnsavedNetworks()} for device that
     * does not support enhanced open networks, should filter out both networks that are not open
     * and enhanced open after network selection is made.
     *
     * Expected behavior: return open networks only
     */
    @Test
    public void getfilterOpenUnsavedNetworks_filtersForOpenAndOweNetworksOweNotSupported() {
        String[] ssids = {"\"test1\"", "\"test2\"", "\"test3\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4", "6c:f3:7f:ae:8c:f5"};
        int[] freqs = {2437, 5180, 2414};
        String[] caps = {"[WPA2-EAP-CCMP][ESS]", "[ESS]", "[RSN-OWE-CCMP][ESS]"};
        int[] levels = {mThresholdMinimumRssi2G, mThresholdMinimumRssi5G + RSSI_BUMP,
                mThresholdMinimumRssi2G + RSSI_BUMP};
        mDummyNominator.setNominatorToSelectCandidate(false);
        when(mWifiNative.getSupportedFeatureSet(anyString()))
                .thenReturn(new Long(~WIFI_FEATURE_OWE));

        List<ScanDetail> scanDetails = WifiNetworkSelectorTestUtil.buildScanDetails(
                ssids, bssids, freqs, caps, levels, mClock);
        HashSet<String> blacklist = new HashSet<>();

        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        List<ScanDetail> expectedOpenUnsavedNetworks = new ArrayList<>();
        expectedOpenUnsavedNetworks.add(scanDetails.get(1));
        assertEquals("Expect open unsaved networks",
                expectedOpenUnsavedNetworks,
                mWifiNetworkSelector.getFilteredScanDetailsForOpenUnsavedNetworks());
    }

    /**
     * Test that registering a new CandidateScorer causes it to be used
     */
    @Test
    public void testCandidateScorerUse() throws Exception {
        String myid = "Mock CandidateScorer";
        when(mCandidateScorer.getIdentifier()).thenReturn(myid);
        setupWifiConfigManager(13);

        int experimentId = experimentIdFromIdentifier(myid);
        assertTrue("" + myid, 42000000 <=  experimentId && experimentId <= 42999999);
        String diagnose = "" + mScoringParams + " // " + experimentId;
        assertTrue(diagnose, mScoringParams.update("expid=" + experimentId));
        assertEquals(experimentId, mScoringParams.getExperimentIdentifier());

        mWifiNetworkSelector.registerCandidateScorer(mCandidateScorer);

        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                setUpTwoNetworks(-35, -40),
                EMPTY_BLACKLIST, mWifiInfo, false, true, true);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);

        verify(mCandidateScorer, atLeastOnce()).scoreCandidates(any());
    }

    /**
     * Tests that no metrics are recorded if there is only a single legacy scorer.
     */
    @Test
    public void testCandidateScorerMetrics_onlyOneScorer() {
        testNoActiveStream();

        verify(mWifiMetrics, never()).logNetworkSelectionDecision(
                anyInt(), anyInt(), anyBoolean(), anyInt());
    }

    private static final WifiCandidates.CandidateScorer NULL_SCORER =
            new WifiCandidates.CandidateScorer() {
                @Override
                public String getIdentifier() {
                    return "NULL_SCORER";
                }

                @Override
                public WifiCandidates.ScoredCandidate scoreCandidates(
                        Collection<WifiCandidates.Candidate> group) {
                    return new WifiCandidates.ScoredCandidate(0, 0, false, null);
                }
            };

    private List<ScanDetail> setUpTwoNetworks(int rssiNetwork1, int rssiNetwork2) {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {5180, 2437};
        String[] caps = {"[ESS]", "[ESS]"};
        int[] levels = {rssiNetwork1, rssiNetwork2};
        int[] securities = {SECURITY_NONE, SECURITY_NONE};
        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                        freqs, caps, levels, securities, mWifiConfigManager, mClock);
        return scanDetailsAndConfigs.getScanDetails();
    }

    /**
     * Tests that metrics are recorded for 3 scorers.
     */
    @Test
    public void testCandidateScorerMetrics_threeScorers() {
        mWifiNetworkSelector.registerCandidateScorer(mCompatibilityScorer);
        mWifiNetworkSelector.registerCandidateScorer(NULL_SCORER);

        // add a second NetworkNominator that returns the second network in the scan list
        mWifiNetworkSelector.registerNetworkNominator(
                new DummyNetworkNominator(1, DUMMY_NOMINATOR_ID_2));

        int compatibilityExpId = experimentIdFromIdentifier(mCompatibilityScorer.getIdentifier());
        mScoringParams.update("expid=" + compatibilityExpId);
        assertEquals(compatibilityExpId, mScoringParams.getExperimentIdentifier());

        testNoActiveStream();

        int nullScorerId = experimentIdFromIdentifier(NULL_SCORER.getIdentifier());

        // Wanted 2 times since testNoActiveStream() calls
        // WifiNetworkSelector.selectNetwork() twice
        verify(mWifiMetrics, times(2)).logNetworkSelectionDecision(nullScorerId,
                compatibilityExpId, false, 2);

        int expid = CompatibilityScorer.COMPATIBILITY_SCORER_DEFAULT_EXPID;
        verify(mWifiMetrics, atLeastOnce()).setNetworkSelectorExperimentId(eq(expid));
    }

    /**
     * Tests that metrics are recorded for two scorers.
     */
    @Test
    public void testCandidateScorerMetricsThroughputScorer() {
        if (WifiNetworkSelector.PRESET_CANDIDATE_SCORER_NAME.equals(
                mThroughputScorer.getIdentifier())) {
            mWifiNetworkSelector.registerCandidateScorer(mCompatibilityScorer);
            return; //TODO(b/142081306) temporarily disabled
        } else {
            mWifiNetworkSelector.registerCandidateScorer(mThroughputScorer);
        }

        // add a second NetworkNominator that returns the second network in the scan list
        mWifiNetworkSelector.registerNetworkNominator(
                new DummyNetworkNominator(1, DUMMY_NOMINATOR_ID_2));

        testNoActiveStream();

        int throughputExpId = experimentIdFromIdentifier(mThroughputScorer.getIdentifier());
        int compatibilityExpId = experimentIdFromIdentifier(mCompatibilityScorer.getIdentifier());

        // Wanted 2 times since testNoActiveStream() calls
        // WifiNetworkSelector.selectNetwork() twice
        if (WifiNetworkSelector.PRESET_CANDIDATE_SCORER_NAME.equals(
                mThroughputScorer.getIdentifier())) {
            verify(mWifiMetrics, times(2)).logNetworkSelectionDecision(
                    compatibilityExpId, throughputExpId, true, 2);
        } else {
            verify(mWifiMetrics, times(2)).logNetworkSelectionDecision(throughputExpId,
                    compatibilityExpId, true, 2);
        }
    }

    /**
     * Tests that passpoint network candidate will update SSID with the latest scanDetail.
     */
    @Test
    public void testPasspointCandidateUpdateWithLatestScanDetail() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2437, 5180};
        String[] caps = {"[WPA2-EAP-CCMP][ESS]", "[WPA2-EAP-CCMP][ESS]"};
        int[] levels = {mThresholdMinimumRssi2G + 1, mThresholdMinimumRssi5G + 1};
        int[] securities = {SECURITY_EAP, SECURITY_EAP};
        HashSet<String> blacklist = new HashSet<>();
        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                        freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        WifiConfiguration[] configs = scanDetailsAndConfigs.getWifiConfigs();
        WifiConfiguration existingConfig = WifiConfigurationTestUtil.createPasspointNetwork();
        existingConfig.SSID = ssids[1];
        // Matched wifiConfig is an passpoint network with SSID from last scan.
        when(mWifiConfigManager.getConfiguredNetwork(configs[0].networkId))
                .thenReturn(existingConfig);
        mWifiNetworkSelector.registerNetworkNominator(
                new DummyNetworkNominator(0, DUMMY_NOMINATOR_ID_2));
        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, true);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        // Check if the wifiConfig is updated with the latest
        verify(mWifiConfigManager).addOrUpdateNetwork(existingConfig,
                existingConfig.creatorUid, existingConfig.creatorName);
        assertEquals(ssids[0], candidate.SSID);
    }

    @Test
    public void testIsFromCarrierOrPrivilegedApp() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2437, 5180};
        String[] caps = {"[WPA2-EAP-CCMP][ESS]", "[WPA2-EAP-CCMP][ESS]"};
        int[] levels = {mThresholdMinimumRssi2G + 1, mThresholdMinimumRssi5G + 1};
        int[] securities = {SECURITY_EAP, SECURITY_EAP};
        HashSet<String> blacklist = new HashSet<>();
        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                        freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        WifiConfiguration[] configs = scanDetailsAndConfigs.getWifiConfigs();
        // Mark one of the networks as carrier privileged.
        configs[0].fromWifiNetworkSuggestion = true;
        configs[0].carrierId = 5;
        mWifiNetworkSelector.registerNetworkNominator(
                new AllNetworkNominator(scanDetailsAndConfigs));
        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, true);
        // Expect one privileged and one regular candidate.
        assertEquals(2, candidates.size());
        boolean foundCarrierOrPrivilegedAppCandidate = false;
        boolean foundNotCarrierOrPrivilegedAppCandidate = false;
        for (WifiCandidates.Candidate candidate : candidates) {
            if (candidate.isCarrierOrPrivileged()) {
                foundCarrierOrPrivilegedAppCandidate = true;
            } else {
                foundNotCarrierOrPrivilegedAppCandidate = true;
            }
        }
        assertTrue(foundCarrierOrPrivilegedAppCandidate);
        assertTrue(foundNotCarrierOrPrivilegedAppCandidate);
    }

    /**
     * Test that network which are not accepting new connections(MBO
     * association disallowed attribute in beacons/probe responses)
     * are filtered out from network selection.
     *
     * NetworkDetail contain the parsed association disallowed
     * reason code.
     *
     * Expected behavior: no network recommended by Network Selector
     */
    @Test
    public void filterMboApAdvertisingAssociationDisallowedAttr() {
        String[] ssids = {"\"test1\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3"};
        int[] freqs = {5180};
        String[] caps = {"[WPA2-PSK][ESS]"};
        int[] levels = {mThresholdQualifiedRssi5G + 8};
        int[] securities = {SECURITY_PSK};
        // MBO-OCE IE with association disallowed attribute.
        byte[][] iesByteStream = {{(byte) 0xdd, (byte) 0x0a,
                        (byte) 0x50, (byte) 0x6F, (byte) 0x9A, (byte) 0x16,
                        (byte) 0x01, (byte) 0x01, (byte) 0x40,
                        (byte) 0x04, (byte) 0x01, (byte) 0x03}};
        HashSet<String> blacklist = new HashSet<String>();

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                    freqs, caps, levels, securities, mWifiConfigManager, mClock, iesByteStream);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();

        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetails, blacklist, mWifiInfo, false, true, false);
        WifiConfiguration candidate = mWifiNetworkSelector.selectNetwork(candidates);
        verify(mWifiMetrics, times(1))
                .incrementNetworkSelectionFilteredBssidCountDueToMboAssocDisallowInd();
        assertEquals("Expect null configuration", null, candidate);
        assertTrue(mWifiNetworkSelector.getConnectableScanDetails().isEmpty());
    }

    @Test
    public void resetOnDisableCallsClearLastSelectedNetwork() {
        mWifiNetworkSelector.resetOnDisable();
        verify(mWifiConfigManager).clearLastSelectedNetwork();
        assertEquals(0, mWifiNetworkSelector.getKnownMeteredNetworkIds().size());
    }

    @Test
    public void meteredStickyness() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2437, 2412};
        String[] caps = {"[WPA2-PSK][ESS]", "[WPA2-EAP-CCMP][ESS]"};
        int[] levels = {mThresholdMinimumRssi2G + 1, mThresholdMinimumRssi2G + 1};
        int[] securities = {SECURITY_PSK, SECURITY_EAP};

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                        freqs, caps, levels, securities, mWifiConfigManager, mClock);

        // Nominate all of these networks
        mWifiNetworkSelector.registerNetworkNominator(
                new AllNetworkNominator(scanDetailsAndConfigs));

        // Check setup
        assertEquals(0, mWifiNetworkSelector.getKnownMeteredNetworkIds().size());

        // No metered networks, expect no sticky bits
        runNetworkSelectionWith(scanDetailsAndConfigs);
        assertEquals(0, mWifiNetworkSelector.getKnownMeteredNetworkIds().size());

        // Encountering a metered network should get recorded
        scanDetailsAndConfigs.getWifiConfigs()[1].meteredHint = true;
        runNetworkSelectionWith(scanDetailsAndConfigs);
        HashSet<Integer> expect = new HashSet<>();
        expect.add(scanDetailsAndConfigs.getWifiConfigs()[1].networkId);
        assertEquals(expect, mWifiNetworkSelector.getKnownMeteredNetworkIds());

        // Override to unmetered should cause sticky removal
        // Make the other one metered this time, using hint
        scanDetailsAndConfigs.getWifiConfigs()[1].meteredOverride =
                WifiConfiguration.METERED_OVERRIDE_NOT_METERED;
        scanDetailsAndConfigs.getWifiConfigs()[0].meteredHint = true;
        runNetworkSelectionWith(scanDetailsAndConfigs);
        expect.clear();
        expect.add(scanDetailsAndConfigs.getWifiConfigs()[0].networkId);
        assertEquals(expect, mWifiNetworkSelector.getKnownMeteredNetworkIds());

        // Override to metered should also cause sticky removal
        scanDetailsAndConfigs.getWifiConfigs()[0].meteredOverride =
                WifiConfiguration.METERED_OVERRIDE_METERED;
        runNetworkSelectionWith(scanDetailsAndConfigs);
        assertEquals(0, mWifiNetworkSelector.getKnownMeteredNetworkIds().size());

        // Need to make sticky list nonempty
        scanDetailsAndConfigs.getWifiConfigs()[0].meteredOverride =
                WifiConfiguration.METERED_OVERRIDE_NONE;
        scanDetailsAndConfigs.getWifiConfigs()[0].meteredHint = true;
        runNetworkSelectionWith(scanDetailsAndConfigs);
        assertEquals(1, mWifiNetworkSelector.getKnownMeteredNetworkIds().size());
        // Toggling wifi off should clear the sticky bits
        mWifiNetworkSelector.resetOnDisable();
        assertEquals(0, mWifiNetworkSelector.getKnownMeteredNetworkIds().size());
    }

    private void runNetworkSelectionWith(ScanDetailsAndWifiConfigs scanDetailsAndConfigs) {
        List<WifiCandidates.Candidate> candidates = mWifiNetworkSelector.getCandidatesFromScan(
                scanDetailsAndConfigs.getScanDetails(),
                new HashSet<>(), // blocklist
                mWifiInfo, // wifiInfo
                false, // connected
                true, // disconnected
                true // untrustedNetworkAllowed
        );
        WifiConfiguration wifiConfiguration = mWifiNetworkSelector.selectNetwork(candidates);
        assertNotNull(wifiConfiguration);
    }
}
