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

import static com.android.server.wifi.ScoredNetworkNominator.SETTINGS_GLOBAL_USE_OPEN_WIFI_PACKAGE;
import static com.android.server.wifi.WifiConfigurationTestUtil.SECURITY_NONE;
import static com.android.server.wifi.WifiConfigurationTestUtil.SECURITY_PSK;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.database.ContentObserver;
import android.net.NetworkKey;
import android.net.NetworkScoreManager;
import android.net.Uri;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiConfiguration;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.LocalLog;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.WifiNetworkSelector.NetworkNominator.OnConnectableListener;
import com.android.server.wifi.WifiNetworkSelectorTestUtil.ScanDetailsAndWifiConfigs;
import com.android.server.wifi.util.WifiPermissionsUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * Unit tests for {@link ScoredNetworkNominator}.
 */
@SmallTest
public class ScoredNetworkNominatorTest extends WifiBaseTest {
    private static final String TEST_PACKAGE_NAME = "name.package.test";
    private static final int TEST_UID = 12345;
    private ContentObserver mContentObserver;
    private int mThresholdQualifiedRssi2G;
    private int mThresholdQualifiedRssi5G;

    @Mock private Context mContext;
    @Mock private Clock mClock;
    @Mock private FrameworkFacade mFrameworkFacade;
    @Mock private NetworkScoreManager mNetworkScoreManager;
    @Mock private PackageManager mPackageManager;
    @Mock private WifiConfigManager mWifiConfigManager;
    @Mock private WifiPermissionsUtil mWifiPermissionsUtil;
    @Mock private OnConnectableListener mOnConnectableListener;
    @Captor private ArgumentCaptor<Collection<NetworkKey>> mNetworkKeyCollectionCaptor;
    @Captor private ArgumentCaptor<WifiConfiguration> mWifiConfigCaptor;

    private WifiNetworkScoreCache mScoreCache;
    private ScoredNetworkNominator mScoredNetworkNominator;

    @Before
    public void setUp() throws Exception {
        mThresholdQualifiedRssi2G = -73;
        mThresholdQualifiedRssi5G = -70;

        MockitoAnnotations.initMocks(this);

        when(mFrameworkFacade.getStringSetting(mContext,
                SETTINGS_GLOBAL_USE_OPEN_WIFI_PACKAGE))
                .thenReturn("test");
        ApplicationInfo appInfo = new ApplicationInfo();
        appInfo.uid = TEST_UID;
        when(mPackageManager.getApplicationInfo(eq(TEST_PACKAGE_NAME), anyInt()))
                .thenReturn(appInfo);
        when(mNetworkScoreManager.getActiveScorerPackage())
                .thenReturn(TEST_PACKAGE_NAME);

        ArgumentCaptor<ContentObserver> observerCaptor =
                ArgumentCaptor.forClass(ContentObserver.class);
        mScoreCache = new WifiNetworkScoreCache(mContext);
        mScoredNetworkNominator = new ScoredNetworkNominator(mContext,
                new Handler(Looper.getMainLooper()), mFrameworkFacade, mNetworkScoreManager,
                mPackageManager, mWifiConfigManager, new LocalLog(0), mScoreCache,
                mWifiPermissionsUtil);
        verify(mFrameworkFacade).registerContentObserver(eq(mContext), any(Uri.class), eq(false),
                observerCaptor.capture());
        mContentObserver = observerCaptor.getValue();

        when(mClock.getElapsedSinceBootMillis()).thenReturn(SystemClock.elapsedRealtime());
    }

    @After
    public void tearDown() {
        validateMockitoUsage();
    }

    @Test
    public void testUpdate_recommendationsDisabled() {
        String[] ssids = {"\"test1\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3"};
        int[] freqs = {2470};
        String[] caps = {"[WPA2-PSK][ESS]"};
        int[] levels = {mThresholdQualifiedRssi2G + 8};
        int[] securities = {SECURITY_PSK};

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs = WifiNetworkSelectorTestUtil
                .setupScanDetailsAndConfigStore(
                ssids, bssids, freqs, caps, levels, securities, mWifiConfigManager, mClock);

        when(mFrameworkFacade.getStringSetting(mContext,
                SETTINGS_GLOBAL_USE_OPEN_WIFI_PACKAGE))
                .thenReturn(null);

        mContentObserver.onChange(false /* unused */);

        mScoredNetworkNominator.update(scanDetailsAndConfigs.getScanDetails());

        verifyZeroInteractions(mNetworkScoreManager);
    }

