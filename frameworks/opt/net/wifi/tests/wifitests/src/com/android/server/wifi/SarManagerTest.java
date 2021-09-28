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

import static android.telephony.TelephonyManager.CALL_STATE_IDLE;
import static android.telephony.TelephonyManager.CALL_STATE_OFFHOOK;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.*;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.test.TestLooper;
import android.telephony.PhoneStateListener;
import android.telephony.TelephonyManager;

import androidx.test.filters.SmallTest;

import com.android.wifi.resources.R;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * unit tests for {@link com.android.server.wifi.SarManager}.
 */
@SmallTest
public class SarManagerTest extends WifiBaseTest {
    private static final String TAG = "WifiSarManagerTest";
    private static final String OP_PACKAGE_NAME = "com.xxx";

    private void enableDebugLogs() {
        mSarMgr.enableVerboseLogging(1);
    }

    private MockResources getMockResources() {
        MockResources resources = new MockResources();
        return resources;
    }

    private SarManager mSarMgr;
    private TestLooper mLooper;
    private MockResources mResources;
    private PhoneStateListener mPhoneStateListener;
    private SarInfo mSarInfo;

    @Mock private Context mContext;
    @Mock TelephonyManager mTelephonyManager;
    @Mock private ApplicationInfo mMockApplInfo;
    @Mock WifiNative mWifiNative;

    @Before
    public void setUp() throws Exception {
        /* Ensure Looper exists */
        mLooper = new TestLooper();

        MockitoAnnotations.initMocks(this);

        /* Default behavior is to return with success */
        when(mWifiNative.selectTxPowerScenario(any(SarInfo.class))).thenReturn(true);

        mResources = getMockResources();

        when(mContext.getResources()).thenReturn(mResources);
        mMockApplInfo.targetSdkVersion = Build.VERSION_CODES.P;
        when(mContext.getApplicationInfo()).thenReturn(mMockApplInfo);
        when(mContext.getOpPackageName()).thenReturn(OP_PACKAGE_NAME);
    }

    @After
    public void cleanUp() throws Exception {
        mSarMgr = null;
        mLooper = null;
        mContext = null;
        mResources = null;
    }

    /**
     * Helper function to capture SarInfo object
     */
    private void captureSarInfo(WifiNative wifiNative) {
        /* Capture the SarInfo */
        ArgumentCaptor<SarInfo> sarInfoCaptor = ArgumentCaptor.forClass(SarInfo.class);
        verify(wifiNative).selectTxPowerScenario(sarInfoCaptor.capture());
        mSarInfo = sarInfoCaptor.getValue();
        assertNotNull(mSarInfo);
    }

    /**
     * Helper function to set configuration for SAR and create the SAR Manager
     *
     */
    private void createSarManager(boolean isSarEnabled, boolean isSarSapEnabled) {
        mResources.setBoolean(
                R.bool.config_wifi_framework_enable_sar_tx_power_limit, isSarEnabled);
        mResources.setBoolean(
                R.bool.config_wifi_framework_enable_soft_ap_sar_tx_power_limit,
                isSarSapEnabled);

        mSarMgr = new SarManager(mContext, mTelephonyManager, mLooper.getLooper(),
                mWifiNative);

        mSarMgr.handleBootCompleted();

        if (isSarEnabled) {
            /* Capture the PhoneStateListener */
            ArgumentCaptor<PhoneStateListener> phoneStateListenerCaptor =
                    ArgumentCaptor.forClass(PhoneStateListener.class);
            verify(mTelephonyManager).listen(phoneStateListenerCaptor.capture(),
                    eq(PhoneStateListener.LISTEN_CALL_STATE));
            mPhoneStateListener = phoneStateListenerCaptor.getValue();
            assertNotNull(mPhoneStateListener);
        }

        /* Enable logs from SarManager */
        enableDebugLogs();
    }

    /**
     * Test that we do register the telephony call state listener on devices which do support
     * setting/resetting Tx power limit.
     */
    @Test
    public void testSarMgr_enabledTxPowerScenario_registerPhone() throws Exception {
        createSarManager(true, false);
        verify(mTelephonyManager).listen(any(), eq(PhoneStateListener.LISTEN_CALL_STATE));
    }

    /**
     * Test that we do not register the telephony call state listener on devices which
     * do not support setting/resetting Tx power limit.
     */
    @Test
    public void testSarMgr_disabledTxPowerScenario_registerPhone() throws Exception {
        createSarManager(false, false);
        verify(mTelephonyManager, never()).listen(any(), anyInt());
    }

