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


import static android.net.wifi.WifiManager.EXTRA_PREVIOUS_WIFI_AP_STATE;
import static android.net.wifi.WifiManager.EXTRA_WIFI_AP_FAILURE_REASON;
import static android.net.wifi.WifiManager.EXTRA_WIFI_AP_INTERFACE_NAME;
import static android.net.wifi.WifiManager.EXTRA_WIFI_AP_MODE;
import static android.net.wifi.WifiManager.EXTRA_WIFI_AP_STATE;
import static android.net.wifi.WifiManager.IFACE_IP_MODE_LOCAL_ONLY;
import static android.net.wifi.WifiManager.SAP_CLIENT_DISCONNECT_REASON_CODE_UNSPECIFIED;
import static android.net.wifi.WifiManager.WIFI_AP_STATE_DISABLED;
import static android.net.wifi.WifiManager.WIFI_AP_STATE_DISABLING;
import static android.net.wifi.WifiManager.WIFI_AP_STATE_ENABLED;
import static android.net.wifi.WifiManager.WIFI_AP_STATE_ENABLING;
import static android.net.wifi.WifiManager.WIFI_AP_STATE_FAILED;

import static com.android.server.wifi.LocalOnlyHotspotRequestInfo.HOTSPOT_NO_ERROR;
import static com.android.server.wifi.util.ApConfigUtil.DEFAULT_AP_CHANNEL;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.NotificationManager;
import android.app.test.TestAlarmManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.MacAddress;
import android.net.wifi.SoftApCapability;
import android.net.wifi.SoftApConfiguration;
import android.net.wifi.SoftApConfiguration.Builder;
import android.net.wifi.SoftApInfo;
import android.net.wifi.WifiClient;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiScanner;
import android.net.wifi.nl80211.NativeWifiClient;
import android.os.UserHandle;
import android.os.test.TestLooper;
import android.provider.Settings;

import androidx.test.filters.SmallTest;

import com.android.internal.util.WakeupMessage;
import com.android.wifi.resources.R;

import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Unit tests for {@link SoftApManager}. */
@SmallTest
public class SoftApManagerTest extends WifiBaseTest {

    private static final String TAG = "SoftApManagerTest";

    private static final String DEFAULT_SSID = "DefaultTestSSID";
    private static final String TEST_SSID = "TestSSID";
    private static final String TEST_PASSWORD = "TestPassword";
    private static final String TEST_COUNTRY_CODE = "TestCountry";
    private static final String TEST_INTERFACE_NAME = "testif0";
    private static final String OTHER_INTERFACE_NAME = "otherif";
    private static final long TEST_DEFAULT_SHUTDOWN_TIMEOUT_MILLS = 600_000;
    private static final MacAddress TEST_MAC_ADDRESS = MacAddress.fromString("22:33:44:55:66:77");
    private static final MacAddress TEST_MAC_ADDRESS_2 = MacAddress.fromString("aa:bb:cc:dd:ee:ff");
    private static final WifiClient TEST_CONNECTED_CLIENT = new WifiClient(TEST_MAC_ADDRESS);
    private static final NativeWifiClient TEST_NATIVE_CLIENT = new NativeWifiClient(
            TEST_MAC_ADDRESS);
    private static final WifiClient TEST_CONNECTED_CLIENT_2 = new WifiClient(TEST_MAC_ADDRESS_2);
    private static final NativeWifiClient TEST_NATIVE_CLIENT_2 = new NativeWifiClient(
            TEST_MAC_ADDRESS_2);
    private static final int TEST_AP_FREQUENCY = 2412;
    private static final int TEST_AP_BANDWIDTH_FROM_IFACE_CALLBACK =
            SoftApInfo.CHANNEL_WIDTH_20MHZ_NOHT;
    private static final int TEST_AP_BANDWIDTH_IN_SOFTAPINFO = SoftApInfo.CHANNEL_WIDTH_20MHZ_NOHT;
    private static final int[] EMPTY_CHANNEL_ARRAY = {};
    private SoftApConfiguration mDefaultApConfig = createDefaultApConfig();

    private TestLooper mLooper;
    private TestAlarmManager mAlarmManager;
    private SoftApInfo mTestSoftApInfo;
    private SoftApCapability mTestSoftApCapability;

    @Mock WifiContext mContext;
    @Mock Resources mResources;
    @Mock WifiNative mWifiNative;
    @Mock WifiManager.SoftApCallback mCallback;
    @Mock ActiveModeManager.Listener mListener;
    @Mock FrameworkFacade mFrameworkFacade;
    @Mock WifiApConfigStore mWifiApConfigStore;
    @Mock WifiMetrics mWifiMetrics;
    @Mock SarManager mSarManager;
    @Mock BaseWifiDiagnostics mWifiDiagnostics;
    @Mock NotificationManager mNotificationManager;
    @Mock SoftApNotifier mFakeSoftApNotifier;

    final ArgumentCaptor<WifiNative.InterfaceCallback> mWifiNativeInterfaceCallbackCaptor =
            ArgumentCaptor.forClass(WifiNative.InterfaceCallback.class);
    final ArgumentCaptor<WifiNative.SoftApListener> mSoftApListenerCaptor =
            ArgumentCaptor.forClass(WifiNative.SoftApListener.class);

    SoftApManager mSoftApManager;

    /** Sets up test. */
    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mLooper = new TestLooper();

        when(mWifiNative.isSetMacAddressSupported(any())).thenReturn(true);
        when(mWifiNative.setMacAddress(any(), any())).thenReturn(true);
        when(mWifiNative.startSoftAp(eq(TEST_INTERFACE_NAME), any(), any())).thenReturn(true);

        when(mFrameworkFacade.getIntegerSetting(
                mContext, Settings.Global.SOFT_AP_TIMEOUT_ENABLED, 1)).thenReturn(1);
        mAlarmManager = new TestAlarmManager();
        when(mContext.getSystemService(Context.ALARM_SERVICE))
                .thenReturn(mAlarmManager.getAlarmManager());
        when(mContext.getResources()).thenReturn(mResources);
        when(mContext.getSystemService(NotificationManager.class))
                .thenReturn(mNotificationManager);
        when(mContext.getWifiOverlayApkPkgName()).thenReturn("test.com.android.wifi.resources");