    @Test
    public void testUpdate_emptyScanList() {
        String[] ssids = {"\"test1\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3"};
        int[] freqs = {2470};
        String[] caps = {"[WPA2-PSK][ESS]"};
        int[] levels = {mThresholdQualifiedRssi2G + 8};
        int[] securities = {SECURITY_PSK};

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs = WifiNetworkSelectorTestUtil
                .setupScanDetailsAndConfigStore(
                        ssids, bssids, freqs, caps, levels, securities, mWifiConfigManager, mClock);

        mScoredNetworkNominator.update(new ArrayList<ScanDetail>());

        verifyZeroInteractions(mNetworkScoreManager);
    }

    @Test
    public void testUpdate_allNetworksUnscored() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2470, 2437};
        String[] caps = {"[WPA2-PSK][ESS]", "[ESS]"};
        int[] securities = {SECURITY_PSK, SECURITY_NONE};
        int[] levels = {mThresholdQualifiedRssi2G + 8, mThresholdQualifiedRssi2G + 10};

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs = WifiNetworkSelectorTestUtil
                .setupScanDetailsAndConfigStore(
                        ssids, bssids, freqs, caps, levels, securities, mWifiConfigManager, mClock);

        mScoredNetworkNominator.update(scanDetailsAndConfigs.getScanDetails());

        verify(mNetworkScoreManager).requestScores(mNetworkKeyCollectionCaptor.capture());
        NetworkKey[] requestedScores =
                mNetworkKeyCollectionCaptor.getValue().toArray(new NetworkKey[0]);
        assertEquals(2, requestedScores.length);
        NetworkKey expectedNetworkKey = NetworkKey.createFromScanResult(
                scanDetailsAndConfigs.getScanDetails().get(0).getScanResult());
        assertEquals(expectedNetworkKey, requestedScores[0]);
        expectedNetworkKey = NetworkKey.createFromScanResult(
                scanDetailsAndConfigs.getScanDetails().get(1).getScanResult());
        assertEquals(expectedNetworkKey, requestedScores[1]);
    }

    @Test
    public void testUpdate_oneScored_oneUnscored() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2470, 2437};
        String[] caps = {"[WPA2-PSK][ESS]", "[ESS]"};
        int[] securities = {SECURITY_PSK, SECURITY_NONE};
        int[] levels = {mThresholdQualifiedRssi2G + 8, mThresholdQualifiedRssi2G + 10};

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs = WifiNetworkSelectorTestUtil
                .setupScanDetailsAndConfigStore(
                        ssids, bssids, freqs, caps, levels, securities, mWifiConfigManager, mClock);

        List<ScanDetail> scoredScanDetails = scanDetailsAndConfigs.getScanDetails().subList(0, 1);
        Integer[] scores = {120};
        boolean[] meteredHints = {true};
        WifiNetworkSelectorTestUtil.configureScoreCache(
                mScoreCache, scoredScanDetails, scores, meteredHints);

        mScoredNetworkNominator.update(scanDetailsAndConfigs.getScanDetails());

        verify(mNetworkScoreManager).requestScores(mNetworkKeyCollectionCaptor.capture());

        NetworkKey[] requestedScores =
                mNetworkKeyCollectionCaptor.getValue().toArray(new NetworkKey[0]);
        assertEquals(1, requestedScores.length);
        NetworkKey expectedNetworkKey = NetworkKey.createFromScanResult(
                scanDetailsAndConfigs.getScanDetails().get(1).getScanResult());
        assertEquals(expectedNetworkKey, requestedScores[0]);
    }

    @Test
    public void testEvaluateNetworks_recommendationsDisabled() {
        when(mFrameworkFacade.getStringSetting(mContext,
                SETTINGS_GLOBAL_USE_OPEN_WIFI_PACKAGE))
                .thenReturn(null);

        mContentObserver.onChange(false /* unused */);

        mScoredNetworkNominator.nominateNetworks(null, null, null, false, false,
                mOnConnectableListener);

        verifyZeroInteractions(mWifiConfigManager, mNetworkScoreManager);
    }

    @Test
    public void testUpdate_externalScorerNotPermittedToSeeScanResults() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2470, 2437};
        String[] caps = {"[WPA2-PSK][ESS]", "[ESS]"};
        int[] securities = {SECURITY_PSK, SECURITY_NONE};
        int[] levels = {mThresholdQualifiedRssi2G + 8, mThresholdQualifiedRssi2G + 10};

        doThrow(new SecurityException()).when(mWifiPermissionsUtil).enforceCanAccessScanResults(
                any(), any(), anyInt(), any());

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs = WifiNetworkSelectorTestUtil
                .setupScanDetailsAndConfigStore(
                        ssids, bssids, freqs, caps, levels, securities, mWifiConfigManager, mClock);

        mScoredNetworkNominator.update(scanDetailsAndConfigs.getScanDetails());

        verify(mNetworkScoreManager, never()).requestScores(anyCollection());
        verify(mWifiPermissionsUtil).enforceCanAccessScanResults(
                eq(TEST_PACKAGE_NAME), eq(null), eq(TEST_UID), nullable(String.class));
    }

    @Test
    public void testUpdate_externalScorerNotPermittedToSeeScanResultsWithException() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2470, 2437};
        String[] caps = {"[WPA2-PSK][ESS]", "[ESS]"};
        int[] securities = {SECURITY_PSK, SECURITY_NONE};
        int[] levels = {mThresholdQualifiedRssi2G + 8, mThresholdQualifiedRssi2G + 10};

        doThrow(new SecurityException()).when(mWifiPermissionsUtil).enforceCanAccessScanResults(
                any(), any(), anyInt(), any());

        ScanDetailsAndWifiConfigs scanDetailsAndConfigs = WifiNetworkSelectorTestUtil
                .setupScanDetailsAndConfigStore(
                        ssids, bssids, freqs, caps, levels, securities, mWifiConfigManager, mClock);

        mScoredNetworkNominator.update(scanDetailsAndConfigs.getScanDetails());

        verify(mNetworkScoreManager, never()).requestScores(anyCollection());
        verify(mWifiPermissionsUtil).enforceCanAccessScanResults(
                eq(TEST_PACKAGE_NAME), eq(null), eq(TEST_UID), nullable(String.class));
    }

    /**
     * When we have created a new ephemeral network, make sure that mOnConnectableListener
     * is called.
     */
    @Test
    public void testEvaluateNetworks_newEphemeralNetworkMustBeReportedAsConnectable() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2470, 2437};
        String[] caps = {"[WPA2-PSK][ESS]", "[ESS]"};
        int[] levels = {mThresholdQualifiedRssi2G + 8, mThresholdQualifiedRssi2G + 10};
        Integer[] scores = {null, 120};
        boolean[] meteredHints = {false, false};
        List<ScanDetail> scanDetails = WifiNetworkSelectorTestUtil.buildScanDetails(
                ssids, bssids, freqs, caps, levels, mClock);
        WifiNetworkSelectorTestUtil.configureScoreCache(mScoreCache,
                scanDetails, scores, meteredHints);
        ScanResult scanResult = scanDetails.get(1).getScanResult();
        WifiConfiguration ephemeralNetworkConfig = WifiNetworkSelectorTestUtil
                .setupEphemeralNetwork(mWifiConfigManager, 1, scanDetails.get(1), meteredHints[1]);
        // No saved networks.
        when(mWifiConfigManager.getConfiguredNetworkForScanDetailAndCache(any(ScanDetail.class)))
                .thenReturn(null);
        // But when we create one, this is should be it.
        when(mWifiConfigManager.addOrUpdateNetwork(any(), eq(TEST_UID), eq(TEST_PACKAGE_NAME)))
                .thenReturn(new NetworkUpdateResult(1));
        // Untrusted networks allowed.
        mScoredNetworkNominator.nominateNetworks(scanDetails,
                null, null, false, true, mOnConnectableListener);
        verify(mOnConnectableListener, atLeastOnce())
                .onConnectable(any(), mWifiConfigCaptor.capture());
        assertTrue(mWifiConfigCaptor.getAllValues().stream()
                .anyMatch(c -> c.networkId == ephemeralNetworkConfig.networkId));
    }

    /**
     * Don't choose available ephemeral networks if no saved networks and untrusted networks
     * are not allowed.
     */
    @Test
    public void testEvaluateNetworks_noEphemeralNetworkWhenUntrustedNetworksNotAllowed() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2470, 2437};
        String[] caps = {"[WPA2-PSK][ESS]", "[ESS]"};
        int[] levels = {mThresholdQualifiedRssi2G + 8, mThresholdQualifiedRssi2G + 10};
        Integer[] scores = {null, 120};
        boolean[] meteredHints = {false, true};

        List<ScanDetail> scanDetails = WifiNetworkSelectorTestUtil.buildScanDetails(
                ssids, bssids, freqs, caps, levels, mClock);
        WifiNetworkSelectorTestUtil.configureScoreCache(mScoreCache,
                scanDetails, scores, meteredHints);

        // No saved networks.
        when(mWifiConfigManager.getConfiguredNetworkForScanDetailAndCache(any(ScanDetail.class)))
                .thenReturn(null);

        WifiNetworkSelectorTestUtil.setupEphemeralNetwork(
                mWifiConfigManager, 1, scanDetails.get(1), meteredHints[1]);

        // Untrusted networks not allowed.
        mScoredNetworkNominator.nominateNetworks(scanDetails,
                null, null, false, false, mOnConnectableListener);

        verify(mOnConnectableListener, never()).onConnectable(any(), any());
    }

    /**
     * Choose externally scored saved network.
     */
    @Test
    public void testEvaluateNetworks_chooseSavedNetworkWithExternalScore() {
        String[] ssids = {"\"test1\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3"};
        int[] freqs = {5200};
        String[] caps = {"[WPA2-PSK][ESS]"};
        int[] securities = {SECURITY_PSK};
        int[] levels = {mThresholdQualifiedRssi5G + 8};
        Integer[] scores = {120};
        boolean[] meteredHints = {false};

        WifiNetworkSelectorTestUtil.ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                        freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        WifiConfiguration[] savedConfigs = scanDetailsAndConfigs.getWifiConfigs();
        savedConfigs[0].useExternalScores = true;

        WifiNetworkSelectorTestUtil.configureScoreCache(mScoreCache,
                scanDetails, scores, meteredHints);

        mScoredNetworkNominator.nominateNetworks(scanDetails,
                null, null, false, true, mOnConnectableListener);

        verify(mOnConnectableListener).onConnectable(any(), mWifiConfigCaptor.capture());
        assertEquals(mWifiConfigCaptor.getValue().networkId, savedConfigs[0].networkId);
    }

    /**
     * Prefer externally scored saved network over untrusted network when they have
     * the same score.
     */
    @Test
    public void testEvaluateNetworks_nullScoredNetworks() {
        String[] ssids = {"\"test1\"", "\"test2\""};
        String[] bssids = {"6c:f3:7f:ae:8c:f3", "6c:f3:7f:ae:8c:f4"};
        int[] freqs = {2470, 2437};
        String[] caps = {"[WPA2-PSK][ESS]", "[ESS]"};
        int[] securities = {SECURITY_PSK, SECURITY_NONE};
        int[] levels = {mThresholdQualifiedRssi2G + 8, mThresholdQualifiedRssi2G + 8};
        Integer[] scores = {null, null};
        boolean[] meteredHints = {false, true};

        WifiNetworkSelectorTestUtil.ScanDetailsAndWifiConfigs scanDetailsAndConfigs =
                WifiNetworkSelectorTestUtil.setupScanDetailsAndConfigStore(ssids, bssids,
                        freqs, caps, levels, securities, mWifiConfigManager, mClock);
        List<ScanDetail> scanDetails = scanDetailsAndConfigs.getScanDetails();
        WifiConfiguration[] savedConfigs = scanDetailsAndConfigs.getWifiConfigs();
        savedConfigs[0].useExternalScores = true;

        WifiNetworkSelectorTestUtil.configureScoreCache(mScoreCache,
                scanDetails, scores, meteredHints);

        mScoredNetworkNominator.nominateNetworks(scanDetails,
                null, null, false, true, mOnConnectableListener);

        verify(mOnConnectableListener).onConnectable(any(), mWifiConfigCaptor.capture());
        assertEquals(mWifiConfigCaptor.getValue().networkId, savedConfigs[0].networkId);
    }
}