    /**
     * Test that for devices that support setting/resetting Tx Power limits, device sets the proper
     * Tx power scenario upon receiving {@link TelephonyManager#CALL_STATE_OFFHOOK} when WiFi STA
     * is enabled
     * In this case Wifi is enabled first, then off-hook is detected
     */
    @Test
    public void testSarMgr_enabledTxPowerScenario_wifiOn_offHook() throws Exception {
        createSarManager(true, false);

        InOrder inOrder = inOrder(mWifiNative);

        /* Enable WiFi State */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_ENABLED);
        captureSarInfo(mWifiNative);

        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);

        /* Set phone state to OFFHOOK */
        mPhoneStateListener.onCallStateChanged(CALL_STATE_OFFHOOK, "");
        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertTrue(mSarInfo.isVoiceCall);
    }

    /**
     * Test that for devices that support setting/resetting Tx Power limits, device sets the proper
     * Tx power scenario upon receiving {@link TelephonyManager#CALL_STATE_OFFHOOK} when WiFi STA
     * is enabled
     * In this case off-hook event is detected first, then wifi is turned on
     */
    @Test
    public void testSarMgr_enabledTxPowerScenario_offHook_wifiOn() throws Exception {
        createSarManager(true, false);

        InOrder inOrder = inOrder(mWifiNative);

        /* Set phone state to OFFHOOK */
        mPhoneStateListener.onCallStateChanged(CALL_STATE_OFFHOOK, "");

        /* Enable WiFi State */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_ENABLED);
        captureSarInfo(mWifiNative);

        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertTrue(mSarInfo.isVoiceCall);
    }

    /**
     * Test that for devices that support setting/resetting Tx Power limits, device sets the proper
     * Tx power scenarios upon receiving {@link TelephonyManager#CALL_STATE_OFFHOOK} and
     * {@link TelephonyManager#CALL_STATE_IDLE} when WiFi STA is enabled
     */
    @Test
    public void testSarMgr_enabledTxPowerScenario_wifiOn_offHook_onHook() throws Exception {
        createSarManager(true, false);

        InOrder inOrder = inOrder(mWifiNative);

        /* Enable WiFi State */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_ENABLED);
        captureSarInfo(mWifiNative);

        /* Now device should set tx power scenario to NORMAL */
        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);

        /* Set phone state to OFFHOOK */
        mPhoneStateListener.onCallStateChanged(CALL_STATE_OFFHOOK, "");

        /* Device should set tx power scenario to Voice call */
        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertTrue(mSarInfo.isVoiceCall);

        /* Set state back to ONHOOK */
        mPhoneStateListener.onCallStateChanged(CALL_STATE_IDLE, "");

        /* Device should set tx power scenario to NORMAL again */
        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);

        /* Disable WiFi State */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_DISABLED);
        inOrder.verify(mWifiNative, never()).selectTxPowerScenario(any(SarInfo.class));
    }

    /**
     * Test that for devices that support setting/resetting Tx Power limits, device does not
     * sets the Tx power scenarios upon receiving {@link TelephonyManager#CALL_STATE_OFFHOOK} and
     * {@link TelephonyManager#CALL_STATE_IDLE} when WiFi STA is disabled
     */
    @Test
    public void testSarMgr_enabledTxPowerScenario_wifiOff_offHook_onHook() throws Exception {
        createSarManager(true, false);

        InOrder inOrder = inOrder(mWifiNative);

        /* Set phone state to OFFHOOK */
        mPhoneStateListener.onCallStateChanged(CALL_STATE_OFFHOOK, "");

        /* Set state back to ONHOOK */
        mPhoneStateListener.onCallStateChanged(CALL_STATE_IDLE, "");

        /* Device should not set tx power scenario at all */
        inOrder.verify(mWifiNative, never()).selectTxPowerScenario(any(SarInfo.class));
    }

    /**
     * Test that for devices supporting SAR the following scenario:
     * - Wifi enabled
     * - A call starts
     * - Wifi disabled
     * - Call ends
     * - Wifi back on
     */
    @Test
    public void testSarMgr_enabledSar_wifiOn_offHook_wifiOff_onHook() throws Exception {
        createSarManager(true, false);

        InOrder inOrder = inOrder(mWifiNative);

        /* Enable WiFi State */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_ENABLED);
        captureSarInfo(mWifiNative);

        /* Now device should set tx power scenario to NORMAL */
        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);

        /* Set phone state to OFFHOOK */
        mPhoneStateListener.onCallStateChanged(CALL_STATE_OFFHOOK, "");
        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertTrue(mSarInfo.isVoiceCall);

        /* Disable WiFi State */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_DISABLED);
        inOrder.verify(mWifiNative, never()).selectTxPowerScenario(any(SarInfo.class));

        /* Set state back to ONHOOK */
        mPhoneStateListener.onCallStateChanged(CALL_STATE_IDLE, "");
        inOrder.verify(mWifiNative, never()).selectTxPowerScenario(any(SarInfo.class));

        /* Enable WiFi State again */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_ENABLED);
        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);
    }

    /**
     * Test that for devices supporting SAR, Wifi disabled, a call starts, a call ends, Wifi
     * enabled.
     */
    @Test
    public void testSarMgr_enabledSar_wifiOff_offHook_onHook_wifiOn() throws Exception {
        createSarManager(true, false);

        InOrder inOrder = inOrder(mWifiNative);

        /* Set phone state to OFFHOOK */
        mPhoneStateListener.onCallStateChanged(CALL_STATE_OFFHOOK, "");
        inOrder.verify(mWifiNative, never()).selectTxPowerScenario(any(SarInfo.class));

        /* Set state back to ONHOOK */
        mPhoneStateListener.onCallStateChanged(CALL_STATE_IDLE, "");
        inOrder.verify(mWifiNative, never()).selectTxPowerScenario(any(SarInfo.class));

        /* Enable WiFi State */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_ENABLED);
        captureSarInfo(mWifiNative);

        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);
    }

    /**
     * Test that for devices supporting SAR, Wifi disabled, a call starts, wifi on, wifi off,
     * call ends.
     */
    @Test
    public void testSarMgr_enabledSar_offHook_wifiOnOff_onHook() throws Exception {
        createSarManager(true, false);

        InOrder inOrder = inOrder(mWifiNative);

        /* Set phone state to OFFHOOK */
        mPhoneStateListener.onCallStateChanged(CALL_STATE_OFFHOOK, "");
        inOrder.verify(mWifiNative, never()).selectTxPowerScenario(any(SarInfo.class));

        /* Enable WiFi State */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_ENABLED);
        captureSarInfo(mWifiNative);
        assertNotNull(mSarInfo);

        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertTrue(mSarInfo.isVoiceCall);

        /* Disable WiFi State */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_DISABLED);
        inOrder.verify(mWifiNative, never()).selectTxPowerScenario(any(SarInfo.class));

        /* Set state back to ONHOOK */
        mPhoneStateListener.onCallStateChanged(CALL_STATE_IDLE, "");
        inOrder.verify(mWifiNative, never()).selectTxPowerScenario(any(SarInfo.class));
    }

    /**
     * Test the error case for for devices supporting SAR, Wifi enabled, a call starts,
     * call ends. With all of these cases, the call to set Tx power scenario fails.
     */
    @Test
    public void testSarMgr_enabledSar_error_wifiOn_offOnHook() throws Exception {
        createSarManager(true, false);

        when(mWifiNative.selectTxPowerScenario(any(SarInfo.class))).thenReturn(false);
        InOrder inOrder = inOrder(mWifiNative);

        /* Enable WiFi State */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_ENABLED);
        captureSarInfo(mWifiNative);
        assertNotNull(mSarInfo);

        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);

        /* Set phone state to OFFHOOK */
        mPhoneStateListener.onCallStateChanged(CALL_STATE_OFFHOOK, "");
        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertTrue(mSarInfo.isVoiceCall);

        /* Set state back to ONHOOK */
        mPhoneStateListener.onCallStateChanged(CALL_STATE_IDLE, "");
        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);
    }

    /**
     * Test that Start of SoftAP for a device that does not have SAR enabled does not result in
     * setting the Tx power scenario
     */
    @Test
    public void testSarMgr_disabledTxPowerScenario_sapOn() throws Exception {
        createSarManager(false, false);

        /* Enable WiFi SoftAP State */
        mSarMgr.setSapWifiState(WifiManager.WIFI_AP_STATE_ENABLED);

        verify(mWifiNative, never()).selectTxPowerScenario(any(SarInfo.class));
    }

    /**
     * Test that Start of SoftAP for a device that has SAR enabled.
     */
    @Test
    public void testSarMgr_enabledTxPowerScenario_sapOn() throws Exception {
        createSarManager(true, false);

        InOrder inOrder = inOrder(mWifiNative);

        /* Enable WiFi SoftAP State */
        mSarMgr.setSapWifiState(WifiManager.WIFI_AP_STATE_ENABLED);
        captureSarInfo(mWifiNative);

        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);
        assertTrue(mSarInfo.isWifiSapEnabled);
        assertFalse(mSarInfo.isWifiScanOnlyEnabled);
    }

    /**
     * Test that for a device that has SAR enabled, when scan-only state is enabled with both SoftAP
     * and Client states disabled, the SarInfo is reported with proper values.
     */
    @Test
    public void testSarMgr_enabledTxPowerScenario_staOff_sapOff_scanOnlyOn() throws Exception {
        createSarManager(true, false);

        /* Enable Wifi ScanOnly State */
        mSarMgr.setScanOnlyWifiState(WifiManager.WIFI_STATE_ENABLED);
        captureSarInfo(mWifiNative);

        verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isWifiSapEnabled);
        assertTrue(mSarInfo.isWifiScanOnlyEnabled);
    }

    /**
     * Test that for a device that has SAR enabled, when scan-only state is enabled, and then
     * client state is enabled, no additional setting of Tx power scenario is initiated
     */
    @Test
    public void testSarMgr_enabledTxPowerScenario_staOn_sapOff_scanOnlyOn() throws Exception {
        createSarManager(true, false);

        InOrder inOrder = inOrder(mWifiNative);

        /* Enable Wifi ScanOnly State */
        mSarMgr.setScanOnlyWifiState(WifiManager.WIFI_STATE_ENABLED);
        captureSarInfo(mWifiNative);

        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isWifiSapEnabled);
        assertTrue(mSarInfo.isWifiScanOnlyEnabled);

        /* Now enable Client state */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_ENABLED);
        inOrder.verify(mWifiNative, never()).selectTxPowerScenario(any(SarInfo.class));
    }

    /**
     * Test the success case for for devices supporting SAR,
     * Wifi enabled, SoftAP enabled, wifi disabled, scan-only enabled, SoftAP disabled.
     *
     * SarManager should report these changes as they occur(only when changes occur to
     * inputs affecting the SAR scenario).
     */
    @Test
    public void testSarMgr_enabledTxPowerScenario_wifi_sap_scanOnly() throws Exception {
        createSarManager(true, false);

        InOrder inOrder = inOrder(mWifiNative);

        /* Enable WiFi State */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_ENABLED);
        captureSarInfo(mWifiNative);

        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);
        assertFalse(mSarInfo.isWifiSapEnabled);
        assertFalse(mSarInfo.isWifiScanOnlyEnabled);

        /* Enable SoftAP state */
        mSarMgr.setSapWifiState(WifiManager.WIFI_AP_STATE_ENABLED);
        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);
        assertTrue(mSarInfo.isWifiSapEnabled);
        assertFalse(mSarInfo.isWifiScanOnlyEnabled);

        /* Disable WiFi State */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_DISABLED);
        inOrder.verify(mWifiNative, never()).selectTxPowerScenario(any(SarInfo.class));

        /* Enable ScanOnly state */
        mSarMgr.setScanOnlyWifiState(WifiManager.WIFI_STATE_ENABLED);
        inOrder.verify(mWifiNative, never()).selectTxPowerScenario(any(SarInfo.class));

        /* Disable SoftAP state */
        mSarMgr.setSapWifiState(WifiManager.WIFI_AP_STATE_DISABLED);
        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);
        assertFalse(mSarInfo.isWifiSapEnabled);
        assertTrue(mSarInfo.isWifiScanOnlyEnabled);
    }

    /**
     * Test the error case for devices supporting SAR,
     * Wifi enabled, SoftAP enabled, wifi disabled, scan-only enabled, SoftAP disabled
     * Throughout this test case, calls to the hal return with error.
     */
    @Test
    public void testSarMgr_enabledTxPowerScenario_error_wifi_sap_scanOnly() throws Exception {
        createSarManager(true, false);

        when(mWifiNative.selectTxPowerScenario(any(SarInfo.class))).thenReturn(false);
        InOrder inOrder = inOrder(mWifiNative);

        /* Enable WiFi State */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_ENABLED);
        captureSarInfo(mWifiNative);

        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);
        assertFalse(mSarInfo.isWifiSapEnabled);
        assertFalse(mSarInfo.isWifiScanOnlyEnabled);

        /* Enable SoftAP state */
        mSarMgr.setSapWifiState(WifiManager.WIFI_AP_STATE_ENABLED);
        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);
        assertTrue(mSarInfo.isWifiSapEnabled);
        assertFalse(mSarInfo.isWifiScanOnlyEnabled);

        /* Disable WiFi State, reporting should still happen */
        mSarMgr.setClientWifiState(WifiManager.WIFI_STATE_DISABLED);
        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);
        assertTrue(mSarInfo.isWifiSapEnabled);
        assertFalse(mSarInfo.isWifiScanOnlyEnabled);
        assertFalse(mSarInfo.isWifiClientEnabled);

        /* Enable ScanOnly state */
        mSarMgr.setScanOnlyWifiState(WifiManager.WIFI_STATE_ENABLED);
        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);
        assertTrue(mSarInfo.isWifiSapEnabled);
        assertTrue(mSarInfo.isWifiScanOnlyEnabled);
        assertFalse(mSarInfo.isWifiClientEnabled);

        /* Disable SoftAP state */
        mSarMgr.setSapWifiState(WifiManager.WIFI_AP_STATE_DISABLED);
        inOrder.verify(mWifiNative).selectTxPowerScenario(eq(mSarInfo));
        assertFalse(mSarInfo.isVoiceCall);
        assertFalse(mSarInfo.isWifiSapEnabled);
        assertTrue(mSarInfo.isWifiScanOnlyEnabled);
        assertFalse(mSarInfo.isWifiClientEnabled);
    }
}
