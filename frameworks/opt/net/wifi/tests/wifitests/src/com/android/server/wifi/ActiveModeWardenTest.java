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

import static com.android.server.wifi.ActiveModeManager.ROLE_CLIENT_PRIMARY;
import static com.android.server.wifi.ActiveModeManager.ROLE_CLIENT_SCAN_ONLY;
import static com.android.server.wifi.ActiveModeManager.ROLE_SOFTAP_LOCAL_ONLY;
import static com.android.server.wifi.ActiveModeManager.ROLE_SOFTAP_TETHERED;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.mockingDetails;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Resources;
import android.location.LocationManager;
import android.net.wifi.SoftApCapability;
import android.net.wifi.SoftApConfiguration;
import android.net.wifi.SoftApConfiguration.Builder;
import android.net.wifi.SoftApInfo;
import android.net.wifi.WifiClient;
import android.net.wifi.WifiManager;
import android.os.BatteryStatsManager;
import android.os.test.TestLooper;
import android.util.Log;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.WifiNative.InterfaceAvailableForRequestListener;
import com.android.server.wifi.util.GeneralUtil;
import com.android.server.wifi.util.WifiPermissionsUtil;
import com.android.wifi.resources.R;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import java.io.ByteArrayOutputStream;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.stream.Collectors;

/**
 * Unit tests for {@link com.android.server.wifi.ActiveModeWarden}.
 */
@SmallTest
public class ActiveModeWardenTest extends WifiBaseTest {
    public static final String TAG = "WifiActiveModeWardenTest";

    private static final String ENABLED_STATE_STRING = "EnabledState";
    private static final String DISABLED_STATE_STRING = "DisabledState";

    private static final String WIFI_IFACE_NAME = "mockWlan";
    private static final int TEST_WIFI_RECOVERY_DELAY_MS = 2000;
    private static final int TEST_AP_FREQUENCY = 2412;
    private static final int TEST_AP_BANDWIDTH = SoftApInfo.CHANNEL_WIDTH_20MHZ;

    TestLooper mLooper;
    @Mock WifiInjector mWifiInjector;
    @Mock Context mContext;
    @Mock Resources mResources;
    @Mock WifiNative mWifiNative;
    @Mock WifiApConfigStore mWifiApConfigStore;
    @Mock ClientModeManager mClientModeManager;
    @Mock SoftApManager mSoftApManager;
    @Mock DefaultModeManager mDefaultModeManager;
    @Mock BatteryStatsManager mBatteryStats;
    @Mock SelfRecovery mSelfRecovery;
    @Mock BaseWifiDiagnostics mWifiDiagnostics;
    @Mock ScanRequestProxy mScanRequestProxy;
    @Mock ClientModeImpl mClientModeImpl;
    @Mock FrameworkFacade mFacade;
    @Mock WifiSettingsStore mSettingsStore;
    @Mock WifiPermissionsUtil mWifiPermissionsUtil;
    @Mock SoftApCapability mSoftApCapability;

    ActiveModeManager.Listener mClientListener;
    ActiveModeManager.Listener mSoftApListener;
    WifiManager.SoftApCallback mSoftApManagerCallback;
    SoftApModeConfiguration mSoftApConfig;
    @Mock WifiManager.SoftApCallback mSoftApStateMachineCallback;
    @Mock WifiManager.SoftApCallback mLohsStateMachineCallback;
    WifiNative.StatusListener mWifiNativeStatusListener;
    ActiveModeWarden mActiveModeWarden;
    private SoftApInfo mTestSoftApInfo;

    final ArgumentCaptor<WifiNative.StatusListener> mStatusListenerCaptor =
            ArgumentCaptor.forClass(WifiNative.StatusListener.class);
    final ArgumentCaptor<InterfaceAvailableForRequestListener> mClientIfaceAvailableListener =
            ArgumentCaptor.forClass(InterfaceAvailableForRequestListener.class);
    final ArgumentCaptor<InterfaceAvailableForRequestListener> mSoftApIfaceAvailableListener =
            ArgumentCaptor.forClass(InterfaceAvailableForRequestListener.class);

    /**
     * Set up the test environment.
     */
    @Before
    public void setUp() throws Exception {
        Log.d(TAG, "Setting up ...");

        MockitoAnnotations.initMocks(this);
        mLooper = new TestLooper();

        when(mWifiInjector.getScanRequestProxy()).thenReturn(mScanRequestProxy);
        when(mClientModeManager.getRole()).thenReturn(ROLE_CLIENT_PRIMARY);
        when(mContext.getResources()).thenReturn(mResources);
        when(mSoftApManager.getRole()).thenReturn(ROLE_SOFTAP_TETHERED);

        when(mResources.getString(R.string.wifi_localhotspot_configure_ssid_default))
                .thenReturn("AndroidShare");
        when(mResources.getInteger(R.integer.config_wifi_framework_recovery_timeout_delay))
                .thenReturn(TEST_WIFI_RECOVERY_DELAY_MS);
        when(mResources.getBoolean(R.bool.config_wifiScanHiddenNetworksScanOnlyMode))
                .thenReturn(false);

        when(mSettingsStore.isWifiToggleEnabled()).thenReturn(false);
        when(mSettingsStore.isAirplaneModeOn()).thenReturn(false);
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(false);
        when(mWifiPermissionsUtil.isLocationModeEnabled()).thenReturn(true);

        doAnswer(new Answer<ClientModeManager>() {
            public ClientModeManager answer(InvocationOnMock invocation) {
                Object[] args = invocation.getArguments();
                mClientListener = (ClientModeManager.Listener) args[0];
                return mClientModeManager;
            }
        }).when(mWifiInjector).makeClientModeManager(any(ActiveModeManager.Listener.class));
        doAnswer(new Answer<SoftApManager>() {
            public SoftApManager answer(InvocationOnMock invocation) {
                Object[] args = invocation.getArguments();
                mSoftApListener = (ActiveModeManager.Listener) args[0];
                mSoftApManagerCallback = (WifiManager.SoftApCallback) args[1];
                mSoftApConfig = (SoftApModeConfiguration) args[2];
                return mSoftApManager;
            }
        }).when(mWifiInjector).makeSoftApManager(any(ActiveModeManager.Listener.class),
                any(WifiManager.SoftApCallback.class), any());

        mActiveModeWarden = createActiveModeWarden();
        mActiveModeWarden.start();
        mLooper.dispatchAll();

        verify(mWifiNative).registerStatusListener(mStatusListenerCaptor.capture());
        mWifiNativeStatusListener = mStatusListenerCaptor.getValue();

        verify(mWifiNative).registerClientInterfaceAvailabilityListener(
                mClientIfaceAvailableListener.capture());
        verify(mWifiNative).registerSoftApInterfaceAvailabilityListener(
                mSoftApIfaceAvailableListener.capture());

        mActiveModeWarden.registerSoftApCallback(mSoftApStateMachineCallback);
        mActiveModeWarden.registerLohsCallback(mLohsStateMachineCallback);
        mTestSoftApInfo = new SoftApInfo();
        mTestSoftApInfo.setFrequency(TEST_AP_FREQUENCY);
        mTestSoftApInfo.setBandwidth(TEST_AP_BANDWIDTH);
    }

    private ActiveModeWarden createActiveModeWarden() {
        ActiveModeWarden warden = new ActiveModeWarden(
                mWifiInjector,
                mLooper.getLooper(),
                mWifiNative,
                mDefaultModeManager,
                mBatteryStats,
                mWifiDiagnostics,
                mContext,
                mClientModeImpl,
                mSettingsStore,
                mFacade,
                mWifiPermissionsUtil);
        // SelfRecovery is created in WifiInjector after ActiveModeWarden, so getSelfRecovery()
        // returns null when constructing ActiveModeWarden.
        when(mWifiInjector.getSelfRecovery()).thenReturn(mSelfRecovery);
        return warden;
    }

    /**
     * Clean up after tests - explicitly set tested object to null.
     */
    @After
    public void cleanUp() throws Exception {
        mActiveModeWarden = null;
        mLooper.dispatchAll();
    }

    /**
     * Helper method to enter the EnabledState and set ClientModeManager in ConnectMode.
     */
    private void enterClientModeActiveState() throws Exception {
        String fromState = mActiveModeWarden.getCurrentMode();
        when(mClientModeManager.getRole()).thenReturn(ROLE_CLIENT_PRIMARY);
        when(mSettingsStore.isWifiToggleEnabled()).thenReturn(true);
        mActiveModeWarden.wifiToggled();
        mLooper.dispatchAll();
        mClientListener.onStarted();
        mLooper.dispatchAll();

        assertInEnabledState();
        verify(mClientModeManager).start();
        verify(mClientModeManager).setRole(ROLE_CLIENT_PRIMARY);
        verify(mScanRequestProxy).enableScanning(true, true);
        if (fromState.equals(DISABLED_STATE_STRING)) {
            verify(mBatteryStats).reportWifiOn();
        }
    }