        when(mResources.getInteger(R.integer.config_wifiFrameworkSoftApShutDownTimeoutMilliseconds))
                .thenReturn((int) TEST_DEFAULT_SHUTDOWN_TIMEOUT_MILLS);
        when(mWifiNative.setCountryCodeHal(
                TEST_INTERFACE_NAME, TEST_COUNTRY_CODE.toUpperCase(Locale.ROOT)))
                .thenReturn(true);
        when(mWifiNative.getFactoryMacAddress(any())).thenReturn(TEST_MAC_ADDRESS);
        when(mWifiApConfigStore.randomizeBssidIfUnset(any(), any())).thenAnswer(
                (invocation) -> invocation.getArgument(1));
        mTestSoftApInfo = new SoftApInfo();
        mTestSoftApInfo.setFrequency(TEST_AP_FREQUENCY);
        mTestSoftApInfo.setBandwidth(TEST_AP_BANDWIDTH_IN_SOFTAPINFO);
        // Default set up all features support.
        long testSoftApFeature = SoftApCapability.SOFTAP_FEATURE_CLIENT_FORCE_DISCONNECT
                | SoftApCapability.SOFTAP_FEATURE_ACS_OFFLOAD
                | SoftApCapability.SOFTAP_FEATURE_WPA3_SAE;
        mTestSoftApCapability = new SoftApCapability(testSoftApFeature);
        mTestSoftApCapability.setMaxSupportedClients(10);
    }

    private SoftApConfiguration createDefaultApConfig() {
        Builder defaultConfigBuilder = new SoftApConfiguration.Builder();
        defaultConfigBuilder.setSsid(DEFAULT_SSID);
        return defaultConfigBuilder.build();
    }

    private SoftApManager createSoftApManager(SoftApModeConfiguration config, String countryCode) {
        if (config.getSoftApConfiguration() == null) {
            when(mWifiApConfigStore.getApConfiguration()).thenReturn(mDefaultApConfig);
        }
        SoftApManager newSoftApManager = new SoftApManager(mContext,
                                                           mLooper.getLooper(),
                                                           mFrameworkFacade,
                                                           mWifiNative,
                                                           countryCode,
                                                           mListener,
                                                           mCallback,
                                                           mWifiApConfigStore,
                                                           config,
                                                           mWifiMetrics,
                                                           mSarManager,
                                                           mWifiDiagnostics);
        mLooper.dispatchAll();

        return newSoftApManager;
    }

    /** Verifies startSoftAp will use default config if AP configuration is not provided. */
    @Test
    public void startSoftApWithoutConfig() throws Exception {
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);
    }

    /** Verifies startSoftAp will use provided config and start AP. */
    @Test
    public void startSoftApWithConfig() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        SoftApModeConfiguration apConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(),
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);
    }

    /**
     * Verifies startSoftAp will start with the hiddenSSID param set when it is set to true in the
     * supplied config.
     */
    @Test
    public void startSoftApWithHiddenSsidTrueInConfig() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setHiddenSsid(true);
        SoftApModeConfiguration apConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(),
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);
    }

    /**
     * Verifies startSoftAp will start with the password param set in the
     * supplied config.
     */
    @Test
    public void startSoftApWithPassphraseInConfig() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setPassphrase(TEST_PASSWORD,
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        SoftApModeConfiguration apConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(),
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);
    }

    /** Tests softap startup if default config fails to load. **/
    @Test
    public void startSoftApDefaultConfigFailedToLoad() throws Exception {
        when(mWifiNative.setupInterfaceForSoftApMode(any())).thenReturn(TEST_INTERFACE_NAME);

        when(mWifiApConfigStore.getApConfiguration()).thenReturn(null);
        SoftApModeConfiguration nullApConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        SoftApManager newSoftApManager = new SoftApManager(mContext,
                                                           mLooper.getLooper(),
                                                           mFrameworkFacade,
                                                           mWifiNative,
                                                           TEST_COUNTRY_CODE,
                                                           mListener,
                                                           mCallback,
                                                           mWifiApConfigStore,
                                                           nullApConfig,
                                                           mWifiMetrics,
                                                           mSarManager,
                                                           mWifiDiagnostics);
        mLooper.dispatchAll();
        newSoftApManager.start();
        mLooper.dispatchAll();
        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_FAILED,
                WifiManager.SAP_START_FAILURE_GENERAL);
        verify(mListener).onStartFailure();
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext, times(2)).sendStickyBroadcastAsUser(intentCaptor.capture(),
                eq(UserHandle.ALL));

        List<Intent> capturedIntents = intentCaptor.getAllValues();
        checkApStateChangedBroadcast(capturedIntents.get(0), WIFI_AP_STATE_ENABLING,
                WIFI_AP_STATE_DISABLED, HOTSPOT_NO_ERROR, TEST_INTERFACE_NAME,
                nullApConfig.getTargetMode());
        checkApStateChangedBroadcast(capturedIntents.get(1), WIFI_AP_STATE_FAILED,
                WIFI_AP_STATE_ENABLING, WifiManager.SAP_START_FAILURE_GENERAL, TEST_INTERFACE_NAME,
                nullApConfig.getTargetMode());
    }

    /**
     * Test that failure to retrieve the SoftApInterface name increments the corresponding metrics
     * and proper state updates are sent out.
     */
    @Test
    public void testSetupForSoftApModeNullApInterfaceNameFailureIncrementsMetrics()
            throws Exception {
        when(mWifiNative.setupInterfaceForSoftApMode(any())).thenReturn(null);

        SoftApModeConfiguration config = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, new SoftApConfiguration.Builder().build(),
                mTestSoftApCapability);

        when(mWifiApConfigStore.getApConfiguration()).thenReturn(null);
        SoftApModeConfiguration nullApConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        SoftApManager newSoftApManager = new SoftApManager(mContext,
                                                           mLooper.getLooper(),
                                                           mFrameworkFacade,
                                                           mWifiNative,
                                                           TEST_COUNTRY_CODE,
                                                           mListener,
                                                           mCallback,
                                                           mWifiApConfigStore,
                                                           nullApConfig,
                                                           mWifiMetrics,
                                                           mSarManager,
                                                           mWifiDiagnostics);
        mLooper.dispatchAll();
        newSoftApManager.start();
        mLooper.dispatchAll();
        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_FAILED,
                WifiManager.SAP_START_FAILURE_GENERAL);
        verify(mListener).onStartFailure();
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext).sendStickyBroadcastAsUser(intentCaptor.capture(),
                eq(UserHandle.ALL));

        checkApStateChangedBroadcast(intentCaptor.getValue(), WIFI_AP_STATE_FAILED,
                WIFI_AP_STATE_DISABLED, WifiManager.SAP_START_FAILURE_GENERAL, null,
                nullApConfig.getTargetMode());

        verify(mWifiMetrics).incrementSoftApStartResult(false,
                WifiManager.SAP_START_FAILURE_GENERAL);
    }

    /**
     * Test that an empty SoftApInterface name is detected as a failure and increments the
     * corresponding metrics and proper state updates are sent out.
     */
    @Test
    public void testSetupForSoftApModeEmptyInterfaceNameFailureIncrementsMetrics()
            throws Exception {
        when(mWifiNative.setupInterfaceForSoftApMode(any())).thenReturn("");

        SoftApModeConfiguration config = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, new SoftApConfiguration.Builder().build(),
                mTestSoftApCapability);

        when(mWifiApConfigStore.getApConfiguration()).thenReturn(null);
        SoftApModeConfiguration nullApConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        SoftApManager newSoftApManager = new SoftApManager(mContext,
                                                           mLooper.getLooper(),
                                                           mFrameworkFacade,
                                                           mWifiNative,
                                                           TEST_COUNTRY_CODE,
                                                           mListener,
                                                           mCallback,
                                                           mWifiApConfigStore,
                                                           nullApConfig,
                                                           mWifiMetrics,
                                                           mSarManager,
                                                           mWifiDiagnostics);
        mLooper.dispatchAll();
        newSoftApManager.start();
        mLooper.dispatchAll();
        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_FAILED,
                WifiManager.SAP_START_FAILURE_GENERAL);
        verify(mListener).onStartFailure();
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext).sendStickyBroadcastAsUser(intentCaptor.capture(),
                eq(UserHandle.ALL));

        checkApStateChangedBroadcast(intentCaptor.getValue(), WIFI_AP_STATE_FAILED,
                WIFI_AP_STATE_DISABLED, WifiManager.SAP_START_FAILURE_GENERAL, "",
                nullApConfig.getTargetMode());

        verify(mWifiMetrics).incrementSoftApStartResult(false,
                WifiManager.SAP_START_FAILURE_GENERAL);
    }

    /**
     * Tests that the generic error is propagated and properly reported when starting softap and no
     * country code is provided.
     */
    @Test
    public void startSoftApOn5GhzFailGeneralErrorForNoCountryCode() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_5GHZ);
        configBuilder.setSsid(TEST_SSID);
        SoftApModeConfiguration softApConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(),
                mTestSoftApCapability);

        when(mWifiNative.setupInterfaceForSoftApMode(any())).thenReturn(TEST_INTERFACE_NAME);

        SoftApManager newSoftApManager = new SoftApManager(mContext,
                mLooper.getLooper(),
                mFrameworkFacade,
                mWifiNative,
                null,
                mListener,
                mCallback,
                mWifiApConfigStore,
                softApConfig,
                mWifiMetrics,
                mSarManager,
                mWifiDiagnostics);
        mLooper.dispatchAll();
        newSoftApManager.start();
        mLooper.dispatchAll();

        verify(mWifiNative, never()).setCountryCodeHal(eq(TEST_INTERFACE_NAME), any());

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext, times(2)).sendStickyBroadcastAsUser(intentCaptor.capture(),
                eq(UserHandle.ALL));

        List<Intent> capturedIntents = intentCaptor.getAllValues();
        checkApStateChangedBroadcast(capturedIntents.get(0), WIFI_AP_STATE_ENABLING,
                WIFI_AP_STATE_DISABLED, HOTSPOT_NO_ERROR, TEST_INTERFACE_NAME,
                softApConfig.getTargetMode());
        checkApStateChangedBroadcast(capturedIntents.get(1), WIFI_AP_STATE_FAILED,
                WIFI_AP_STATE_ENABLING, WifiManager.SAP_START_FAILURE_GENERAL, TEST_INTERFACE_NAME,
                softApConfig.getTargetMode());
    }

    /**
     * Tests that the generic error is propagated and properly reported when starting softap and the
     * country code cannot be set.
     */
    @Test
    public void startSoftApOn5GhzFailGeneralErrorForCountryCodeSetFailure() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_5GHZ);
        configBuilder.setSsid(TEST_SSID);
        SoftApModeConfiguration softApConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(),
                mTestSoftApCapability);

        when(mWifiNative.setupInterfaceForSoftApMode(any())).thenReturn(TEST_INTERFACE_NAME);
        when(mWifiNative.setCountryCodeHal(
                TEST_INTERFACE_NAME, TEST_COUNTRY_CODE.toUpperCase(Locale.ROOT)))
                .thenReturn(false);

        SoftApManager newSoftApManager = new SoftApManager(mContext,
                                                           mLooper.getLooper(),
                                                           mFrameworkFacade,
                                                           mWifiNative,
                                                           TEST_COUNTRY_CODE,
                                                           mListener,
                                                           mCallback,
                                                           mWifiApConfigStore,
                                                           softApConfig,
                                                           mWifiMetrics,
                                                           mSarManager,
                                                           mWifiDiagnostics);
        mLooper.dispatchAll();
        newSoftApManager.start();
        mLooper.dispatchAll();

        verify(mWifiNative).setCountryCodeHal(
                TEST_INTERFACE_NAME, TEST_COUNTRY_CODE.toUpperCase(Locale.ROOT));

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext, times(2)).sendStickyBroadcastAsUser(intentCaptor.capture(),
                eq(UserHandle.ALL));

        List<Intent> capturedIntents = intentCaptor.getAllValues();
        checkApStateChangedBroadcast(capturedIntents.get(0), WIFI_AP_STATE_ENABLING,
                WIFI_AP_STATE_DISABLED, HOTSPOT_NO_ERROR, TEST_INTERFACE_NAME,
                softApConfig.getTargetMode());
        checkApStateChangedBroadcast(capturedIntents.get(1), WIFI_AP_STATE_FAILED,
                WIFI_AP_STATE_ENABLING, WifiManager.SAP_START_FAILURE_GENERAL, TEST_INTERFACE_NAME,
                softApConfig.getTargetMode());
    }

    /**
     * Tests that there is no failure in starting softap in 2Ghz band when no country code is
     * provided.
     */
    @Test
    public void startSoftApOn24GhzNoFailForNoCountryCode() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        SoftApModeConfiguration softApConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(),
                mTestSoftApCapability);

        startSoftApAndVerifyEnabled(softApConfig, null);
        verify(mWifiNative, never()).setCountryCodeHal(eq(TEST_INTERFACE_NAME), any());
    }

    /**
     * Tests that there is no failure in starting softap in ANY band when no country code is
     * provided.
     */
    @Test
    public void startSoftApOnAnyGhzNoFailForNoCountryCode() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_ANY);
        configBuilder.setSsid(TEST_SSID);
        SoftApModeConfiguration softApConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(),
                mTestSoftApCapability);

        startSoftApAndVerifyEnabled(softApConfig, null);
        verify(mWifiNative, never()).setCountryCodeHal(eq(TEST_INTERFACE_NAME), any());
    }

    /**
     * Tests that there is no failure in starting softap in 2Ghz band when country code cannot be
     * set.
     */
    @Test
    public void startSoftApOn2GhzNoFailForCountryCodeSetFailure() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        SoftApModeConfiguration softApConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(),
                mTestSoftApCapability);

        when(mWifiNative.setCountryCodeHal(eq(TEST_INTERFACE_NAME), any())).thenReturn(false);

        startSoftApAndVerifyEnabled(softApConfig, TEST_COUNTRY_CODE);
        verify(mWifiNative).setCountryCodeHal(
                TEST_INTERFACE_NAME, TEST_COUNTRY_CODE.toUpperCase(Locale.ROOT));
    }

    /**
     * Tests that there is no failure in starting softap in ANY band when country code cannot be
     * set.
     */
    @Test
    public void startSoftApOnAnyNoFailForCountryCodeSetFailure() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_ANY);
        configBuilder.setSsid(TEST_SSID);
        SoftApModeConfiguration softApConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(),
                mTestSoftApCapability);

        when(mWifiNative.setCountryCodeHal(eq(TEST_INTERFACE_NAME), any())).thenReturn(false);

        startSoftApAndVerifyEnabled(softApConfig, TEST_COUNTRY_CODE);
        verify(mWifiNative).setCountryCodeHal(
                TEST_INTERFACE_NAME, TEST_COUNTRY_CODE.toUpperCase(Locale.ROOT));
    }

    /**
     * Tests that the NO_CHANNEL error is propagated and properly reported when starting softap and
     * a valid channel cannot be determined.
     */
    @Test
    public void startSoftApFailNoChannel() throws Exception {
        SoftApCapability noAcsCapability = new SoftApCapability(0);
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_5GHZ);
        configBuilder.setSsid(TEST_SSID);
        SoftApModeConfiguration softApConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(),
                noAcsCapability);

        when(mWifiNative.getChannelsForBand(WifiScanner.WIFI_BAND_5_GHZ))
                .thenReturn(EMPTY_CHANNEL_ARRAY);
        when(mWifiNative.setupInterfaceForSoftApMode(any())).thenReturn(TEST_INTERFACE_NAME);
        when(mWifiNative.isHalStarted()).thenReturn(true);

        SoftApManager newSoftApManager = new SoftApManager(mContext,
                                                           mLooper.getLooper(),
                                                           mFrameworkFacade,
                                                           mWifiNative,
                                                           TEST_COUNTRY_CODE,
                                                           mListener,
                                                           mCallback,
                                                           mWifiApConfigStore,
                                                           softApConfig,
                                                           mWifiMetrics,
                                                           mSarManager,
                                                           mWifiDiagnostics);
        mLooper.dispatchAll();
        newSoftApManager.start();
        mLooper.dispatchAll();

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext, times(2)).sendStickyBroadcastAsUser(intentCaptor.capture(),
                eq(UserHandle.ALL));

        List<Intent> capturedIntents = intentCaptor.getAllValues();
        checkApStateChangedBroadcast(capturedIntents.get(0), WIFI_AP_STATE_ENABLING,
                WIFI_AP_STATE_DISABLED, HOTSPOT_NO_ERROR, TEST_INTERFACE_NAME,
                softApConfig.getTargetMode());
        checkApStateChangedBroadcast(capturedIntents.get(1), WIFI_AP_STATE_FAILED,
                WIFI_AP_STATE_ENABLING, WifiManager.SAP_START_FAILURE_NO_CHANNEL,
                TEST_INTERFACE_NAME, softApConfig.getTargetMode());
    }

    /**
     * Tests startup when Ap Interface fails to start successfully.
     */
    @Test
    public void startSoftApApInterfaceFailedToStart() throws Exception {
        when(mWifiNative.setupInterfaceForSoftApMode(any())).thenReturn(TEST_INTERFACE_NAME);
        when(mWifiNative.startSoftAp(eq(TEST_INTERFACE_NAME), any(), any())).thenReturn(false);

        SoftApModeConfiguration softApModeConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, mDefaultApConfig,
                mTestSoftApCapability);

        SoftApManager newSoftApManager = new SoftApManager(mContext,
                                                           mLooper.getLooper(),
                                                           mFrameworkFacade,
                                                           mWifiNative,
                                                           TEST_COUNTRY_CODE,
                                                           mListener,
                                                           mCallback,
                                                           mWifiApConfigStore,
                                                           softApModeConfig,
                                                           mWifiMetrics,
                                                           mSarManager,
                                                           mWifiDiagnostics);

        mLooper.dispatchAll();
        newSoftApManager.start();
        mLooper.dispatchAll();
        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_FAILED,
                WifiManager.SAP_START_FAILURE_GENERAL);
        verify(mListener).onStartFailure();
        verify(mWifiNative).teardownInterface(TEST_INTERFACE_NAME);
    }

    /**
     * Tests the handling of stop command when soft AP is not started.
     */
    @Test
    public void stopWhenNotStarted() throws Exception {
        mSoftApManager = createSoftApManager(
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability), TEST_COUNTRY_CODE);
        mSoftApManager.stop();
        mLooper.dispatchAll();
        /* Verify no state changes. */
        verify(mCallback, never()).onStateChanged(anyInt(), anyInt());
        verifyNoMoreInteractions(mListener);
        verify(mSarManager, never()).setSapWifiState(anyInt());
        verify(mContext, never()).sendStickyBroadcastAsUser(any(), any());
        verify(mWifiNative, never()).teardownInterface(anyString());
    }

    /**
     * Tests the handling of stop command when soft AP is started.
     */
    @Test
    public void stopWhenStarted() throws Exception {
        SoftApModeConfiguration softApModeConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(softApModeConfig);

        // reset to clear verified Intents for ap state change updates
        reset(mContext);

        InOrder order = inOrder(mCallback, mListener, mContext);

        mSoftApManager.stop();
        assertTrue(mSoftApManager.isStopping());
        mLooper.dispatchAll();

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        order.verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_DISABLING, 0);
        order.verify(mContext).sendStickyBroadcastAsUser(intentCaptor.capture(),
                eq(UserHandle.ALL));
        checkApStateChangedBroadcast(intentCaptor.getValue(), WIFI_AP_STATE_DISABLING,
                WIFI_AP_STATE_ENABLED, HOTSPOT_NO_ERROR, TEST_INTERFACE_NAME,
                softApModeConfig.getTargetMode());

        order.verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_DISABLED, 0);
        verify(mSarManager).setSapWifiState(WifiManager.WIFI_AP_STATE_DISABLED);
        verify(mWifiDiagnostics).stopLogging(TEST_INTERFACE_NAME);
        order.verify(mContext).sendStickyBroadcastAsUser(intentCaptor.capture(),
                eq(UserHandle.ALL));
        checkApStateChangedBroadcast(intentCaptor.getValue(), WIFI_AP_STATE_DISABLED,
                WIFI_AP_STATE_DISABLING, HOTSPOT_NO_ERROR, TEST_INTERFACE_NAME,
                softApModeConfig.getTargetMode());
        order.verify(mListener).onStopped();
        assertFalse(mSoftApManager.isStopping());
    }

    /**
     * Verify that onDestroyed properly reports softap stop.
     */
    @Test
    public void cleanStopOnInterfaceDestroyed() throws Exception {
        SoftApModeConfiguration softApModeConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(softApModeConfig);

        // reset to clear verified Intents for ap state change updates
        reset(mContext);

        InOrder order = inOrder(mCallback, mListener, mContext);

        mWifiNativeInterfaceCallbackCaptor.getValue().onDestroyed(TEST_INTERFACE_NAME);

        mLooper.dispatchAll();
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        order.verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_DISABLING, 0);
        order.verify(mContext).sendStickyBroadcastAsUser(intentCaptor.capture(),
                eq(UserHandle.ALL));
        checkApStateChangedBroadcast(intentCaptor.getValue(), WIFI_AP_STATE_DISABLING,
                WIFI_AP_STATE_ENABLED, HOTSPOT_NO_ERROR, TEST_INTERFACE_NAME,
                softApModeConfig.getTargetMode());

        order.verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_DISABLED, 0);
        order.verify(mContext).sendStickyBroadcastAsUser(intentCaptor.capture(),
                eq(UserHandle.ALL));
        checkApStateChangedBroadcast(intentCaptor.getValue(), WIFI_AP_STATE_DISABLED,
                WIFI_AP_STATE_DISABLING, HOTSPOT_NO_ERROR, TEST_INTERFACE_NAME,
                softApModeConfig.getTargetMode());
        order.verify(mListener).onStopped();
        assertFalse(mSoftApManager.isStopping());
    }

    /**
     * Verify that onDestroyed after softap is stopped doesn't trigger a callback.
     */
    @Test
    public void noCallbackOnInterfaceDestroyedWhenAlreadyStopped() throws Exception {
        SoftApModeConfiguration softApModeConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(softApModeConfig);

        mSoftApManager.stop();
        mLooper.dispatchAll();
        verify(mListener).onStopped();

        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_DISABLING, 0);
        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_DISABLED, 0);

        reset(mCallback);

        // now trigger interface destroyed and make sure callback doesn't get called
        mWifiNativeInterfaceCallbackCaptor.getValue().onDestroyed(TEST_INTERFACE_NAME);
        mLooper.dispatchAll();

        verifyNoMoreInteractions(mCallback, mListener);
    }

    /**
     * Verify that onDown is handled by SoftApManager.
     */
    @Test
    public void testInterfaceOnDownHandled() throws Exception {
        SoftApModeConfiguration softApModeConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(softApModeConfig);

        // reset to clear verified Intents for ap state change updates
        reset(mContext, mCallback, mWifiNative);

        InOrder order = inOrder(mCallback, mListener, mContext);

        mWifiNativeInterfaceCallbackCaptor.getValue().onDown(TEST_INTERFACE_NAME);

        mLooper.dispatchAll();

        order.verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_FAILED,
                WifiManager.SAP_START_FAILURE_GENERAL);
        order.verify(mListener).onStopped();
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext, times(3)).sendStickyBroadcastAsUser(intentCaptor.capture(),
                eq(UserHandle.ALL));

        List<Intent> capturedIntents = intentCaptor.getAllValues();
        checkApStateChangedBroadcast(capturedIntents.get(0), WIFI_AP_STATE_FAILED,
                WIFI_AP_STATE_ENABLED, WifiManager.SAP_START_FAILURE_GENERAL, TEST_INTERFACE_NAME,
                softApModeConfig.getTargetMode());
        checkApStateChangedBroadcast(capturedIntents.get(1), WIFI_AP_STATE_DISABLING,
                WIFI_AP_STATE_FAILED, HOTSPOT_NO_ERROR, TEST_INTERFACE_NAME,
                softApModeConfig.getTargetMode());
        checkApStateChangedBroadcast(capturedIntents.get(2), WIFI_AP_STATE_DISABLED,
                WIFI_AP_STATE_DISABLING, HOTSPOT_NO_ERROR, TEST_INTERFACE_NAME,
                softApModeConfig.getTargetMode());
    }

    /**
     * Verify that onDown for a different interface name does not stop SoftApManager.
     */
    @Test
    public void testInterfaceOnDownForDifferentInterfaceDoesNotTriggerStop() throws Exception {
        SoftApModeConfiguration softApModeConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(softApModeConfig);

        // reset to clear verified Intents for ap state change updates
        reset(mContext, mCallback, mWifiNative);

        InOrder order = inOrder(mCallback, mContext);

        mWifiNativeInterfaceCallbackCaptor.getValue().onDown(OTHER_INTERFACE_NAME);

        mLooper.dispatchAll();

        verifyNoMoreInteractions(mContext, mCallback, mListener, mWifiNative);
    }

    /**
     * Verify that onFailure from hostapd is handled by SoftApManager.
     */
    @Test
    public void testHostapdOnFailureHandled() throws Exception {
        SoftApModeConfiguration softApModeConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(softApModeConfig);

        // reset to clear verified Intents for ap state change updates
        reset(mContext, mCallback, mWifiNative);

        InOrder order = inOrder(mCallback, mListener, mContext);

        mSoftApListenerCaptor.getValue().onFailure();
        mLooper.dispatchAll();

        order.verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_FAILED,
                WifiManager.SAP_START_FAILURE_GENERAL);
        order.verify(mListener).onStopped();
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext, times(3)).sendStickyBroadcastAsUser(intentCaptor.capture(),
                eq(UserHandle.ALL));

        List<Intent> capturedIntents = intentCaptor.getAllValues();
        checkApStateChangedBroadcast(capturedIntents.get(0), WIFI_AP_STATE_FAILED,
                WIFI_AP_STATE_ENABLED, WifiManager.SAP_START_FAILURE_GENERAL, TEST_INTERFACE_NAME,
                softApModeConfig.getTargetMode());
        checkApStateChangedBroadcast(capturedIntents.get(1), WIFI_AP_STATE_DISABLING,
                WIFI_AP_STATE_FAILED, HOTSPOT_NO_ERROR, TEST_INTERFACE_NAME,
                softApModeConfig.getTargetMode());
        checkApStateChangedBroadcast(capturedIntents.get(2), WIFI_AP_STATE_DISABLED,
                WIFI_AP_STATE_DISABLING, HOTSPOT_NO_ERROR, TEST_INTERFACE_NAME,
                softApModeConfig.getTargetMode());
    }

    @Test
    public void updatesMetricsOnChannelSwitchedEvent() throws Exception {
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        final int channelFrequency = 2437;
        final int channelBandwidth = SoftApInfo.CHANNEL_WIDTH_20MHZ_NOHT;
        mSoftApListenerCaptor.getValue().onSoftApChannelSwitched(channelFrequency,
                channelBandwidth);
        mLooper.dispatchAll();

        verify(mWifiMetrics).addSoftApChannelSwitchedEvent(channelFrequency, channelBandwidth,
                apConfig.getTargetMode());
    }

    @Test
    public void updatesMetricsOnChannelSwitchedEventDetectsBandUnsatisfiedOnBand2Ghz()
            throws Exception {
        SoftApConfiguration config = createDefaultApConfig();
        Builder configBuilder = new SoftApConfiguration.Builder(config);
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);

        SoftApModeConfiguration apConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(),
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        final int channelFrequency = 5180;
        final int channelBandwidth = SoftApInfo.CHANNEL_WIDTH_20MHZ_NOHT;
        mSoftApListenerCaptor.getValue().onSoftApChannelSwitched(channelFrequency,
                channelBandwidth);
        mLooper.dispatchAll();

        verify(mWifiMetrics).addSoftApChannelSwitchedEvent(channelFrequency, channelBandwidth,
                apConfig.getTargetMode());
        verify(mWifiMetrics).incrementNumSoftApUserBandPreferenceUnsatisfied();
    }

    @Test
    public void updatesMetricsOnChannelSwitchedEventDetectsBandUnsatisfiedOnBand5Ghz()
            throws Exception {
        SoftApConfiguration config = createDefaultApConfig();
        Builder configBuilder = new SoftApConfiguration.Builder(config);
        configBuilder.setBand(SoftApConfiguration.BAND_5GHZ);

        SoftApModeConfiguration apConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(),
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        final int channelFrequency = 2437;
        final int channelBandwidth = SoftApInfo.CHANNEL_WIDTH_20MHZ_NOHT;
        mSoftApListenerCaptor.getValue().onSoftApChannelSwitched(channelFrequency,
                channelBandwidth);
        mLooper.dispatchAll();

        verify(mWifiMetrics).addSoftApChannelSwitchedEvent(channelFrequency, channelBandwidth,
                apConfig.getTargetMode());
        verify(mWifiMetrics).incrementNumSoftApUserBandPreferenceUnsatisfied();
    }

    @Test
    public void updatesMetricsOnChannelSwitchedEventDoesNotDetectBandUnsatisfiedOnBandAny()
            throws Exception {
        SoftApConfiguration config = createDefaultApConfig();
        Builder configBuilder = new SoftApConfiguration.Builder(config);
        configBuilder.setBand(SoftApConfiguration.BAND_ANY);

        SoftApModeConfiguration apConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(),
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        final int channelFrequency = 5220;
        final int channelBandwidth = SoftApInfo.CHANNEL_WIDTH_20MHZ_NOHT;
        mSoftApListenerCaptor.getValue().onSoftApChannelSwitched(channelFrequency,
                channelBandwidth);
        mLooper.dispatchAll();

        verify(mWifiMetrics).addSoftApChannelSwitchedEvent(channelFrequency, channelBandwidth,
                apConfig.getTargetMode());
        verify(mWifiMetrics, never()).incrementNumSoftApUserBandPreferenceUnsatisfied();
    }

    /**
     * If SoftApManager gets an update for the ap channal and the frequency, it will trigger
     * callbacks to update softap information.
     */
    @Test
    public void testOnSoftApChannelSwitchedEventTriggerSoftApInfoUpdate() throws Exception {
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        mSoftApListenerCaptor.getValue().onSoftApChannelSwitched(
                TEST_AP_FREQUENCY, TEST_AP_BANDWIDTH_FROM_IFACE_CALLBACK);
        mLooper.dispatchAll();

        verify(mCallback).onInfoChanged(mTestSoftApInfo);
        verify(mWifiMetrics).addSoftApChannelSwitchedEvent(TEST_AP_FREQUENCY,
                TEST_AP_BANDWIDTH_IN_SOFTAPINFO, apConfig.getTargetMode());
    }

    /**
     * If SoftApManager gets an update for the ap channal and the frequency those are the same,
     * do not trigger callbacks a second time.
     */
    @Test
    public void testDoesNotTriggerCallbackForSameChannelInfoUpdate() throws Exception {
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        mSoftApListenerCaptor.getValue().onSoftApChannelSwitched(
                TEST_AP_FREQUENCY, TEST_AP_BANDWIDTH_FROM_IFACE_CALLBACK);
        mLooper.dispatchAll();

        // now trigger callback again, but we should have each method only called once
        mSoftApListenerCaptor.getValue().onSoftApChannelSwitched(
                TEST_AP_FREQUENCY, TEST_AP_BANDWIDTH_FROM_IFACE_CALLBACK);
        mLooper.dispatchAll();

        verify(mCallback).onInfoChanged(mTestSoftApInfo);
        verify(mWifiMetrics).addSoftApChannelSwitchedEvent(TEST_AP_FREQUENCY,
                TEST_AP_BANDWIDTH_IN_SOFTAPINFO, apConfig.getTargetMode());
    }

    /**
     * If SoftApManager gets an update for the invalid ap frequency, it will not
     * trigger callbacks
     */
    @Test
    public void testHandlesInvalidChannelFrequency() throws Exception {
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        mSoftApListenerCaptor.getValue().onSoftApChannelSwitched(
                -1, TEST_AP_BANDWIDTH_FROM_IFACE_CALLBACK);
        mLooper.dispatchAll();

        verify(mCallback, never()).onInfoChanged(any());
        verify(mWifiMetrics, never()).addSoftApChannelSwitchedEvent(anyInt(), anyInt(),
                anyInt());
    }

    /**
     * If softap leave started state, it should update softap inforation which frequency is 0 via
     * trigger callbacks.
     */
    @Test
    public void testCallbackForChannelUpdateToZeroWhenLeaveSoftapStarted() throws Exception {
        InOrder order = inOrder(mCallback, mWifiMetrics);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        mSoftApListenerCaptor.getValue().onSoftApChannelSwitched(
                TEST_AP_FREQUENCY, TEST_AP_BANDWIDTH_FROM_IFACE_CALLBACK);
        mLooper.dispatchAll();

        order.verify(mCallback).onInfoChanged(mTestSoftApInfo);
        order.verify(mWifiMetrics).addSoftApChannelSwitchedEvent(TEST_AP_FREQUENCY,
                TEST_AP_BANDWIDTH_IN_SOFTAPINFO, apConfig.getTargetMode());

        mSoftApManager.stop();
        mLooper.dispatchAll();

        mTestSoftApInfo.setFrequency(0);
        mTestSoftApInfo.setBandwidth(SoftApInfo.CHANNEL_WIDTH_INVALID);

        order.verify(mCallback).onInfoChanged(mTestSoftApInfo);
        order.verify(mWifiMetrics, never()).addSoftApChannelSwitchedEvent(0,
                SoftApInfo.CHANNEL_WIDTH_INVALID, apConfig.getTargetMode());
    }

    @Test
    public void updatesConnectedClients() throws Exception {
        InOrder order = inOrder(mCallback, mWifiMetrics);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        order.verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        order.verify(mCallback).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );
        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
    }

    /**
     * If SoftApManager gets an update for the number of connected clients that is the same, do not
     * trigger callbacks a second time.
     */
    @Test
    public void testDoesNotTriggerCallbackForSameClients() throws Exception {
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        // now trigger callback again, but we should have each method only called once
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        // Should just trigger 1 time callback, the first time will be happen when softap enable
        verify(mCallback, times(2)).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, false);
        mLooper.dispatchAll();

        // now trigger callback again, but we should have each method only called once
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, false);
        mLooper.dispatchAll();

        // Should just trigger 1 time callback to update to zero client.
        // Should just trigger 1 time callback, the first time will be happen when softap enable
        verify(mCallback, times(3)).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.size() == 0)
        );

        verify(mWifiMetrics)
                .addSoftApNumAssociatedStationsChangedEvent(
                0, apConfig.getTargetMode());

    }

    @Test
    public void stopDisconnectsConnectedClients() throws Exception {
        InOrder order = inOrder(mCallback, mWifiMetrics);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                        mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        order.verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        order.verify(mCallback).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );
        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());

        mSoftApManager.stop();
        mLooper.dispatchAll();

        verify(mWifiNative).forceClientDisconnect(TEST_INTERFACE_NAME, TEST_MAC_ADDRESS,
                SAP_CLIENT_DISCONNECT_REASON_CODE_UNSPECIFIED);
    }

    @Test
    public void handlesInvalidConnectedClients() throws Exception {
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        /* Invalid values should be ignored */
        final NativeWifiClient mInvalidClient = null;
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(mInvalidClient, true);
        mLooper.dispatchAll();
        verify(mCallback, never()).onConnectedClientsChanged(null);
        verify(mWifiMetrics, never()).addSoftApNumAssociatedStationsChangedEvent(anyInt(),
                anyInt());
    }

    @Test
    public void testCallbackForClientUpdateToZeroWhenLeaveSoftapStarted() throws Exception {
        InOrder order = inOrder(mCallback, mWifiMetrics);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        order.verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        order.verify(mCallback).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );
        order.verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
        // Verify timer is canceled at this point
        verify(mAlarmManager.getAlarmManager()).cancel(any(WakeupMessage.class));

        mSoftApManager.stop();
        mLooper.dispatchAll();

        order.verify(mCallback).onConnectedClientsChanged(new ArrayList<>());
        order.verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(0,
                apConfig.getTargetMode());
        // Verify timer is canceled after stop softap
        verify(mAlarmManager.getAlarmManager()).cancel(any(WakeupMessage.class));
    }

    @Test
    public void testClientConnectFailureWhenClientInBlcokedListAndClientAuthorizationDisabled()
            throws Exception {
        ArrayList<MacAddress> blockedClientList = new ArrayList<>();
        mTestSoftApCapability.setMaxSupportedClients(10);
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setClientControlByUserEnabled(false);
        // Client in blocked list
        blockedClientList.add(TEST_MAC_ADDRESS);
        configBuilder.setBlockedClientList(blockedClientList);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED,
                configBuilder.build(), mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        // Client is not allow verify
        verify(mWifiNative).forceClientDisconnect(
                        TEST_INTERFACE_NAME, TEST_MAC_ADDRESS,
                        WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER);
        verify(mWifiMetrics, never()).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
        verify(mCallback, never()).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );

    }

    @Test
    public void testClientDisconnectWhenClientInBlcokedLisUpdatedtAndClientAuthorizationDisabled()
            throws Exception {
        ArrayList<MacAddress> blockedClientList = new ArrayList<>();
        mTestSoftApCapability.setMaxSupportedClients(10);
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setClientControlByUserEnabled(false);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED,
                configBuilder.build(), mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        // Client connected check
        verify(mWifiNative, never()).forceClientDisconnect(
                        TEST_INTERFACE_NAME, TEST_MAC_ADDRESS,
                        WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER);
        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
        verify(mCallback, times(2)).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );

        reset(mCallback);
        reset(mWifiNative);
        // Update configuration
        blockedClientList.add(TEST_MAC_ADDRESS);
        configBuilder.setBlockedClientList(blockedClientList);
        mSoftApManager.updateConfiguration(configBuilder.build());
        mLooper.dispatchAll();
        // Client difconnected
        verify(mWifiNative).forceClientDisconnect(
                        TEST_INTERFACE_NAME, TEST_MAC_ADDRESS,
                        WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER);
        // The callback should not trigger in configuration update case.
        verify(mCallback, never()).onBlockedClientConnecting(TEST_CONNECTED_CLIENT,
                WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER);

    }



    @Test
    public void testForceClientDisconnectInvokeBecauseClientAuthorizationEnabled()
            throws Exception {
        mTestSoftApCapability.setMaxSupportedClients(10);
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setClientControlByUserEnabled(true);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED,
                configBuilder.build(), mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        // Client is not allow verify
        verify(mWifiNative).forceClientDisconnect(
                        TEST_INTERFACE_NAME, TEST_MAC_ADDRESS,
                        WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER);
        verify(mWifiMetrics, never()).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
        verify(mCallback, never()).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );

    }

    @Test
    public void testClientConnectedAfterUpdateToAllowListwhenClientAuthorizationEnabled()
            throws Exception {
        mTestSoftApCapability.setMaxSupportedClients(10);
        ArrayList<MacAddress> allowedClientList = new ArrayList<>();
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setClientControlByUserEnabled(true);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED,
                configBuilder.build(), mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        // Client is not allow verify
        verify(mWifiNative).forceClientDisconnect(
                        TEST_INTERFACE_NAME, TEST_MAC_ADDRESS,
                        WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER);
        verify(mWifiMetrics, never()).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
        verify(mCallback, never()).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );
        verify(mCallback).onBlockedClientConnecting(TEST_CONNECTED_CLIENT,
                WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER);
        reset(mCallback);
        reset(mWifiNative);
        // Update configuration
        allowedClientList.add(TEST_MAC_ADDRESS);
        configBuilder.setAllowedClientList(allowedClientList);
        mSoftApManager.updateConfiguration(configBuilder.build());
        mLooper.dispatchAll();
        // Client connected again
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();
        verify(mWifiNative, never()).forceClientDisconnect(
                        TEST_INTERFACE_NAME, TEST_MAC_ADDRESS,
                        WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER);
        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
        verify(mCallback).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );
        verify(mCallback, never()).onBlockedClientConnecting(TEST_CONNECTED_CLIENT,
                WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER);
    }

    @Test
    public void testClientConnectedAfterUpdateToBlockListwhenClientAuthorizationEnabled()
            throws Exception {
        mTestSoftApCapability.setMaxSupportedClients(10);
        ArrayList<MacAddress> blockedClientList = new ArrayList<>();
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setClientControlByUserEnabled(true);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED,
                configBuilder.build(), mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        // Client is not allow verify
        verify(mWifiNative).forceClientDisconnect(
                        TEST_INTERFACE_NAME, TEST_MAC_ADDRESS,
                        WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER);
        verify(mWifiMetrics, never()).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
        verify(mCallback, never()).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );
        verify(mCallback).onBlockedClientConnecting(TEST_CONNECTED_CLIENT,
                WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER);
        reset(mCallback);
        reset(mWifiNative);
        // Update configuration
        blockedClientList.add(TEST_MAC_ADDRESS);
        configBuilder.setBlockedClientList(blockedClientList);
        mSoftApManager.updateConfiguration(configBuilder.build());
        mLooper.dispatchAll();
        // Client connected again
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();
        verify(mWifiNative).forceClientDisconnect(
                        TEST_INTERFACE_NAME, TEST_MAC_ADDRESS,
                        WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER);
        verify(mWifiMetrics, never()).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
        verify(mCallback, never()).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );
        verify(mCallback, never()).onBlockedClientConnecting(TEST_CONNECTED_CLIENT,
                WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER);
    }

    @Test
    public void testConfigChangeToSmallAndClientAddBlockListCauseClientDisconnect()
            throws Exception {
        mTestSoftApCapability.setMaxSupportedClients(10);
        ArrayList<MacAddress> allowedClientList = new ArrayList<>();
        allowedClientList.add(TEST_MAC_ADDRESS);
        allowedClientList.add(TEST_MAC_ADDRESS_2);
        ArrayList<MacAddress> blockedClientList = new ArrayList<>();

        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setClientControlByUserEnabled(true);
        configBuilder.setMaxNumberOfClients(2);
        configBuilder.setAllowedClientList(allowedClientList);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED,
                configBuilder.build(), mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        verify(mCallback, times(2)).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );

        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
        // Verify timer is canceled at this point
        verify(mAlarmManager.getAlarmManager()).cancel(any(WakeupMessage.class));

        // Second client connect and max client set is 1.
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT_2, true);
        mLooper.dispatchAll();

        verify(mCallback, times(3)).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT_2))
        );
        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                2, apConfig.getTargetMode());
        reset(mCallback);
        reset(mWifiNative);
        // Update configuration
        allowedClientList.clear();
        allowedClientList.add(TEST_MAC_ADDRESS_2);

        blockedClientList.add(TEST_MAC_ADDRESS);
        configBuilder.setBlockedClientList(blockedClientList);
        configBuilder.setAllowedClientList(allowedClientList);
        configBuilder.setMaxNumberOfClients(1);
        mSoftApManager.updateConfiguration(configBuilder.build());
        mLooper.dispatchAll();
        verify(mWifiNative).forceClientDisconnect(
                        TEST_INTERFACE_NAME, TEST_MAC_ADDRESS,
                        WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER);
        verify(mWifiNative, never()).forceClientDisconnect(
                        TEST_INTERFACE_NAME, TEST_MAC_ADDRESS,
                        WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_NO_MORE_STAS);
    }


    @Test
    public void schedulesTimeoutTimerOnStart() throws Exception {
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);
        verify(mResources)
                .getInteger(R.integer.config_wifiFrameworkSoftApShutDownTimeoutMilliseconds);

        // Verify timer is scheduled
        verify(mAlarmManager.getAlarmManager()).setExact(anyInt(), anyLong(),
                eq(mSoftApManager.SOFT_AP_SEND_MESSAGE_TIMEOUT_TAG), any(), any());
    }

    @Test
    public void schedulesTimeoutTimerOnStartWithConfigedValue() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setShutdownTimeoutMillis(50000);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED,
                configBuilder.build(), mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        // Verify timer is scheduled
        verify(mAlarmManager.getAlarmManager()).setExact(anyInt(), anyLong(),
                eq(mSoftApManager.SOFT_AP_SEND_MESSAGE_TIMEOUT_TAG), any(), any());
    }

    @Test
    public void cancelsTimeoutTimerOnStop() throws Exception {
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);
        mSoftApManager.stop();
        mLooper.dispatchAll();

        // Verify timer is canceled
        verify(mAlarmManager.getAlarmManager()).cancel(any(WakeupMessage.class));
    }

    @Test
    public void cancelsTimeoutTimerOnNewClientsConnect() throws Exception {
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        // Verify timer is canceled
        verify(mAlarmManager.getAlarmManager()).cancel(any(WakeupMessage.class));
    }

    @Test
    public void schedulesTimeoutTimerWhenAllClientsDisconnect() throws Exception {
        InOrder order = inOrder(mCallback, mWifiMetrics);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);
        order.verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();
        order.verify(mCallback).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );
        // Verify timer is canceled at this point
        verify(mAlarmManager.getAlarmManager()).cancel(any(WakeupMessage.class));

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(TEST_NATIVE_CLIENT, false);
        mLooper.dispatchAll();
        // Verify timer is scheduled again
        verify(mAlarmManager.getAlarmManager(), times(2)).setExact(anyInt(), anyLong(),
                eq(mSoftApManager.SOFT_AP_SEND_MESSAGE_TIMEOUT_TAG), any(), any());
    }

    @Test
    public void stopsSoftApOnTimeoutMessage() throws Exception {
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);
        doNothing().when(mFakeSoftApNotifier)
                .showSoftApShutDownTimeoutExpiredNotification();
        mAlarmManager.dispatch(mSoftApManager.SOFT_AP_SEND_MESSAGE_TIMEOUT_TAG);
        mLooper.dispatchAll();

        verify(mWifiNative).teardownInterface(TEST_INTERFACE_NAME);
        verify(mFakeSoftApNotifier).showSoftApShutDownTimeoutExpiredNotification();
    }

    @Test
    public void cancelsTimeoutTimerOnTimeoutToggleChangeWhenNoClients() throws Exception {
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        SoftApConfiguration newConfig = new SoftApConfiguration.Builder(mDefaultApConfig)
                .setAutoShutdownEnabled(false)
                .build();
        mSoftApManager.updateConfiguration(newConfig);
        mLooper.dispatchAll();

        // Verify timer is canceled
        verify(mAlarmManager.getAlarmManager()).cancel(any(WakeupMessage.class));
    }

    @Test
    public void schedulesTimeoutTimerOnTimeoutToggleChangeWhenNoClients() throws Exception {
        // start with timeout toggle disabled
        mDefaultApConfig = new SoftApConfiguration.Builder(mDefaultApConfig)
                .setAutoShutdownEnabled(false)
                .build();
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        SoftApConfiguration newConfig = new SoftApConfiguration.Builder(mDefaultApConfig)
                .setAutoShutdownEnabled(true)
                .build();
        mSoftApManager.updateConfiguration(newConfig);
        mLooper.dispatchAll();

        // Verify timer is scheduled
        verify(mAlarmManager.getAlarmManager()).setExact(anyInt(), anyLong(),
                eq(mSoftApManager.SOFT_AP_SEND_MESSAGE_TIMEOUT_TAG), any(), any());
    }

    @Test
    public void doesNotScheduleTimeoutTimerOnStartWhenTimeoutIsDisabled() throws Exception {
        // start with timeout toggle disabled
        mDefaultApConfig = new SoftApConfiguration.Builder(mDefaultApConfig)
                .setAutoShutdownEnabled(false)
                .build();
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        // Verify timer is not scheduled
        verify(mAlarmManager.getAlarmManager(), never()).setExact(anyInt(), anyLong(),
                eq(mSoftApManager.SOFT_AP_SEND_MESSAGE_TIMEOUT_TAG), any(), any());
    }

    @Test
    public void doesNotScheduleTimeoutTimerWhenAllClientsDisconnectButTimeoutIsDisabled()
            throws Exception {
        // start with timeout toggle disabled
        mDefaultApConfig = new SoftApConfiguration.Builder(mDefaultApConfig)
                .setAutoShutdownEnabled(false)
                .build();
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);
        // add client
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();
        // remove client
        mSoftApListenerCaptor.getValue()
                .onConnectedClientsChanged(TEST_NATIVE_CLIENT, false);
        mLooper.dispatchAll();
        // Verify timer is not scheduled
        verify(mAlarmManager.getAlarmManager(), never()).setExact(anyInt(), anyLong(),
                eq(mSoftApManager.SOFT_AP_SEND_MESSAGE_TIMEOUT_TAG), any(), any());
    }

    @Test
    public void resetsFactoryMacWhenRandomizationOff() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setBssid(null);

        SoftApModeConfiguration apConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(), mTestSoftApCapability);
        ArgumentCaptor<MacAddress> mac = ArgumentCaptor.forClass(MacAddress.class);
        when(mWifiNative.getFactoryMacAddress(TEST_INTERFACE_NAME)).thenReturn(TEST_MAC_ADDRESS);
        when(mWifiNative.setMacAddress(eq(TEST_INTERFACE_NAME), mac.capture())).thenReturn(true);

        startSoftApAndVerifyEnabled(apConfig);

        assertThat(mac.getValue()).isEqualTo(TEST_MAC_ADDRESS);
    }

    @Test
    public void setsCustomMac() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setBssid(MacAddress.fromString("23:34:45:56:67:78"));
        SoftApModeConfiguration apConfig = new SoftApModeConfiguration(
                IFACE_IP_MODE_LOCAL_ONLY, configBuilder.build(), mTestSoftApCapability);
        ArgumentCaptor<MacAddress> mac = ArgumentCaptor.forClass(MacAddress.class);
        when(mWifiNative.setMacAddress(eq(TEST_INTERFACE_NAME), mac.capture())).thenReturn(true);

        startSoftApAndVerifyEnabled(apConfig);

        assertThat(mac.getValue()).isEqualTo(MacAddress.fromString("23:34:45:56:67:78"));
    }

    @Test
    public void setsCustomMacWhenSetMacNotSupport() throws Exception {
        when(mWifiNative.setupInterfaceForSoftApMode(any())).thenReturn(TEST_INTERFACE_NAME);
        when(mWifiNative.isSetMacAddressSupported(any())).thenReturn(false);
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setBssid(MacAddress.fromString("23:34:45:56:67:78"));
        SoftApModeConfiguration apConfig = new SoftApModeConfiguration(
                IFACE_IP_MODE_LOCAL_ONLY, configBuilder.build(), mTestSoftApCapability);
        ArgumentCaptor<MacAddress> mac = ArgumentCaptor.forClass(MacAddress.class);

        mSoftApManager = createSoftApManager(apConfig, TEST_COUNTRY_CODE);
        mSoftApManager.start();
        mLooper.dispatchAll();
        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_ENABLING, 0);
        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_FAILED,
                WifiManager.SAP_START_FAILURE_UNSUPPORTED_CONFIGURATION);
        verify(mWifiNative, never()).setMacAddress(any(), any());
    }

    @Test
    public void setMacFailureWhenCustomMac() throws Exception {
        when(mWifiNative.setupInterfaceForSoftApMode(any())).thenReturn(TEST_INTERFACE_NAME);
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setBssid(MacAddress.fromString("23:34:45:56:67:78"));
        SoftApModeConfiguration apConfig = new SoftApModeConfiguration(
                IFACE_IP_MODE_LOCAL_ONLY, configBuilder.build(), mTestSoftApCapability);
        ArgumentCaptor<MacAddress> mac = ArgumentCaptor.forClass(MacAddress.class);
        when(mWifiNative.setMacAddress(eq(TEST_INTERFACE_NAME), mac.capture())).thenReturn(false);

        mSoftApManager = createSoftApManager(apConfig, TEST_COUNTRY_CODE);
        mSoftApManager.start();
        mLooper.dispatchAll();
        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_ENABLING, 0);
        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_FAILED,
                WifiManager.SAP_START_FAILURE_GENERAL);
        assertThat(mac.getValue()).isEqualTo(MacAddress.fromString("23:34:45:56:67:78"));
    }

    @Test
    public void setMacFailureWhenRandomMac() throws Exception {
        when(mWifiNative.setupInterfaceForSoftApMode(any())).thenReturn(TEST_INTERFACE_NAME);
        when(mWifiApConfigStore.getApConfiguration()).thenReturn(mDefaultApConfig);
        SoftApConfiguration randomizedBssidConfig =
                new SoftApConfiguration.Builder(mDefaultApConfig)
                .setBssid(TEST_MAC_ADDRESS).build();
        when(mWifiApConfigStore.randomizeBssidIfUnset(any(), any())).thenReturn(
                randomizedBssidConfig);
        SoftApModeConfiguration apConfig = new SoftApModeConfiguration(
                IFACE_IP_MODE_LOCAL_ONLY, null, mTestSoftApCapability);
        ArgumentCaptor<MacAddress> mac = ArgumentCaptor.forClass(MacAddress.class);
        when(mWifiNative.setMacAddress(eq(TEST_INTERFACE_NAME), mac.capture())).thenReturn(false);
        mSoftApManager = createSoftApManager(apConfig, TEST_COUNTRY_CODE);
        mSoftApManager.start();
        mLooper.dispatchAll();
        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_ENABLING, 0);
        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_FAILED,
                WifiManager.SAP_START_FAILURE_GENERAL);
    }

    @Test
    public void setRandomMacWhenSetMacNotsupport() throws Exception {
        when(mWifiNative.isSetMacAddressSupported(any())).thenReturn(false);
        SoftApModeConfiguration apConfig = new SoftApModeConfiguration(
                IFACE_IP_MODE_LOCAL_ONLY, null, mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);
        verify(mWifiNative, never()).setMacAddress(any(), any());
    }

    @Test
    public void testForceClientDisconnectInvokeBecauseReachMaxClient() throws Exception {
        mTestSoftApCapability.setMaxSupportedClients(1);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        verify(mCallback, times(2)).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );

        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
        // Verify timer is canceled at this point
        verify(mAlarmManager.getAlarmManager()).cancel(any(WakeupMessage.class));

        // Second client connect and max client set is 1.
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT_2, true);
        mLooper.dispatchAll();
        verify(mWifiNative).forceClientDisconnect(
                        TEST_INTERFACE_NAME, TEST_MAC_ADDRESS_2,
                        WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_NO_MORE_STAS);
        verify(mWifiMetrics, never()).addSoftApNumAssociatedStationsChangedEvent(
                2, apConfig.getTargetMode());
        // Trigger connection again
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT_2, true);
        mLooper.dispatchAll();
        // Verify just update metrics one time
        verify(mWifiMetrics).noteSoftApClientBlocked(1);
    }

    @Test
    public void testCapabilityChangeToSmallCauseClientDisconnect() throws Exception {
        mTestSoftApCapability.setMaxSupportedClients(2);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        verify(mCallback, times(2)).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );

        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
        // Verify timer is canceled at this point
        verify(mAlarmManager.getAlarmManager()).cancel(any(WakeupMessage.class));

        // Second client connect and max client set is 1.
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT_2, true);
        mLooper.dispatchAll();

        verify(mCallback, times(3)).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT_2))
        );
        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                2, apConfig.getTargetMode());

        // Trigger Capability Change
        mTestSoftApCapability.setMaxSupportedClients(1);
        mSoftApManager.updateCapability(mTestSoftApCapability);
        mLooper.dispatchAll();
        // Verify Disconnect will trigger
        verify(mWifiNative).forceClientDisconnect(
                        any(), any(), anyInt());
    }

    @Test
    public void testCapabilityChangeNotApplyForLohsMode() throws Exception {
        mTestSoftApCapability.setMaxSupportedClients(2);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_LOCAL_ONLY, null,
                mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        verify(mCallback, times(2)).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );

        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
        // Verify timer is canceled at this point
        verify(mAlarmManager.getAlarmManager()).cancel(any(WakeupMessage.class));

        // Second client connect and max client set is 1.
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT_2, true);
        mLooper.dispatchAll();

        verify(mCallback, times(3)).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT_2))
        );
        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                2, apConfig.getTargetMode());

        // Trigger Capability Change
        mTestSoftApCapability.setMaxSupportedClients(1);
        mSoftApManager.updateCapability(mTestSoftApCapability);
        mLooper.dispatchAll();
        // Verify Disconnect will NOT trigger even capability changed
        verify(mWifiNative, never()).forceClientDisconnect(
                        any(), any(), anyInt());
    }

    /** Starts soft AP and verifies that it is enabled successfully. */
    protected void startSoftApAndVerifyEnabled(
            SoftApModeConfiguration softApConfig) throws Exception {
        startSoftApAndVerifyEnabled(softApConfig, TEST_COUNTRY_CODE);
    }

    /** Starts soft AP and verifies that it is enabled successfully. */
    protected void startSoftApAndVerifyEnabled(
            SoftApModeConfiguration softApConfig, String countryCode) throws Exception {
        // The expected config to pass to Native
        SoftApConfiguration expectedConfig = null;
        // The config which base on mDefaultApConfig and generate ramdonized mac address
        SoftApConfiguration randomizedBssidConfig = null;
        InOrder order = inOrder(mCallback, mWifiNative);

        SoftApConfiguration config = softApConfig.getSoftApConfiguration();
        if (config == null) {
            // Only generate randomized mac for default config since test case doesn't care it.
            when(mWifiApConfigStore.getApConfiguration()).thenReturn(mDefaultApConfig);
            randomizedBssidConfig =
                    new SoftApConfiguration.Builder(mDefaultApConfig)
                    .setBssid(TEST_MAC_ADDRESS).build();
            when(mWifiApConfigStore.randomizeBssidIfUnset(any(), any())).thenReturn(
                    randomizedBssidConfig);
            expectedConfig = new SoftApConfiguration.Builder(randomizedBssidConfig)
                .setChannel(DEFAULT_AP_CHANNEL, SoftApConfiguration.BAND_2GHZ)
                .build();
        } else {
            expectedConfig = new SoftApConfiguration.Builder(config)
                .setChannel(DEFAULT_AP_CHANNEL, SoftApConfiguration.BAND_2GHZ)
                .build();
        }
        mSoftApManager = createSoftApManager(softApConfig, countryCode);
        mSoftApManager.mSoftApNotifier = mFakeSoftApNotifier;
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);

        when(mWifiNative.setupInterfaceForSoftApMode(any()))
                .thenReturn(TEST_INTERFACE_NAME);


        mSoftApManager.start();
        mSoftApManager.setRole(ActiveModeManager.ROLE_SOFTAP_TETHERED);
        mLooper.dispatchAll();
        verify(mFakeSoftApNotifier).dismissSoftApShutDownTimeoutExpiredNotification();
        order.verify(mWifiNative).setupInterfaceForSoftApMode(
                mWifiNativeInterfaceCallbackCaptor.capture());
        ArgumentCaptor<SoftApConfiguration> configCaptor =
                ArgumentCaptor.forClass(SoftApConfiguration.class);
        order.verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_ENABLING, 0);
        order.verify(mWifiNative).startSoftAp(eq(TEST_INTERFACE_NAME),
                configCaptor.capture(), mSoftApListenerCaptor.capture());
        assertThat(configCaptor.getValue()).isEqualTo(expectedConfig);
        mWifiNativeInterfaceCallbackCaptor.getValue().onUp(TEST_INTERFACE_NAME);
        mLooper.dispatchAll();
        order.verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_ENABLED, 0);
        order.verify(mCallback).onConnectedClientsChanged(new ArrayList<>());
        verify(mSarManager).setSapWifiState(WifiManager.WIFI_AP_STATE_ENABLED);
        verify(mWifiDiagnostics).startLogging(TEST_INTERFACE_NAME);
        verify(mContext, times(2)).sendStickyBroadcastAsUser(intentCaptor.capture(),
                eq(UserHandle.ALL));
        List<Intent> capturedIntents = intentCaptor.getAllValues();
        checkApStateChangedBroadcast(capturedIntents.get(0), WIFI_AP_STATE_ENABLING,
                WIFI_AP_STATE_DISABLED, HOTSPOT_NO_ERROR, TEST_INTERFACE_NAME,
                softApConfig.getTargetMode());
        checkApStateChangedBroadcast(capturedIntents.get(1), WIFI_AP_STATE_ENABLED,
                WIFI_AP_STATE_ENABLING, HOTSPOT_NO_ERROR, TEST_INTERFACE_NAME,
                softApConfig.getTargetMode());
        verify(mListener).onStarted();
        verify(mWifiMetrics).addSoftApUpChangedEvent(true, softApConfig.getTargetMode(),
                TEST_DEFAULT_SHUTDOWN_TIMEOUT_MILLS);
        verify(mWifiMetrics).updateSoftApConfiguration(config == null
                ? randomizedBssidConfig : config, softApConfig.getTargetMode());
        verify(mWifiMetrics).updateSoftApCapability(softApConfig.getCapability(),
                softApConfig.getTargetMode());
    }

    private void checkApStateChangedBroadcast(Intent intent, int expectedCurrentState,
            int expectedPrevState, int expectedErrorCode,
            String expectedIfaceName, int expectedMode) {
        int currentState = intent.getIntExtra(EXTRA_WIFI_AP_STATE, WIFI_AP_STATE_DISABLED);
        int prevState = intent.getIntExtra(EXTRA_PREVIOUS_WIFI_AP_STATE, WIFI_AP_STATE_DISABLED);
        int errorCode = intent.getIntExtra(EXTRA_WIFI_AP_FAILURE_REASON, HOTSPOT_NO_ERROR);
        String ifaceName = intent.getStringExtra(EXTRA_WIFI_AP_INTERFACE_NAME);
        int mode = intent.getIntExtra(EXTRA_WIFI_AP_MODE, WifiManager.IFACE_IP_MODE_UNSPECIFIED);
        assertEquals(expectedCurrentState, currentState);
        assertEquals(expectedPrevState, prevState);
        assertEquals(expectedErrorCode, errorCode);
        assertEquals(expectedIfaceName, ifaceName);
        assertEquals(expectedMode, mode);
    }

    @Test
    public void testForceClientDisconnectNotInvokeWhenNotSupport() throws Exception {
        long testSoftApFeature = SoftApCapability.SOFTAP_FEATURE_WPA3_SAE
                | SoftApCapability.SOFTAP_FEATURE_ACS_OFFLOAD;
        SoftApCapability noClientControlCapability = new SoftApCapability(testSoftApFeature);
        noClientControlCapability.setMaxSupportedClients(1);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, null,
                noClientControlCapability);
        startSoftApAndVerifyEnabled(apConfig);

        verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        verify(mCallback, times(2)).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );

        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
        // Verify timer is canceled at this point
        verify(mAlarmManager.getAlarmManager()).cancel(any(WakeupMessage.class));

        // Second client connect and max client set is 1.
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT_2, true);
        mLooper.dispatchAll();
        // feature not support thus it should not trigger disconnect
        verify(mWifiNative, never()).forceClientDisconnect(
                        any(), any(), anyInt());
        // feature not support thus client still allow connected.
        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                2, apConfig.getTargetMode());
    }

    @Test
    public void testSoftApEnableFailureBecauseSetMaxClientWhenNotSupport() throws Exception {
        long testSoftApFeature = SoftApCapability.SOFTAP_FEATURE_WPA3_SAE
                | SoftApCapability.SOFTAP_FEATURE_ACS_OFFLOAD;
        when(mWifiNative.setupInterfaceForSoftApMode(any())).thenReturn(TEST_INTERFACE_NAME);
        SoftApCapability noClientControlCapability = new SoftApCapability(testSoftApFeature);
        noClientControlCapability.setMaxSupportedClients(1);
        SoftApConfiguration softApConfig = new SoftApConfiguration.Builder(
                mDefaultApConfig).setMaxNumberOfClients(1).build();

        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, softApConfig,
                noClientControlCapability);
        SoftApManager newSoftApManager = new SoftApManager(mContext,
                                                           mLooper.getLooper(),
                                                           mFrameworkFacade,
                                                           mWifiNative,
                                                           TEST_COUNTRY_CODE,
                                                           mListener,
                                                           mCallback,
                                                           mWifiApConfigStore,
                                                           apConfig,
                                                           mWifiMetrics,
                                                           mSarManager,
                                                           mWifiDiagnostics);
        mLooper.dispatchAll();
        newSoftApManager.start();
        mLooper.dispatchAll();
        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_ENABLING, 0);
        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_FAILED,
                WifiManager.SAP_START_FAILURE_UNSUPPORTED_CONFIGURATION);
        verify(mWifiMetrics).incrementSoftApStartResult(false,
                WifiManager.SAP_START_FAILURE_UNSUPPORTED_CONFIGURATION);
        verify(mListener).onStartFailure();
    }

    @Test
    public void testSoftApEnableFailureBecauseSecurityTypeSaeSetupButSaeNotSupport()
            throws Exception {
        long testSoftApFeature = SoftApCapability.SOFTAP_FEATURE_CLIENT_FORCE_DISCONNECT
                | SoftApCapability.SOFTAP_FEATURE_ACS_OFFLOAD;
        when(mWifiNative.setupInterfaceForSoftApMode(any())).thenReturn(TEST_INTERFACE_NAME);
        SoftApCapability noSaeCapability = new SoftApCapability(testSoftApFeature);
        SoftApConfiguration softApConfig = new SoftApConfiguration.Builder(
                mDefaultApConfig).setPassphrase(TEST_PASSWORD,
                SoftApConfiguration.SECURITY_TYPE_WPA3_SAE).build();

        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED, softApConfig,
                noSaeCapability);
        SoftApManager newSoftApManager = new SoftApManager(mContext,
                                                           mLooper.getLooper(),
                                                           mFrameworkFacade,
                                                           mWifiNative,
                                                           TEST_COUNTRY_CODE,
                                                           mListener,
                                                           mCallback,
                                                           mWifiApConfigStore,
                                                           apConfig,
                                                           mWifiMetrics,
                                                           mSarManager,
                                                           mWifiDiagnostics);
        mLooper.dispatchAll();
        newSoftApManager.start();
        mLooper.dispatchAll();
        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_ENABLING, 0);
        verify(mCallback).onStateChanged(WifiManager.WIFI_AP_STATE_FAILED,
                WifiManager.SAP_START_FAILURE_UNSUPPORTED_CONFIGURATION);
        verify(mListener).onStartFailure();
    }

    @Test
    public void testConfigurationChangedApplySinceDoesNotNeedToRestart() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);

        SoftApModeConfiguration apConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(), mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        // Verify timer is scheduled
        verify(mAlarmManager.getAlarmManager()).setExact(anyInt(), anyLong(),
                eq(mSoftApManager.SOFT_AP_SEND_MESSAGE_TIMEOUT_TAG), any(), any());
        verify(mCallback).onConnectedClientsChanged(new ArrayList<>());
        verify(mWifiMetrics).updateSoftApConfiguration(configBuilder.build(),
                WifiManager.IFACE_IP_MODE_TETHERED);

        mLooper.dispatchAll();

        // Trigger Configuration Change
        configBuilder.setShutdownTimeoutMillis(500000);
        mSoftApManager.updateConfiguration(configBuilder.build());
        mLooper.dispatchAll();
        // Verify timer is canceled at this point since timeout changed
        verify(mAlarmManager.getAlarmManager()).cancel(any(WakeupMessage.class));
        // Verify timer setup again
        verify(mAlarmManager.getAlarmManager(), times(2)).setExact(anyInt(), anyLong(),
                eq(mSoftApManager.SOFT_AP_SEND_MESSAGE_TIMEOUT_TAG), any(), any());
        verify(mWifiMetrics).updateSoftApConfiguration(configBuilder.build(),
                WifiManager.IFACE_IP_MODE_TETHERED);

    }

    @Test
    public void testConfigurationChangedDoesNotApplySinceNeedToRestart() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);

        SoftApModeConfiguration apConfig = new SoftApModeConfiguration(
                WifiManager.IFACE_IP_MODE_TETHERED, configBuilder.build(), mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        // Verify timer is scheduled
        verify(mAlarmManager.getAlarmManager()).setExact(anyInt(), anyLong(),
                eq(mSoftApManager.SOFT_AP_SEND_MESSAGE_TIMEOUT_TAG), any(), any());

        mLooper.dispatchAll();

        // Trigger Configuration Change
        configBuilder.setShutdownTimeoutMillis(500000);
        configBuilder.setSsid(TEST_SSID + "new");
        mSoftApManager.updateConfiguration(configBuilder.build());
        mLooper.dispatchAll();
        // Verify timer cancel will not apply since changed config need to apply via restart.
        verify(mAlarmManager.getAlarmManager(), never()).cancel(any(WakeupMessage.class));
    }

    @Test
    public void testConfigChangeToSmallCauseClientDisconnect() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setMaxNumberOfClients(2);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED,
                configBuilder.build(), mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        verify(mCallback, times(2)).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );

        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
        // Verify timer is canceled at this point
        verify(mAlarmManager.getAlarmManager()).cancel(any(WakeupMessage.class));

        // Second client connect and max client set is 2.
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT_2, true);
        mLooper.dispatchAll();

        verify(mCallback, times(3)).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT_2))
        );
        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                2, apConfig.getTargetMode());

        // Trigger Configuration Change
        configBuilder.setMaxNumberOfClients(1);
        mSoftApManager.updateConfiguration(configBuilder.build());
        mLooper.dispatchAll();
        // Verify Disconnect will trigger
        verify(mWifiNative).forceClientDisconnect(
                        any(), any(), anyInt());
    }

    @Test
    public void testConfigChangeWillTriggerUpdateMetricsAgain() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_2GHZ);
        configBuilder.setSsid(TEST_SSID);
        configBuilder.setMaxNumberOfClients(1);
        SoftApModeConfiguration apConfig =
                new SoftApModeConfiguration(WifiManager.IFACE_IP_MODE_TETHERED,
                configBuilder.build(), mTestSoftApCapability);
        startSoftApAndVerifyEnabled(apConfig);

        verify(mCallback).onConnectedClientsChanged(new ArrayList<>());

        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();

        verify(mCallback, times(2)).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT))
        );

        verify(mWifiMetrics).addSoftApNumAssociatedStationsChangedEvent(
                1, apConfig.getTargetMode());
        // Verify timer is canceled at this point
        verify(mAlarmManager.getAlarmManager()).cancel(any(WakeupMessage.class));

        // Second client connect and max client set is 1.
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT_2, true);
        mLooper.dispatchAll();
        verify(mWifiNative).forceClientDisconnect(
                        TEST_INTERFACE_NAME, TEST_MAC_ADDRESS_2,
                        WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_NO_MORE_STAS);

        // Verify update metrics
        verify(mWifiMetrics).noteSoftApClientBlocked(1);

        // Trigger Configuration Change
        configBuilder.setMaxNumberOfClients(2);
        mSoftApManager.updateConfiguration(configBuilder.build());
        mLooper.dispatchAll();

        // Second client connect and max client set is 2.
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT_2, true);
        mLooper.dispatchAll();
        verify(mCallback, times(3)).onConnectedClientsChanged(
                Mockito.argThat((List<WifiClient> clients) ->
                        clients.contains(TEST_CONNECTED_CLIENT_2))
        );

        // Trigger Configuration Change
        configBuilder.setMaxNumberOfClients(1);
        mSoftApManager.updateConfiguration(configBuilder.build());
        mLooper.dispatchAll();
        // Let client disconnect due to maximum number change to small.
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, false);
        mLooper.dispatchAll();

        // Trigger connection again
        mSoftApListenerCaptor.getValue().onConnectedClientsChanged(
                TEST_NATIVE_CLIENT, true);
        mLooper.dispatchAll();
        // Verify just update metrics one time
        verify(mWifiMetrics, times(2)).noteSoftApClientBlocked(1);
    }
}