    /**
     * Helper method to enter the EnabledState and set ClientModeManager in ScanOnlyMode.
     */
    private void enterScanOnlyModeActiveState() throws Exception {
        String fromState = mActiveModeWarden.getCurrentMode();
        when(mClientModeManager.getRole()).thenReturn(ROLE_CLIENT_SCAN_ONLY);
        when(mWifiPermissionsUtil.isLocationModeEnabled()).thenReturn(true);
        when(mSettingsStore.isAirplaneModeOn()).thenReturn(false);
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(true);
        when(mSettingsStore.isWifiToggleEnabled()).thenReturn(false);
        mActiveModeWarden.wifiToggled();
        mLooper.dispatchAll();
        mClientListener.onStarted();
        mLooper.dispatchAll();

        assertInEnabledState();
        verify(mClientModeManager).start();
        verify(mClientModeManager).setRole(ROLE_CLIENT_SCAN_ONLY);
        verify(mScanRequestProxy).enableScanning(true, false);
        if (fromState.equals(DISABLED_STATE_STRING)) {
            verify(mBatteryStats).reportWifiOn();
        }
        verify(mBatteryStats).reportWifiState(BatteryStatsManager.WIFI_STATE_OFF_SCANNING, null);
    }

    private void enterSoftApActiveMode() throws Exception {
        enterSoftApActiveMode(
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mSoftApCapability));
    }

    /**
     * Helper method to activate SoftApManager.
     *
     * This method puts the test object into the correct state and verifies steps along the way.
     */
    private void enterSoftApActiveMode(SoftApModeConfiguration softApConfig) throws Exception {
        String fromState = mActiveModeWarden.getCurrentMode();
        int softApRole = softApConfig.getTargetMode() == WifiManager.IFACE_IP_MODE_TETHERED
                ? ROLE_SOFTAP_TETHERED : ROLE_SOFTAP_LOCAL_ONLY;
        when(mSoftApManager.getRole()).thenReturn(softApRole);
        mActiveModeWarden.startSoftAp(softApConfig);
        mLooper.dispatchAll();
        mSoftApListener.onStarted();
        mLooper.dispatchAll();

        assertInEnabledState();
        assertThat(softApConfig).isEqualTo(mSoftApConfig);
        verify(mSoftApManager).start();
        verify(mSoftApManager).setRole(softApRole);
        if (fromState.equals(DISABLED_STATE_STRING)) {
            verify(mBatteryStats).reportWifiOn();
        }
    }

    private void enterStaDisabledMode(boolean isSoftApModeManagerActive) {
        String fromState = mActiveModeWarden.getCurrentMode();
        when(mSettingsStore.isWifiToggleEnabled()).thenReturn(false);
        when(mWifiPermissionsUtil.isLocationModeEnabled()).thenReturn(false);
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(false);
        mActiveModeWarden.wifiToggled();
        mLooper.dispatchAll();
        if (mClientListener != null) {
            mClientListener.onStopped();
            mLooper.dispatchAll();
        }

        if (isSoftApModeManagerActive) {
            assertInEnabledState();
        } else {
            assertInDisabledState();
        }
        if (fromState.equals(ENABLED_STATE_STRING)) {
            verify(mScanRequestProxy).enableScanning(false, false);
        }
    }

    private void shutdownWifi() {
        mActiveModeWarden.recoveryDisableWifi();
        mLooper.dispatchAll();
    }

    private void assertInEnabledState() {
        assertThat(mActiveModeWarden.getCurrentMode()).isEqualTo(ENABLED_STATE_STRING);
    }

    private void assertInDisabledState() {
        assertThat(mActiveModeWarden.getCurrentMode()).isEqualTo(DISABLED_STATE_STRING);
    }

    /**
     * Emergency mode is a sub-mode within each main state (ScanOnly, Client, DisabledState).
     */
    private void assertInEmergencyMode() {
        assertThat(mActiveModeWarden.isInEmergencyMode()).isTrue();
    }

    /**
     * Counts the number of times a void method was called on a mock.
     *
     * Void methods cannot be passed to Mockito.mockingDetails(). Thus we have to use method name
     * matching instead.
     */
    private static int getMethodInvocationCount(Object mock, String methodName) {
        long count = mockingDetails(mock).getInvocations()
                .stream()
                .filter(invocation -> methodName.equals(invocation.getMethod().getName()))
                .count();
        return (int) count;
    }

    /**
     * Counts the number of times a non-void method was called on a mock.
     *
     * For non-void methods, can pass the method call literal directly:
     * e.g. getMethodInvocationCount(mock.method());
     */
    private static int getMethodInvocationCount(Object mockMethod) {
        return mockingDetails(mockMethod).getInvocations().size();
    }

    private void assertWifiShutDown(Runnable r) {
        assertWifiShutDown(r, 1);
    }

    /**
     * Asserts that the runnable r has shut down wifi properly.
     *
     * @param r     runnable that will shut down wifi
     * @param times expected number of times that <code>r</code> shut down wifi
     */
    private void assertWifiShutDown(Runnable r, int times) {
        // take snapshot of ActiveModeManagers
        Collection<ActiveModeManager> activeModeManagers =
                mActiveModeWarden.getActiveModeManagers();

        List<Integer> expectedStopInvocationCounts = activeModeManagers
                .stream()
                .map(manager -> getMethodInvocationCount(manager, "stop") + times)
                .collect(Collectors.toList());

        r.run();

        List<Integer> actualStopInvocationCounts = activeModeManagers
                .stream()
                .map(manager -> getMethodInvocationCount(manager, "stop"))
                .collect(Collectors.toList());

        String managerNames = activeModeManagers.stream()
                .map(manager -> manager.getClass().getCanonicalName())
                .collect(Collectors.joining(", ", "[", "]"));

        assertThat(actualStopInvocationCounts)
                .named(managerNames)
                .isEqualTo(expectedStopInvocationCounts);
    }

    private void assertEnteredEcmMode(Runnable r) {
        assertEnteredEcmMode(r, 1);
    }

    /**
     * Asserts that the runnable r has entered ECM state properly.
     *
     * @param r     runnable that will enter ECM
     * @param times expected number of times that <code>r</code> shut down wifi
     */
    private void assertEnteredEcmMode(Runnable r, int times) {
        // take snapshot of ActiveModeManagers
        Collection<ActiveModeManager> activeModeManagers =
                mActiveModeWarden.getActiveModeManagers();

        boolean disableWifiInEcm = mFacade.getConfigWiFiDisableInECBM(mContext);

        List<Integer> expectedStopInvocationCounts = activeModeManagers.stream()
                .map(manager -> {
                    int initialCount = getMethodInvocationCount(manager, "stop");
                    // carrier config enabled, all mode managers should have been shut down once
                    int count = disableWifiInEcm ? initialCount + times : initialCount;
                    if (manager instanceof SoftApManager) {
                        // expect SoftApManager.close() to be called
                        return count + times;
                    } else {
                        // don't expect other Managers close() to be called
                        return count;
                    }
                })
                .collect(Collectors.toList());

        r.run();

        assertInEmergencyMode();

        List<Integer> actualStopInvocationCounts = activeModeManagers.stream()
                .map(manager -> getMethodInvocationCount(manager, "stop"))
                .collect(Collectors.toList());

        String managerNames = activeModeManagers.stream()
                .map(manager -> manager.getClass().getCanonicalName())
                .collect(Collectors.joining(", ", "[", "]"));

        assertThat(actualStopInvocationCounts)
                .named(managerNames)
                .isEqualTo(expectedStopInvocationCounts);
    }

    /** Test that after starting up, ActiveModeWarden is in the DisabledState State. */
    @Test
    public void testDisabledStateAtStartup() {
        assertInDisabledState();
    }

    /**
     * Test that ActiveModeWarden properly enters the EnabledState (in ScanOnlyMode) from the
     * DisabledState state.
     */
    @Test
    public void testEnterScanOnlyModeFromDisabled() throws Exception {
        enterScanOnlyModeActiveState();
    }

    /**
     * Test that ActiveModeWarden enables hidden network scanning in scan-only-mode
     * if configured to do.
     */
    @Test
    public void testScanOnlyModeScanHiddenNetworks() throws Exception {
        when(mResources.getBoolean(R.bool.config_wifiScanHiddenNetworksScanOnlyMode))
                .thenReturn(true);

        mActiveModeWarden = createActiveModeWarden();
        mActiveModeWarden.start();
        mLooper.dispatchAll();

        when(mClientModeManager.getRole()).thenReturn(ROLE_CLIENT_SCAN_ONLY);
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(true);
        when(mSettingsStore.isWifiToggleEnabled()).thenReturn(false);
        mActiveModeWarden.wifiToggled();
        mLooper.dispatchAll();
        mClientListener.onStarted();
        mLooper.dispatchAll();

        assertInEnabledState();
        verify(mClientModeManager).start();
        verify(mClientModeManager).setRole(ROLE_CLIENT_SCAN_ONLY);
        verify(mScanRequestProxy).enableScanning(true, true);
    }

    /**
     * Test that ActiveModeWarden properly starts the SoftApManager from the
     * DisabledState state.
     */
    @Test
    public void testEnterSoftApModeFromDisabled() throws Exception {
        enterSoftApActiveMode();
    }

    /**
     * Test that ActiveModeWarden properly starts the SoftApManager from another state.
     */
    @Test
    public void testEnterSoftApModeFromDifferentState() throws Exception {
        enterClientModeActiveState();
        assertInEnabledState();
        reset(mBatteryStats, mScanRequestProxy);
        enterSoftApActiveMode();
    }

    /**
     * Test that we can disable wifi fully from the EnabledState (in ScanOnlyMode).
     */
    @Test
    public void testDisableWifiFromScanOnlyModeActiveState() throws Exception {
        enterScanOnlyModeActiveState();

        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(false);
        mActiveModeWarden.scanAlwaysModeChanged();
        mLooper.dispatchAll();
        mClientListener.onStopped();
        mLooper.dispatchAll();

        verify(mClientModeManager).stop();
        verify(mBatteryStats).reportWifiOff();
        assertInDisabledState();
    }

    /**
     * Test that we can disable wifi when SoftApManager is active and not impact softap.
     */
    @Test
    public void testDisableWifiFromSoftApModeActiveStateDoesNotStopSoftAp() throws Exception {
        enterSoftApActiveMode();
        enterScanOnlyModeActiveState();

        reset(mDefaultModeManager);
        enterStaDisabledMode(true);
        verify(mSoftApManager, never()).stop();
        verify(mBatteryStats, never()).reportWifiOff();
    }

    /**
     * Test that we can switch from the EnabledState (in ScanOnlyMode) to another mode.
     */
    @Test
    public void testSwitchModeWhenScanOnlyModeActiveState() throws Exception {
        enterScanOnlyModeActiveState();

        reset(mBatteryStats, mScanRequestProxy);
        enterClientModeActiveState();
        mLooper.dispatchAll();
        verify(mClientModeManager).setRole(ROLE_CLIENT_PRIMARY);
        assertInEnabledState();
    }

    /**
     * Reentering EnabledState should be a NOP.
     */
    @Test
    public void testReenterClientModeActiveStateIsNop() throws Exception {
        enterClientModeActiveState();
        verify(mClientModeManager, times(1)).start();

        when(mSettingsStore.isWifiToggleEnabled()).thenReturn(true);
        mActiveModeWarden.wifiToggled();
        mLooper.dispatchAll();
        // Should not start again.
        verify(mClientModeManager, times(1)).start();
    }

    /**
     * Test that we can switch mode when SoftApManager is active to another mode.
     */
    @Test
    public void testSwitchModeWhenSoftApActiveMode() throws Exception {
        enterSoftApActiveMode();

        reset(mWifiNative);

        enterClientModeActiveState();
        mLooper.dispatchAll();
        verify(mSoftApManager, never()).stop();
        assertInEnabledState();
        verify(mWifiNative, never()).teardownAllInterfaces();
    }

    /**
     * Test that we activate SoftApModeManager if we are already in DisabledState due to
     * a failure.
     */
    @Test
    public void testEnterSoftApModeActiveWhenAlreadyInSoftApMode() throws Exception {
        enterSoftApActiveMode();
        // now inject failure through the SoftApManager.Listener
        mSoftApListener.onStartFailure();
        mLooper.dispatchAll();
        assertInDisabledState();
        // clear the first call to start SoftApManager
        reset(mSoftApManager, mBatteryStats);

        enterSoftApActiveMode();
    }

    /**
     * Test that we return to the DisabledState after a failure is reported when in the
     * EnabledState.
     */
    @Test
    public void testScanOnlyModeFailureWhenActive() throws Exception {
        enterScanOnlyModeActiveState();
        // now inject a failure through the ScanOnlyModeManager.Listener
        mClientListener.onStartFailure();
        mLooper.dispatchAll();
        assertInDisabledState();
        verify(mBatteryStats).reportWifiOff();
    }

    /**
     * Test that we return to the DisabledState after a failure is reported when
     * SoftApManager is active.
     */
    @Test
    public void testSoftApFailureWhenActive() throws Exception {
        enterSoftApActiveMode();
        // now inject failure through the SoftApManager.Listener
        mSoftApListener.onStartFailure();
        mLooper.dispatchAll();
        verify(mBatteryStats).reportWifiOff();
    }

    /**
     * Test that we return to the DisabledState after the ClientModeManager running in ScanOnlyMode
     * is stopped.
     */
    @Test
    public void testScanOnlyModeDisabledWhenActive() throws Exception {
        enterScanOnlyModeActiveState();

        // now inject the stop message through the ScanOnlyModeManager.Listener
        mClientListener.onStopped();
        mLooper.dispatchAll();

        assertInDisabledState();
        verify(mBatteryStats).reportWifiOff();
    }

    /**
     * Test that we return to the DisabledState after the SoftApManager is stopped.
     */
    @Test
    public void testSoftApDisabledWhenActive() throws Exception {
        enterSoftApActiveMode();
        reset(mWifiNative);
        // now inject failure through the SoftApManager.Listener
        mSoftApListener.onStartFailure();
        mLooper.dispatchAll();
        verify(mBatteryStats).reportWifiOff();
        verifyNoMoreInteractions(mWifiNative);
    }

    /**
     * Verifies that SoftApStateChanged event is being passed from SoftApManager to WifiServiceImpl
     */
    @Test
    public void callsWifiServiceCallbackOnSoftApStateChanged() throws Exception {
        enterSoftApActiveMode();

        mSoftApListener.onStarted();
        mSoftApManagerCallback.onStateChanged(WifiManager.WIFI_AP_STATE_ENABLED, 0);
        mLooper.dispatchAll();

        verify(mSoftApStateMachineCallback).onStateChanged(WifiManager.WIFI_AP_STATE_ENABLED, 0);
    }

    /**
     * Verifies that SoftApStateChanged event isn't passed to WifiServiceImpl for LOHS,
     * so the state change for LOHS doesn't affect Wifi Tethering indication.
     */
    @Test
    public void doesntCallWifiServiceCallbackOnLOHSStateChanged() throws Exception {
        enterSoftApActiveMode(new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_LOCAL_ONLY, null, mSoftApCapability));

        mSoftApListener.onStarted();
        mSoftApManagerCallback.onStateChanged(WifiManager.WIFI_AP_STATE_ENABLED, 0);
        mLooper.dispatchAll();

        verify(mSoftApStateMachineCallback, never()).onStateChanged(anyInt(), anyInt());
        verify(mSoftApStateMachineCallback, never()).onConnectedClientsChanged(any());
        verify(mSoftApStateMachineCallback, never()).onInfoChanged(any());
    }

    /**
     * Verifies that NumClientsChanged event is being passed from SoftApManager to WifiServiceImpl
     */
    @Test
    public void callsWifiServiceCallbackOnSoftApConnectedClientsChanged() throws Exception {
        final List<WifiClient> testClients = new ArrayList();
        enterSoftApActiveMode();
        mSoftApManagerCallback.onConnectedClientsChanged(testClients);
        mLooper.dispatchAll();

        verify(mSoftApStateMachineCallback).onConnectedClientsChanged(testClients);
    }

    /**
     * Verifies that SoftApInfoChanged event is being passed from SoftApManager to WifiServiceImpl
     */
    @Test
    public void callsWifiServiceCallbackOnSoftApInfoChanged() throws Exception {
        enterSoftApActiveMode();
        mSoftApManagerCallback.onInfoChanged(mTestSoftApInfo);
        mLooper.dispatchAll();

        verify(mSoftApStateMachineCallback).onInfoChanged(mTestSoftApInfo);
    }

    /**
     * Test that we remain in the active state when we get a state change update that scan mode is
     * active.
     */
    @Test
    public void testScanOnlyModeStaysActiveOnEnabledUpdate() throws Exception {
        enterScanOnlyModeActiveState();
        // now inject success through the Listener
        mClientListener.onStarted();
        mLooper.dispatchAll();
        assertInEnabledState();
        verify(mClientModeManager, never()).stop();
    }

    /**
     * Test that a config passed in to the call to enterSoftApMode is used to create the new
     * SoftApManager.
     */
    @Test
    public void testConfigIsPassedToWifiInjector() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setSsid("ThisIsAConfig");
        SoftApModeConfiguration softApConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(), mSoftApCapability);
        enterSoftApActiveMode(softApConfig);
    }

    /**
     * Test that when enterSoftAPMode is called with a null config, we pass a null config to
     * WifiInjector.makeSoftApManager.
     *
     * Passing a null config to SoftApManager indicates that the default config should be used.
     */
    @Test
    public void testNullConfigIsPassedToWifiInjector() throws Exception {
        enterSoftApActiveMode();
    }

    /**
     * Test that two calls to switch to SoftAPMode in succession ends up with the correct config.
     *
     * Expectation: we should end up in SoftAPMode state configured with the second config.
     */
    @Test
    public void testStartSoftApModeTwiceWithTwoConfigs() throws Exception {
        when(mWifiInjector.getWifiApConfigStore()).thenReturn(mWifiApConfigStore);
        Builder configBuilder1 = new SoftApConfiguration.Builder();
        configBuilder1.setSsid("ThisIsAConfig");
        SoftApModeConfiguration softApConfig1 = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder1.build(),
                mSoftApCapability);
        Builder configBuilder2 = new SoftApConfiguration.Builder();
        configBuilder2.setSsid("ThisIsASecondConfig");
        SoftApModeConfiguration softApConfig2 = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder2.build(),
                mSoftApCapability);

        doAnswer(new Answer<SoftApManager>() {
            public SoftApManager answer(InvocationOnMock invocation) {
                Object[] args = invocation.getArguments();
                mSoftApListener = (ActiveModeManager.Listener) args[0];
                return mSoftApManager;
            }
        }).when(mWifiInjector).makeSoftApManager(any(ActiveModeManager.Listener.class),
                any(WifiManager.SoftApCallback.class), eq(softApConfig1));
        // make a second softap manager
        SoftApManager softapManager = mock(SoftApManager.class);
        GeneralUtil.Mutable<ActiveModeManager.Listener> softApListener =
                new GeneralUtil.Mutable<>();
        doAnswer(new Answer<SoftApManager>() {
            public SoftApManager answer(InvocationOnMock invocation) {
                Object[] args = invocation.getArguments();
                softApListener.value = (ActiveModeManager.Listener) args[0];
                return softapManager;
            }
        }).when(mWifiInjector).makeSoftApManager(any(ActiveModeManager.Listener.class),
                any(WifiManager.SoftApCallback.class), eq(softApConfig2));

        mActiveModeWarden.startSoftAp(softApConfig1);
        mLooper.dispatchAll();
        mSoftApListener.onStarted();
        mActiveModeWarden.startSoftAp(softApConfig2);
        mLooper.dispatchAll();
        softApListener.value.onStarted();

        verify(mSoftApManager).start();
        verify(softapManager).start();
        verify(mBatteryStats).reportWifiOn();
    }

    /**
     * Test that we safely disable wifi if it is already disabled.
     */
    @Test
    public void disableWifiWhenAlreadyOff() throws Exception {
        enterStaDisabledMode(false);
        verifyZeroInteractions(mWifiNative);
    }

    /**
     * Trigger recovery and a bug report if we see a native failure
     * while the device is not shutting down
     */
    @Test
    public void handleWifiNativeFailureDeviceNotShuttingDown() throws Exception {
        mWifiNativeStatusListener.onStatusChanged(false);
        mLooper.dispatchAll();
        verify(mWifiDiagnostics).captureBugReportData(
                WifiDiagnostics.REPORT_REASON_WIFINATIVE_FAILURE);
        verify(mSelfRecovery).trigger(eq(SelfRecovery.REASON_WIFINATIVE_FAILURE));
    }

    /**
     * Verify the device shutting down doesn't trigger recovery or bug report.
     */
    @Test
    public void handleWifiNativeFailureDeviceShuttingDown() throws Exception {
        mActiveModeWarden.notifyShuttingDown();
        mWifiNativeStatusListener.onStatusChanged(false);
        mLooper.dispatchAll();
        verify(mWifiDiagnostics, never()).captureBugReportData(
                WifiDiagnostics.REPORT_REASON_WIFINATIVE_FAILURE);
        verify(mSelfRecovery, never()).trigger(eq(SelfRecovery.REASON_WIFINATIVE_FAILURE));
    }

    /**
     * Verify an onStatusChanged callback with "true" does not trigger recovery.
     */
    @Test
    public void handleWifiNativeStatusReady() throws Exception {
        mWifiNativeStatusListener.onStatusChanged(true);
        mLooper.dispatchAll();
        verify(mWifiDiagnostics, never()).captureBugReportData(
                WifiDiagnostics.REPORT_REASON_WIFINATIVE_FAILURE);
        verify(mSelfRecovery, never()).trigger(eq(SelfRecovery.REASON_WIFINATIVE_FAILURE));
    }

    /**
     * Verify that mode stop is safe even if the underlying Client mode exited already.
     */
    @Test
    public void shutdownWifiDoesNotCrashWhenClientModeExitsOnDestroyed() throws Exception {
        enterClientModeActiveState();

        mClientListener.onStopped();
        mLooper.dispatchAll();

        shutdownWifi();

        assertInDisabledState();
    }

    /**
     * Verify that an interface destruction callback is safe after already having been stopped.
     */
    @Test
    public void onDestroyedCallbackDoesNotCrashWhenClientModeAlreadyStopped() throws Exception {
        enterClientModeActiveState();

        shutdownWifi();

        mClientListener.onStopped();
        mLooper.dispatchAll();

        assertInDisabledState();
    }

    /**
     * Verify that mode stop is safe even if the underlying softap mode exited already.
     */
    @Test
    public void shutdownWifiDoesNotCrashWhenSoftApExitsOnDestroyed() throws Exception {
        enterSoftApActiveMode();

        mSoftApListener.onStopped();
        mLooper.dispatchAll();
        mSoftApManagerCallback.onStateChanged(WifiManager.WIFI_AP_STATE_DISABLED, 0);
        mLooper.dispatchAll();

        shutdownWifi();

        verify(mSoftApStateMachineCallback).onStateChanged(WifiManager.WIFI_AP_STATE_DISABLED, 0);
    }

    /**
     * Verify that an interface destruction callback is safe after already having been stopped.
     */
    @Test
    public void onDestroyedCallbackDoesNotCrashWhenSoftApModeAlreadyStopped() throws Exception {
        enterSoftApActiveMode();

        shutdownWifi();

        mSoftApListener.onStopped();
        mSoftApManagerCallback.onStateChanged(WifiManager.WIFI_AP_STATE_DISABLED, 0);
        mLooper.dispatchAll();

        verify(mSoftApStateMachineCallback).onStateChanged(WifiManager.WIFI_AP_STATE_DISABLED, 0);
    }

    /**
     * Verify that we do not crash when calling dump and wifi is fully disabled.
     */
    @Test
    public void dumpWhenWifiFullyOffDoesNotCrash() throws Exception {
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        PrintWriter writer = new PrintWriter(stream);
        mActiveModeWarden.dump(null, writer, null);
    }

    /**
     * Verify that we trigger dump on active mode managers.
     */
    @Test
    public void dumpCallsActiveModeManagers() throws Exception {
        enterSoftApActiveMode();
        enterClientModeActiveState();

        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        PrintWriter writer = new PrintWriter(stream);
        mActiveModeWarden.dump(null, writer, null);

        verify(mSoftApManager).dump(null, writer, null);
        verify(mClientModeManager).dump(null, writer, null);
    }

    /**
     * Verify that stopping tethering doesn't stop LOHS.
     */
    @Test
    public void testStopTetheringButNotLOHS() throws Exception {
        // prepare WiFi configurations
        when(mWifiInjector.getWifiApConfigStore()).thenReturn(mWifiApConfigStore);
        SoftApModeConfiguration tetherConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mSoftApCapability);
        SoftApConfiguration lohsConfigWC = WifiApConfigStore.generateLocalOnlyHotspotConfig(
                mContext, SoftApConfiguration.BAND_2GHZ, null);
        SoftApModeConfiguration lohsConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_LOCAL_ONLY, lohsConfigWC,
                mSoftApCapability);

        // mock SoftAPManagers
        when(mSoftApManager.getRole()).thenReturn(ROLE_SOFTAP_TETHERED);
        doAnswer(new Answer<SoftApManager>() {
            public SoftApManager answer(InvocationOnMock invocation) {
                Object[] args = invocation.getArguments();
                mSoftApListener = (ActiveModeManager.Listener) args[0];
                return mSoftApManager;
            }
        }).when(mWifiInjector).makeSoftApManager(any(ActiveModeManager.Listener.class),
                any(WifiManager.SoftApCallback.class), eq(tetherConfig));
        // make a second softap manager
        SoftApManager lohsSoftapManager = mock(SoftApManager.class);
        when(lohsSoftapManager.getRole()).thenReturn(ROLE_SOFTAP_LOCAL_ONLY);
        GeneralUtil.Mutable<ActiveModeManager.Listener> lohsSoftApListener =
                new GeneralUtil.Mutable<>();
        doAnswer(new Answer<SoftApManager>() {
            public SoftApManager answer(InvocationOnMock invocation) {
                Object[] args = invocation.getArguments();
                lohsSoftApListener.value = (ActiveModeManager.Listener) args[0];
                return lohsSoftapManager;
            }
        }).when(mWifiInjector).makeSoftApManager(any(ActiveModeManager.Listener.class),
                any(WifiManager.SoftApCallback.class), eq(lohsConfig));

        // enable tethering and LOHS
        mActiveModeWarden.startSoftAp(tetherConfig);
        mLooper.dispatchAll();
        mSoftApListener.onStarted();
        mActiveModeWarden.startSoftAp(lohsConfig);
        mLooper.dispatchAll();
        lohsSoftApListener.value.onStarted();
        verify(mSoftApManager).start();
        verify(lohsSoftapManager).start();
        verify(mBatteryStats).reportWifiOn();

        // disable tethering
        mActiveModeWarden.stopSoftAp(WifiManager.IFACE_IP_MODE_TETHERED);
        mLooper.dispatchAll();
        verify(mSoftApManager).stop();
        verify(lohsSoftapManager, never()).stop();
    }

    /**
     * Verify that toggling wifi from disabled starts client mode.
     */
    @Test
    public void enableWifi() throws Exception {
        assertInDisabledState();

        when(mSettingsStore.isWifiToggleEnabled()).thenReturn(true);
        mActiveModeWarden.wifiToggled();
        mLooper.dispatchAll();

        mClientListener.onStarted();
        mLooper.dispatchAll();

        assertInEnabledState();
    }

    /**
     * Test verifying that we can enter scan mode when the scan mode changes
     */
    @Test
    public void enableScanMode() throws Exception {
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(true);
        mActiveModeWarden.scanAlwaysModeChanged();
        mLooper.dispatchAll();
        verify(mClientModeManager).start();
        verify(mClientModeManager).setRole(ROLE_CLIENT_SCAN_ONLY);
        assertInEnabledState();
        verify(mClientModeManager, never()).stop();
    }

    /**
     * Verify that if scanning is enabled at startup, we enter scan mode
     */
    @Test
    public void testEnterScanModeAtStartWhenSet() throws Exception {
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(true);

        mActiveModeWarden = createActiveModeWarden();
        mActiveModeWarden.start();
        mLooper.dispatchAll();

        assertInEnabledState();
    }

    /**
     * Verify that if Wifi is enabled at startup, we enter client mode
     */
    @Test
    public void testEnterClientModeAtStartWhenSet() throws Exception {
        when(mSettingsStore.isWifiToggleEnabled()).thenReturn(true);

        mActiveModeWarden = createActiveModeWarden();
        mActiveModeWarden.start();
        mLooper.dispatchAll();

        assertInEnabledState();

        verify(mClientModeManager).start();
        verify(mClientModeManager).setRole(ROLE_CLIENT_PRIMARY);
    }

    /**
     * Do not enter scan mode if location mode disabled.
     */
    @Test
    public void testDoesNotEnterScanModeWhenLocationModeDisabled() throws Exception {
        // Start a new WifiController with wifi disabled
        when(mSettingsStore.isWifiToggleEnabled()).thenReturn(false);
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(false);
        when(mWifiPermissionsUtil.isLocationModeEnabled()).thenReturn(false);

        mActiveModeWarden = createActiveModeWarden();
        mActiveModeWarden.start();
        mLooper.dispatchAll();

        assertInDisabledState();

        // toggling scan always available is not sufficient for scan mode
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(true);
        mActiveModeWarden.scanAlwaysModeChanged();
        mLooper.dispatchAll();

        assertInDisabledState();
    }

    /**
     * Only enter scan mode if location mode enabled
     */
    @Test
    public void testEnterScanModeWhenLocationModeEnabled() throws Exception {
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(true);
        when(mWifiPermissionsUtil.isLocationModeEnabled()).thenReturn(false);

        reset(mContext);
        when(mContext.getResources()).thenReturn(mResources);
        mActiveModeWarden = createActiveModeWarden();
        mActiveModeWarden.start();
        mLooper.dispatchAll();

        ArgumentCaptor<BroadcastReceiver> bcastRxCaptor =
                ArgumentCaptor.forClass(BroadcastReceiver.class);
        verify(mContext).registerReceiver(bcastRxCaptor.capture(), any(IntentFilter.class));
        BroadcastReceiver broadcastReceiver = bcastRxCaptor.getValue();

        assertInDisabledState();

        when(mWifiPermissionsUtil.isLocationModeEnabled()).thenReturn(true);
        Intent intent = new Intent(LocationManager.MODE_CHANGED_ACTION);
        broadcastReceiver.onReceive(mContext, intent);
        mLooper.dispatchAll();

        assertInEnabledState();
    }


    /**
     * Disabling location mode when in scan mode will disable wifi
     */
    @Test
    public void testExitScanModeWhenLocationModeDisabled() throws Exception {
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(true);
        when(mSettingsStore.isAirplaneModeOn()).thenReturn(false);
        when(mWifiPermissionsUtil.isLocationModeEnabled()).thenReturn(true);

        reset(mContext);
        when(mContext.getResources()).thenReturn(mResources);
        mActiveModeWarden = createActiveModeWarden();
        mActiveModeWarden.start();
        mLooper.dispatchAll();
        mClientListener.onStarted();
        mLooper.dispatchAll();

        ArgumentCaptor<BroadcastReceiver> bcastRxCaptor =
                ArgumentCaptor.forClass(BroadcastReceiver.class);
        verify(mContext).registerReceiver(bcastRxCaptor.capture(), any(IntentFilter.class));
        BroadcastReceiver broadcastReceiver = bcastRxCaptor.getValue();

        assertInEnabledState();

        when(mWifiPermissionsUtil.isLocationModeEnabled()).thenReturn(false);
        Intent intent = new Intent(LocationManager.MODE_CHANGED_ACTION);
        broadcastReceiver.onReceive(mContext, intent);
        mLooper.dispatchAll();

        mClientListener.onStopped();
        mLooper.dispatchAll();

        assertInDisabledState();
    }

    /**
     * When in Client mode, make sure ECM triggers wifi shutdown.
     */
    @Test
    public void testEcmOnFromClientMode() throws Exception {
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(false);
        enableWifi();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);

        assertWifiShutDown(() -> {
            // test ecm changed
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
        });
    }

    /**
     * ECM disabling messages, when in client mode (not expected) do not trigger state changes.
     */
    @Test
    public void testEcmOffInClientMode() throws Exception {
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(false);
        enableWifi();

        // Test with WifiDisableInECBM turned off
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(false);

        assertEnteredEcmMode(() -> {
            // test ecm changed
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
        });
    }

    /**
     * When ECM activates and we are in client mode, disabling ECM should return us to client mode.
     */
    @Test
    public void testEcmDisabledReturnsToClientMode() throws Exception {
        enableWifi();
        assertInEnabledState();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);

        assertWifiShutDown(() -> {
            // test ecm changed
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
        });

        // test ecm changed
        mActiveModeWarden.emergencyCallbackModeChanged(false);
        mLooper.dispatchAll();

        assertInEnabledState();
    }

    /**
     * When Ecm mode is enabled, we should shut down wifi when we get an emergency mode changed
     * update.
     */
    @Test
    public void testEcmOnFromScanMode() throws Exception {
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(true);
        mActiveModeWarden.scanAlwaysModeChanged();
        mLooper.dispatchAll();

        mClientListener.onStarted();
        mLooper.dispatchAll();

        assertInEnabledState();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);

        assertWifiShutDown(() -> {
            // test ecm changed
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
        });
    }

    /**
     * When Ecm mode is disabled, we should not shut down scan mode if we get an emergency mode
     * changed update, but we should turn off soft AP
     */
    @Test
    public void testEcmOffInScanMode() throws Exception {
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(true);
        mActiveModeWarden.scanAlwaysModeChanged();
        mLooper.dispatchAll();

        assertInEnabledState();

        // Test with WifiDisableInECBM turned off:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(false);

        assertEnteredEcmMode(() -> {
            // test ecm changed
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
        });
    }

    /**
     * When ECM is disabled, we should return to scan mode
     */
    @Test
    public void testEcmDisabledReturnsToScanMode() throws Exception {
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(true);
        mActiveModeWarden.scanAlwaysModeChanged();
        mLooper.dispatchAll();

        assertInEnabledState();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);

        assertWifiShutDown(() -> {
            // test ecm changed
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
        });

        // test ecm changed
        mActiveModeWarden.emergencyCallbackModeChanged(false);
        mLooper.dispatchAll();

        assertInEnabledState();
    }

    /**
     * When Ecm mode is enabled, we should shut down wifi when we get an emergency mode changed
     * update.
     */
    @Test
    public void testEcmOnFromSoftApMode() throws Exception {
        enterSoftApActiveMode();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);

        assertEnteredEcmMode(() -> {
            // test ecm changed
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
        });
    }

    /**
     * When Ecm mode is disabled, we should shut down softap mode if we get an emergency mode
     * changed update
     */
    @Test
    public void testEcmOffInSoftApMode() throws Exception {
        enterSoftApActiveMode();

        // Test with WifiDisableInECBM turned off:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(false);

        // test ecm changed
        mActiveModeWarden.emergencyCallbackModeChanged(true);
        mLooper.dispatchAll();

        verify(mSoftApManager).stop();
    }

    /**
     * When ECM is activated and we were in softap mode, we should just return to wifi off when ECM
     * ends
     */
    @Test
    public void testEcmDisabledRemainsDisabledWhenSoftApHadBeenOn() throws Exception {
        assertInDisabledState();

        enterSoftApActiveMode();

        // verify Soft AP Manager started
        verify(mSoftApManager).start();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);

        assertEnteredEcmMode(() -> {
            // test ecm changed
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
            mSoftApListener.onStopped();
            mLooper.dispatchAll();
        });

        // test ecm changed
        mActiveModeWarden.emergencyCallbackModeChanged(false);
        mLooper.dispatchAll();

        assertInDisabledState();

        // verify no additional calls to enable softap
        verify(mSoftApManager).start();
    }

    /**
     * Wifi should remain off when already disabled and we enter ECM.
     */
    @Test
    public void testEcmOnFromDisabledMode() throws Exception {
        assertInDisabledState();
        verify(mSoftApManager, never()).start();
        verify(mClientModeManager, never()).start();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);

        assertEnteredEcmMode(() -> {
            // test ecm changed
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
        });
    }


    /**
     * Updates about call state change also trigger entry of ECM mode.
     */
    @Test
    public void testEnterEcmOnEmergencyCallStateChange() throws Exception {
        assertInDisabledState();

        enableWifi();
        assertInEnabledState();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);

        assertEnteredEcmMode(() -> {
            // test call state changed
            mActiveModeWarden.emergencyCallStateChanged(true);
            mLooper.dispatchAll();
            mClientListener.onStopped();
            mLooper.dispatchAll();
        });

        mActiveModeWarden.emergencyCallStateChanged(false);
        mLooper.dispatchAll();

        assertInEnabledState();
    }

    /**
     * Verify when both ECM and call state changes arrive, we enter ECM mode
     */
    @Test
    public void testEnterEcmWithBothSignals() throws Exception {
        assertInDisabledState();

        enableWifi();
        assertInEnabledState();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);

        assertWifiShutDown(() -> {
            mActiveModeWarden.emergencyCallStateChanged(true);
            mLooper.dispatchAll();
            mClientListener.onStopped();
            mLooper.dispatchAll();
        });

        assertWifiShutDown(() -> {
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
        }, 0); // does not cause another shutdown

        // client mode only started once so far
        verify(mClientModeManager).start();

        mActiveModeWarden.emergencyCallStateChanged(false);
        mLooper.dispatchAll();

        // stay in ecm, do not send an additional client mode trigger
        assertInEmergencyMode();
        // assert that the underlying state is in disabled state
        assertInDisabledState();

        // client mode still only started once
        verify(mClientModeManager).start();

        mActiveModeWarden.emergencyCallbackModeChanged(false);
        mLooper.dispatchAll();

        // now we can re-enable wifi
        verify(mClientModeManager, times(2)).start();
        assertInEnabledState();
    }

    /**
     * Verify when both ECM and call state changes arrive but out of order, we enter ECM mode
     */
    @Test
    public void testEnterEcmWithBothSignalsOutOfOrder() throws Exception {
        assertInDisabledState();

        enableWifi();

        assertInEnabledState();
        verify(mClientModeManager).start();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);

        assertEnteredEcmMode(() -> {
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
            mClientListener.onStopped();
            mLooper.dispatchAll();
        });
        assertInDisabledState();

        assertEnteredEcmMode(() -> {
            mActiveModeWarden.emergencyCallStateChanged(true);
            mLooper.dispatchAll();
        }, 0); // does not enter ECM state again

        mActiveModeWarden.emergencyCallStateChanged(false);
        mLooper.dispatchAll();

        // stay in ecm, do not send an additional client mode trigger
        assertInEmergencyMode();
        // assert that the underlying state is in disabled state
        assertInDisabledState();

        // client mode still only started once
        verify(mClientModeManager).start();

        mActiveModeWarden.emergencyCallbackModeChanged(false);
        mLooper.dispatchAll();

        // now we can re-enable wifi
        verify(mClientModeManager, times(2)).start();
        assertInEnabledState();
    }

    /**
     * Verify when both ECM and call state changes arrive but completely out of order,
     * we still enter and properly exit ECM mode
     */
    @Test
    public void testEnterEcmWithBothSignalsOppositeOrder() throws Exception {
        assertInDisabledState();

        enableWifi();

        assertInEnabledState();
        verify(mClientModeManager).start();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);

        assertEnteredEcmMode(() -> {
            mActiveModeWarden.emergencyCallStateChanged(true);
            mLooper.dispatchAll();
            mClientListener.onStopped();
            mLooper.dispatchAll();
        });
        assertInDisabledState();

        assertEnteredEcmMode(() -> {
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
        }, 0); // still only 1 shutdown

        mActiveModeWarden.emergencyCallbackModeChanged(false);
        mLooper.dispatchAll();

        // stay in ecm, do not send an additional client mode trigger
        assertInEmergencyMode();
        // assert that the underlying state is in disabled state
        assertInDisabledState();

        // client mode still only started once
        verify(mClientModeManager).start();

        mActiveModeWarden.emergencyCallStateChanged(false);
        mLooper.dispatchAll();

        // now we can re-enable wifi
        verify(mClientModeManager, times(2)).start();
        assertInEnabledState();
    }

    /**
     * When ECM is active, we might get addition signals of ECM mode, drop those additional signals,
     * we must exit when one of each signal is received.
     *
     * In any case, duplicate signals indicate a bug from Telephony. Each signal should be turned
     * off before it is turned on again.
     */
    @Test
    public void testProperExitFromEcmModeWithMultipleMessages() throws Exception {
        assertInDisabledState();

        enableWifi();

        verify(mClientModeManager).start();
        assertInEnabledState();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);

        assertEnteredEcmMode(() -> {
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mActiveModeWarden.emergencyCallStateChanged(true);
            mActiveModeWarden.emergencyCallStateChanged(true);
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
            mClientListener.onStopped();
            mLooper.dispatchAll();
        });
        assertInDisabledState();

        assertEnteredEcmMode(() -> {
            mActiveModeWarden.emergencyCallbackModeChanged(false);
            mLooper.dispatchAll();
            mActiveModeWarden.emergencyCallbackModeChanged(false);
            mLooper.dispatchAll();
            mActiveModeWarden.emergencyCallbackModeChanged(false);
            mLooper.dispatchAll();
            mActiveModeWarden.emergencyCallbackModeChanged(false);
            mLooper.dispatchAll();
        }, 0);

        // didn't enter client mode again
        verify(mClientModeManager).start();
        assertInDisabledState();

        // now we will exit ECM
        mActiveModeWarden.emergencyCallStateChanged(false);
        mLooper.dispatchAll();

        // now we can re-enable wifi
        verify(mClientModeManager, times(2)).start();
        assertInEnabledState();
    }

    /**
     * Toggling wifi when in ECM does not exit ecm mode and enable wifi
     */
    @Test
    public void testWifiDoesNotToggleOnWhenInEcm() throws Exception {
        assertInDisabledState();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);
        // test ecm changed
        assertEnteredEcmMode(() -> {
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
        });

        // now toggle wifi and verify we do not start wifi
        when(mSettingsStore.isWifiToggleEnabled()).thenReturn(true);
        mActiveModeWarden.wifiToggled();
        mLooper.dispatchAll();

        verify(mClientModeManager, never()).start();
        assertInDisabledState();
    }

    @Test
    public void testAirplaneModeDoesNotToggleOnWhenInEcm() throws Exception {
        // TODO(b/139829963): investigate the expected behavior is when toggling airplane mode in
        //  ECM
    }

    /**
     * Toggling scan mode when in ECM does not exit ecm mode and enable scan mode
     */
    @Test
    public void testScanModeDoesNotToggleOnWhenInEcm() throws Exception {
        assertInDisabledState();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);
        assertEnteredEcmMode(() -> {
            // test ecm changed
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
        });

        // now enable scanning and verify we do not start wifi
        when(mSettingsStore.isWifiToggleEnabled()).thenReturn(true);
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(true);
        mActiveModeWarden.scanAlwaysModeChanged();
        mLooper.dispatchAll();

        verify(mClientModeManager, never()).start();
        assertInDisabledState();
    }


    /**
     * Toggling softap mode when in ECM does not exit ecm mode and enable softap
     */
    @Test
    public void testSoftApModeDoesNotToggleOnWhenInEcm() throws Exception {
        assertInDisabledState();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);
        assertEnteredEcmMode(() -> {
            // test ecm changed
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
        });

        mActiveModeWarden.startSoftAp(
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mSoftApCapability));
        mLooper.dispatchAll();

        verify(mSoftApManager, never()).start();
        assertInDisabledState();
    }

    /**
     * Toggling off softap mode when in ECM does not induce a mode change
     */
    @Test
    public void testSoftApStoppedDoesNotSwitchModesWhenInEcm() throws Exception {
        assertInDisabledState();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);
        assertEnteredEcmMode(() -> {
            // test ecm changed
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
        });

        mActiveModeWarden.stopSoftAp(WifiManager.IFACE_IP_MODE_UNSPECIFIED);
        mLooper.dispatchAll();

        assertInDisabledState();
        verifyNoMoreInteractions(mSoftApManager, mClientModeManager);
    }

    /**
     * Toggling softap mode when in airplane mode needs to enable softap
     */
    @Test
    public void testSoftApModeToggleWhenInAirplaneMode() throws Exception {
        // Test with airplane mode turned on:
        when(mSettingsStore.isAirplaneModeOn()).thenReturn(true);

        // Turn on SoftAp.
        mActiveModeWarden.startSoftAp(
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mSoftApCapability));
        mLooper.dispatchAll();
        verify(mSoftApManager).start();

        // Turn off SoftAp.
        mActiveModeWarden.stopSoftAp(WifiManager.IFACE_IP_MODE_UNSPECIFIED);
        mLooper.dispatchAll();

        verify(mSoftApManager).stop();
    }

    /**
     * Toggling off scan mode when in ECM does not induce a mode change
     */
    @Test
    public void testScanModeStoppedSwitchModeToDisabledStateWhenInEcm() throws Exception {
        enterScanOnlyModeActiveState();
        assertInEnabledState();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);
        assertEnteredEcmMode(() -> {
            // test ecm changed
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
            mClientListener.onStopped();
            mLooper.dispatchAll();
        });

        // Spurious onStopped
        mClientListener.onStopped();
        mLooper.dispatchAll();

        assertInDisabledState();
    }

    /**
     * Toggling off client mode when in ECM does not induce a mode change
     */
    @Test
    public void testClientModeStoppedSwitchModeToDisabledStateWhenInEcm() throws Exception {
        enterClientModeActiveState();
        assertInEnabledState();

        // Test with WifiDisableInECBM turned on:
        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);
        assertEnteredEcmMode(() -> {
            // test ecm changed
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
            mClientListener.onStopped();
            mLooper.dispatchAll();
        });

        // Spurious onStopped
        mClientListener.onStopped();
        mLooper.dispatchAll();

        assertInDisabledState();
    }

    /**
     * When AP mode is enabled and wifi was previously in AP mode, we should return to
     * EnabledState after the AP is disabled.
     * Enter EnabledState, activate AP mode, disable AP mode.
     * <p>
     * Expected: AP should successfully start and exit, then return to EnabledState.
     */
    @Test
    public void testReturnToEnabledStateAfterAPModeShutdown() throws Exception {
        enableWifi();
        assertInEnabledState();
        verify(mClientModeManager).start();

        mActiveModeWarden.startSoftAp(
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mSoftApCapability));
        // add an "unexpected" sta mode stop to simulate a single interface device
        mClientListener.onStopped();
        mLooper.dispatchAll();

        // Now stop the AP
        mSoftApListener.onStopped();
        mLooper.dispatchAll();

        // We should re-enable client mode
        verify(mClientModeManager, times(2)).start();
        assertInEnabledState();
    }

    /**
     * When in STA mode and SoftAP is enabled and the device supports STA+AP (i.e. the STA wasn't
     * shut down when the AP started), both modes will be running concurrently.
     *
     * Then when the AP is disabled, we should remain in STA mode.
     *
     * Enter EnabledState, activate AP mode, toggle WiFi off.
     * <p>
     * Expected: AP should successfully start and exit, then return to EnabledState.
     */
    @Test
    public void testReturnToEnabledStateAfterWifiEnabledShutdown() throws Exception {
        enableWifi();
        assertInEnabledState();
        verify(mClientModeManager).start();

        mActiveModeWarden.startSoftAp(
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mSoftApCapability));
        mLooper.dispatchAll();

        when(mSettingsStore.isWifiToggleEnabled()).thenReturn(true);
        mActiveModeWarden.wifiToggled();
        mSoftApListener.onStopped();
        mLooper.dispatchAll();

        // wasn't called again
        verify(mClientModeManager).start();
        assertInEnabledState();
    }

    @Test
    public void testRestartWifiStackInEnabledStateTriggersBugReport() throws Exception {
        enableWifi();
        mActiveModeWarden.recoveryRestartWifi(SelfRecovery.REASON_WIFINATIVE_FAILURE);
        mLooper.dispatchAll();
        verify(mClientModeImpl).takeBugReport(anyString(), anyString());
    }

    @Test
    public void testRestartWifiWatchdogDoesNotTriggerBugReport() throws Exception {
        enableWifi();
        mActiveModeWarden.recoveryRestartWifi(SelfRecovery.REASON_LAST_RESORT_WATCHDOG);
        mLooper.dispatchAll();
        verify(mClientModeImpl, never()).takeBugReport(anyString(), anyString());
    }

    /**
     * When in sta mode, CMD_RECOVERY_DISABLE_WIFI messages should trigger wifi to disable.
     */
    @Test
    public void testRecoveryDisabledTurnsWifiOff() throws Exception {
        enableWifi();
        assertInEnabledState();
        mActiveModeWarden.recoveryDisableWifi();
        mLooper.dispatchAll();
        verify(mClientModeManager).stop();
        mClientListener.onStopped();
        mLooper.dispatchAll();
        assertInDisabledState();
    }

    /**
     * When wifi is disabled, CMD_RECOVERY_DISABLE_WIFI should not trigger a state change.
     */
    @Test
    public void testRecoveryDisabledWhenWifiAlreadyOff() throws Exception {
        assertInDisabledState();
        assertWifiShutDown(() -> {
            mActiveModeWarden.recoveryDisableWifi();
            mLooper.dispatchAll();
        });
    }

    /**
     * The command to trigger a WiFi reset should not trigger any action by WifiController if we
     * are not in STA mode.
     * WiFi is not in connect mode, so any calls to reset the wifi stack due to connection failures
     * should be ignored.
     * Create and start WifiController in DisabledState, send command to restart WiFi
     * <p>
     * Expected: WiFiController should not call ActiveModeWarden.disableWifi()
     */
    @Test
    public void testRestartWifiStackInDisabledState() throws Exception {
        assertInDisabledState();

        mActiveModeWarden.recoveryRestartWifi(SelfRecovery.REASON_WIFINATIVE_FAILURE);
        mLooper.dispatchAll();

        assertInDisabledState();
        verifyNoMoreInteractions(mClientModeManager, mSoftApManager);
    }

    /**
     * The command to trigger a WiFi reset should trigger a wifi reset in ClientModeImpl through
     * the ActiveModeWarden.shutdownWifi() call when in STA mode.
     * When WiFi is in scan mode, calls to reset the wifi stack due to native failure
     * should trigger a supplicant stop, and subsequently, a driver reload.
     * Create and start WifiController in EnabledState, send command to restart WiFi
     * <p>
     * Expected: WiFiController should call ActiveModeWarden.shutdownWifi() and
     * ActiveModeWarden should enter SCAN_ONLY mode and the wifi driver should be started.
     */
    @Test
    public void testRestartWifiStackInDisabledStateWithScanState() throws Exception {
        assertInDisabledState();

        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(true);
        mActiveModeWarden.scanAlwaysModeChanged();
        mLooper.dispatchAll();

        assertInEnabledState();
        verify(mClientModeManager).start();
        verify(mClientModeManager).setRole(ROLE_CLIENT_SCAN_ONLY);

        mActiveModeWarden.recoveryRestartWifi(SelfRecovery.REASON_WIFINATIVE_FAILURE);
        mLooper.dispatchAll();

        verify(mClientModeManager).stop();
        mClientListener.onStopped();
        mLooper.dispatchAll();
        assertInDisabledState();

        mLooper.moveTimeForward(TEST_WIFI_RECOVERY_DELAY_MS);
        mLooper.dispatchAll();

        verify(mClientModeManager, times(2)).start();
        verify(mClientModeManager, times(2)).setRole(ROLE_CLIENT_SCAN_ONLY);
        assertInEnabledState();
    }

    /**
     * The command to trigger a WiFi reset should trigger a wifi reset in ClientModeImpl through
     * the ActiveModeWarden.shutdownWifi() call when in STA mode.
     * WiFi is in connect mode, calls to reset the wifi stack due to connection failures
     * should trigger a supplicant stop, and subsequently, a driver reload.
     * Create and start WifiController in EnabledState, send command to restart WiFi
     * <p>
     * Expected: WiFiController should call ActiveModeWarden.shutdownWifi() and
     * ActiveModeWarden should enter CONNECT_MODE and the wifi driver should be started.
     */
    @Test
    public void testRestartWifiStackInEnabledState() throws Exception {
        enableWifi();
        assertInEnabledState();
        verify(mClientModeManager).start();

        assertWifiShutDown(() -> {
            mActiveModeWarden.recoveryRestartWifi(SelfRecovery.REASON_WIFINATIVE_FAILURE);
            mLooper.dispatchAll();
            // Complete the stop
            mClientListener.onStopped();
            mLooper.dispatchAll();
        });

        // still only started once
        verify(mClientModeManager).start();

        mLooper.moveTimeForward(TEST_WIFI_RECOVERY_DELAY_MS);
        mLooper.dispatchAll();

        // started again
        verify(mClientModeManager, times(2)).start();
        assertInEnabledState();
    }

    /**
     * The command to trigger a WiFi reset should not trigger a reset when in ECM mode.
     * Enable wifi and enter ECM state, send command to restart wifi.
     * <p>
     * Expected: The command to trigger a wifi reset should be ignored and we should remain in ECM
     * mode.
     */
    @Test
    public void testRestartWifiStackDoesNotExitECMMode() throws Exception {
        enableWifi();
        assertInEnabledState();
        verify(mClientModeManager).start();
        verify(mClientModeManager).setRole(ROLE_CLIENT_PRIMARY);

        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);
        assertEnteredEcmMode(() -> {
            mActiveModeWarden.emergencyCallStateChanged(true);
            mLooper.dispatchAll();
            mClientListener.onStopped();
            mLooper.dispatchAll();
        });
        assertInEmergencyMode();
        assertInDisabledState();
        verify(mClientModeManager).stop();
        verify(mClientModeManager, atLeastOnce()).getRole();

        mActiveModeWarden.recoveryRestartWifi(SelfRecovery.REASON_LAST_RESORT_WATCHDOG);
        mLooper.dispatchAll();

        verify(mClientModeManager).start(); // wasn't called again
        assertInEmergencyMode();
        assertInDisabledState();
        verifyNoMoreInteractions(mClientModeManager, mSoftApManager);
    }

    /**
     * The command to trigger a WiFi reset should trigger a reset when in AP mode.
     * Enter AP mode, send command to restart wifi.
     * <p>
     * Expected: The command to trigger a wifi reset should trigger wifi shutdown.
     */
    @Test
    public void testRestartWifiStackFullyStopsWifi() throws Exception {
        mActiveModeWarden.startSoftAp(new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_LOCAL_ONLY, null, mSoftApCapability));
        mLooper.dispatchAll();
        verify(mSoftApManager).start();
        verify(mSoftApManager).setRole(ROLE_SOFTAP_LOCAL_ONLY);

        assertWifiShutDown(() -> {
            mActiveModeWarden.recoveryRestartWifi(SelfRecovery.REASON_STA_IFACE_DOWN);
            mLooper.dispatchAll();
        });
    }

    /**
     * Tests that when Wifi is already disabled and another Wifi toggle command arrives,
     * don't enter scan mode if {@link WifiSettingsStore#isScanAlwaysAvailable()} is false.
     * Note: {@link WifiSettingsStore#isScanAlwaysAvailable()} returns false if either the wifi
     * scanning is disabled and airplane mode is on.
     */
    @Test
    public void staDisabled_toggleWifiOff_scanNotAvailable_dontGoToScanMode() {
        assertInDisabledState();

        when(mSettingsStore.isWifiToggleEnabled()).thenReturn(false);
        when(mWifiPermissionsUtil.isLocationModeEnabled()).thenReturn(true);
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(false);
        when(mSettingsStore.isAirplaneModeOn()).thenReturn(true);

        mActiveModeWarden.wifiToggled();
        mLooper.dispatchAll();

        assertInDisabledState();
        verify(mClientModeManager, never()).start();
    }

    /**
     * Tests that when Wifi is already disabled and another Wifi toggle command arrives,
     * enter scan mode if {@link WifiSettingsStore#isScanAlwaysAvailable()} is true.
     * Note: {@link WifiSettingsStore#isScanAlwaysAvailable()} returns true if both the wifi
     * scanning is enabled and airplane mode is off.
     */
    @Test
    public void staDisabled_toggleWifiOff_scanAvailable_goToScanMode() {
        assertInDisabledState();

        when(mSettingsStore.isWifiToggleEnabled()).thenReturn(false);
        when(mWifiPermissionsUtil.isLocationModeEnabled()).thenReturn(true);
        when(mSettingsStore.isScanAlwaysAvailable()).thenReturn(true);
        when(mSettingsStore.isAirplaneModeOn()).thenReturn(false);

        mActiveModeWarden.wifiToggled();
        mLooper.dispatchAll();

        assertInEnabledState();
        verify(mClientModeManager).start();
        verify(mClientModeManager).setRole(ROLE_CLIENT_SCAN_ONLY);
    }

    /**
     * Tests that if the carrier config to disable Wifi is enabled during ECM, Wifi is shut down
     * when entering ECM and turned back on when exiting ECM.
     */
    @Test
    public void ecmDisablesWifi_exitEcm_restartWifi() throws Exception {
        enterClientModeActiveState();

        verify(mClientModeManager).start();

        when(mFacade.getConfigWiFiDisableInECBM(mContext)).thenReturn(true);
        assertEnteredEcmMode(() -> {
            mActiveModeWarden.emergencyCallbackModeChanged(true);
            mLooper.dispatchAll();
        });
        assertInEnabledState();
        verify(mClientModeManager).stop();

        mActiveModeWarden.emergencyCallbackModeChanged(false);
        mLooper.dispatchAll();

        assertThat(mActiveModeWarden.isInEmergencyMode()).isFalse();
        // client mode restarted
        verify(mClientModeManager, times(2)).start();
        assertInEnabledState();
    }

    @Test
    public void testUpdateCapabilityInSoftApActiveMode() throws Exception {
        SoftApCapability testCapability = new SoftApCapability(0);
        enterSoftApActiveMode();
        mActiveModeWarden.updateSoftApCapability(testCapability);
        mLooper.dispatchAll();
        verify(mSoftApManager).updateCapability(testCapability);
    }

    @Test
    public void testUpdateConfigInSoftApActiveMode() throws Exception {
        SoftApConfiguration testConfig = new SoftApConfiguration.Builder()
                .setSsid("Test123").build();
        enterSoftApActiveMode();
        mActiveModeWarden.updateSoftApConfiguration(testConfig);
        mLooper.dispatchAll();
        verify(mSoftApManager).updateConfiguration(testConfig);
    }

    @Test
    public void testUpdateCapabilityInNonSoftApActiveMode() throws Exception {
        SoftApCapability testCapability = new SoftApCapability(0);
        enterClientModeActiveState();
        mActiveModeWarden.updateSoftApCapability(testCapability);
        mLooper.dispatchAll();
        verify(mSoftApManager, never()).updateCapability(any());
    }

    @Test
    public void testUpdateConfigInNonSoftApActiveMode() throws Exception {
        SoftApConfiguration testConfig = new SoftApConfiguration.Builder()
                .setSsid("Test123").build();
        enterClientModeActiveState();
        mActiveModeWarden.updateSoftApConfiguration(testConfig);
        mLooper.dispatchAll();
        verify(mSoftApManager, never()).updateConfiguration(any());
    }

    @Test
    public void interfaceAvailabilityListener() throws Exception {
        assertFalse(mActiveModeWarden.canRequestMoreClientModeManagers());
        assertFalse(mActiveModeWarden.canRequestMoreSoftApManagers());

        assertNotNull(mClientIfaceAvailableListener.getValue());
        assertNotNull(mSoftApIfaceAvailableListener.getValue());

        mClientIfaceAvailableListener.getValue().onAvailabilityChanged(true);
        mLooper.dispatchAll();
        assertTrue(mActiveModeWarden.canRequestMoreClientModeManagers());

        mSoftApIfaceAvailableListener.getValue().onAvailabilityChanged(true);
        mLooper.dispatchAll();
        assertTrue(mActiveModeWarden.canRequestMoreSoftApManagers());

        mClientIfaceAvailableListener.getValue().onAvailabilityChanged(false);
        mLooper.dispatchAll();
        assertFalse(mActiveModeWarden.canRequestMoreClientModeManagers());

        mSoftApIfaceAvailableListener.getValue().onAvailabilityChanged(false);
        mLooper.dispatchAll();
        assertFalse(mActiveModeWarden.canRequestMoreSoftApManagers());
    }

    @Test
    public void isStaApConcurrencySupported() throws Exception {
        when(mWifiNative.isStaApConcurrencySupported()).thenReturn(false);
        assertFalse(mActiveModeWarden.isStaApConcurrencySupported());

        when(mWifiNative.isStaApConcurrencySupported()).thenReturn(true);
        assertTrue(mActiveModeWarden.isStaApConcurrencySupported());
    }

    @Test
    public void airplaneModeToggleOnDisablesWifi() throws Exception {
        enterClientModeActiveState();
        assertInEnabledState();

        assertWifiShutDown(() -> {
            when(mSettingsStore.isAirplaneModeOn()).thenReturn(true);
            mActiveModeWarden.airplaneModeToggled();
            mLooper.dispatchAll();
        });

        mClientListener.onStopped();
        mLooper.dispatchAll();
        assertInDisabledState();
    }

    @Test
    public void airplaneModeToggleOnDisablesSoftAp() throws Exception {
        enterSoftApActiveMode();
        assertInEnabledState();

        assertWifiShutDown(() -> {
            when(mSettingsStore.isAirplaneModeOn()).thenReturn(true);
            mActiveModeWarden.airplaneModeToggled();
            mLooper.dispatchAll();
        });

        mSoftApListener.onStopped();
        mLooper.dispatchAll();
        assertInDisabledState();
    }

    @Test
    public void airplaneModeToggleOffIsDeferredWhileProcessingToggleOnWithOneModeManager()
            throws Exception {
        enterClientModeActiveState();
        assertInEnabledState();

        // APM toggle on
        assertWifiShutDown(() -> {
            when(mSettingsStore.isAirplaneModeOn()).thenReturn(true);
            mActiveModeWarden.airplaneModeToggled();
            mLooper.dispatchAll();
        });


        // APM toggle off before the stop is complete.
        assertInEnabledState();
        when(mClientModeManager.isStopping()).thenReturn(true);
        when(mSettingsStore.isAirplaneModeOn()).thenReturn(false);
        mActiveModeWarden.airplaneModeToggled();
        mLooper.dispatchAll();

        mClientListener.onStopped();
        mLooper.dispatchAll();

        verify(mClientModeManager, times(2)).start();
        verify(mClientModeManager, times(2)).setRole(ROLE_CLIENT_PRIMARY);

        mClientListener.onStarted();
        mLooper.dispatchAll();

        // We should be back to enabled state.
        assertInEnabledState();
    }

    @Test
    public void airplaneModeToggleOffIsDeferredWhileProcessingToggleOnWithOneModeManager2()
            throws Exception {
        enterClientModeActiveState();
        assertInEnabledState();

        // APM toggle on
        assertWifiShutDown(() -> {
            when(mSettingsStore.isAirplaneModeOn()).thenReturn(true);
            mActiveModeWarden.airplaneModeToggled();
            mLooper.dispatchAll();
        });


        // APM toggle off before the stop is complete.
        assertInEnabledState();
        when(mClientModeManager.isStopping()).thenReturn(true);
        when(mSettingsStore.isAirplaneModeOn()).thenReturn(false);
        mActiveModeWarden.airplaneModeToggled();
        // This test is identical to
        // airplaneModeToggleOffIsDeferredWhileProcessingToggleOnWithOneModeManager, except the
        // dispatchAll() here is removed. There could be a race between airplaneModeToggled and
        // mClientListener.onStopped(). See b/160105640#comment5.

        mClientListener.onStopped();
        mLooper.dispatchAll();

        verify(mClientModeManager, times(2)).start();
        verify(mClientModeManager, times(2)).setRole(ROLE_CLIENT_PRIMARY);

        mClientListener.onStarted();
        mLooper.dispatchAll();

        // We should be back to enabled state.
        assertInEnabledState();
    }

    @Test
    public void airplaneModeToggleOffIsDeferredWhileProcessingToggleOnWithTwoModeManager()
            throws Exception {
        enterClientModeActiveState();
        enterSoftApActiveMode();
        assertInEnabledState();

        // APM toggle on
        assertWifiShutDown(() -> {
            when(mSettingsStore.isAirplaneModeOn()).thenReturn(true);
            mActiveModeWarden.airplaneModeToggled();
            mLooper.dispatchAll();
        });


        // APM toggle off before the stop is complete.
        assertInEnabledState();
        when(mClientModeManager.isStopping()).thenReturn(true);
        when(mSoftApManager.isStopping()).thenReturn(true);
        when(mSettingsStore.isAirplaneModeOn()).thenReturn(false);
        mActiveModeWarden.airplaneModeToggled();
        mLooper.dispatchAll();

        // AP stopped, should not process APM toggle.
        mSoftApListener.onStopped();
        mLooper.dispatchAll();
        verify(mClientModeManager, times(1)).start();
        verify(mClientModeManager, times(1)).setRole(ROLE_CLIENT_PRIMARY);

        // STA also stopped, should process APM toggle.
        mClientListener.onStopped();
        mLooper.dispatchAll();
        verify(mClientModeManager, times(2)).start();
        verify(mClientModeManager, times(2)).setRole(ROLE_CLIENT_PRIMARY);

        mClientListener.onStarted();
        mLooper.dispatchAll();

        // We should be back to enabled state.
        assertInEnabledState();
    }
}
