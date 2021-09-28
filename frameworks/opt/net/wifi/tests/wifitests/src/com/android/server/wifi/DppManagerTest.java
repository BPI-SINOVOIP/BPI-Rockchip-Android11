/*
 * Copyright 2018 The Android Open Source Project
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

import static android.hardware.wifi.supplicant.V1_2.DppAkm.PSK;
import static android.hardware.wifi.supplicant.V1_2.DppAkm.SAE;
import static android.hardware.wifi.supplicant.V1_3.DppFailureCode.AUTHENTICATION;
import static android.hardware.wifi.supplicant.V1_3.DppFailureCode.BUSY;
import static android.hardware.wifi.supplicant.V1_3.DppFailureCode.CANNOT_FIND_NETWORK;
import static android.hardware.wifi.supplicant.V1_3.DppFailureCode.CONFIGURATION;
import static android.hardware.wifi.supplicant.V1_3.DppFailureCode.CONFIGURATION_REJECTED;
import static android.hardware.wifi.supplicant.V1_3.DppFailureCode.ENROLLEE_AUTHENTICATION;
import static android.hardware.wifi.supplicant.V1_3.DppFailureCode.FAILURE;
import static android.hardware.wifi.supplicant.V1_3.DppFailureCode.NOT_COMPATIBLE;
import static android.hardware.wifi.supplicant.V1_3.DppFailureCode.NOT_SUPPORTED;
import static android.hardware.wifi.supplicant.V1_3.DppFailureCode.TIMEOUT;
import static android.hardware.wifi.supplicant.V1_3.DppProgressCode.AUTHENTICATION_SUCCESS;
import static android.hardware.wifi.supplicant.V1_3.DppProgressCode.CONFIGURATION_ACCEPTED;
import static android.hardware.wifi.supplicant.V1_3.DppProgressCode.CONFIGURATION_SENT_WAITING_RESPONSE;
import static android.hardware.wifi.supplicant.V1_3.DppProgressCode.RESPONSE_PENDING;
import static android.hardware.wifi.supplicant.V1_3.DppSuccessCode.CONFIGURATION_APPLIED;
import static android.hardware.wifi.supplicant.V1_3.DppSuccessCode.CONFIGURATION_SENT;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_AUTHENTICATION;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_BUSY;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_CANNOT_FIND_NETWORK;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_CONFIGURATION;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_ENROLLEE_AUTHENTICATION;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_ENROLLEE_REJECTED_CONFIGURATION;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_GENERIC;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_INVALID_NETWORK;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_NOT_COMPATIBLE;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_NOT_SUPPORTED;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_TIMEOUT;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_PROGRESS_AUTHENTICATION_SUCCESS;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_PROGRESS_CONFIGURATION_ACCEPTED;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_PROGRESS_CONFIGURATION_SENT_WAITING_RESPONSE;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_PROGRESS_RESPONSE_PENDING;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_SUCCESS_CONFIGURATION_APPLIED;
import static android.net.wifi.EasyConnectStatusCallback.EASY_CONNECT_EVENT_SUCCESS_CONFIGURATION_SENT;
import static android.net.wifi.WifiManager.EASY_CONNECT_NETWORK_ROLE_STA;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.net.wifi.EasyConnectStatusCallback;
import android.net.wifi.IDppCallback;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiSsid;
import android.os.Handler;
import android.os.IBinder;
import android.os.test.TestLooper;
import android.test.suitebuilder.annotation.SmallTest;

import com.android.internal.util.WakeupMessage;

import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for {@link DppManager}.
 */
@SmallTest
public class DppManagerTest extends WifiBaseTest {
    private static final String TAG = "DppManagerTest";
    private static final String TEST_INTERFACE_NAME = "testif0";
    private static final int TEST_PEER_ID = 1;
    private static final String TEST_SSID = "\"Test_SSID\"";
    private static final String TEST_SSID_NO_QUOTE = TEST_SSID.replace("\"", "");
    private static final String TEST_SSID_ENCODED = "546573745f53534944";
    private static final String TEST_PASSWORD = "\"secretPassword\"";
    private static final String TEST_PASSWORD_ENCODED = "73656372657450617373776f7264";
    private static final int TEST_NETWORK_ID = 1;
    private static final String TEST_BSSID = "01:02:03:04:05:06";

    TestLooper mLooper;

    DppManager mDppManager;

    @Mock
    Context mContext;
    @Mock
    WifiMetrics mWifiMetrics;
    @Mock
    WifiNative mWifiNative;
    @Mock
    WifiConfigManager mWifiConfigManager;
    @Mock
    IBinder mBinder;
    @Mock
    IDppCallback mDppCallback;
    @Mock
    IDppCallback mDppCallbackConcurrent;
    @Mock
    WakeupMessage mWakeupMessage;
    @Mock
    DppMetrics mDppMetrics;
    @Mock
    ScanRequestProxy mScanRequestProxy;

    String mUri =
            "DPP:C:81/1;I:DPP_TESTER;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADebGHMJoCcE7OZP/aek5muaJo"
                    + "zGy2FVKPRjA/I/qyC8Q=;;";

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mLooper = new TestLooper();
        mDppManager = createDppManager();
        mLooper.dispatchAll();

        // Add Peer URI successful, return peer ID 1
        when(mWifiNative.addDppPeerUri(anyString(), anyString()))
                .thenReturn(1);
        when(mWifiNative.removeDppUri(anyString(), anyInt()))
                .thenReturn(true);

        // Return test interface name
        when(mWifiNative.getClientInterfaceName()).thenReturn(TEST_INTERFACE_NAME);

        // Successfully start enrollee
        when(mWifiNative.startDppEnrolleeInitiator(anyString(), anyInt(), anyInt())).thenReturn(
                true);

        // Successfully start configurator
        when(mWifiNative.startDppConfiguratorInitiator(anyString(), anyInt(), anyInt(), anyString(),
                any(), any(), anyInt(), anyInt())).thenReturn(true);
    }

    private DppManager createDppManager() {
        DppManager dppManger = new DppManager(new Handler(mLooper.getLooper()), mWifiNative,
                mWifiConfigManager, mContext, mDppMetrics, mScanRequestProxy);
        dppManger.mDppTimeoutMessage = mWakeupMessage;
        dppManger.enableVerboseLogging(1);
        return dppManger;
    }

    /**
     * DPP Tests
     */
    @Test
    public void testStartDppAsConfiguratorInitiatorWithBadNetworkId() throws Exception {
        // Return NULL when for the selected network (invalid network)
        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(null);

        mDppManager.startDppAsConfiguratorInitiator(0, mBinder, mUri, 1,
                EASY_CONNECT_NETWORK_ROLE_STA,
                mDppCallback);
        verify(mDppCallback).onFailure(
                eq(EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_INVALID_NETWORK), eq(null),
                eq(null), eq(new int[0]));
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mDppMetrics).updateDppConfiguratorInitiatorRequests();
        verify(mDppMetrics).updateDppFailure(eq(EasyConnectStatusCallback
                .EASY_CONNECT_EVENT_FAILURE_INVALID_NETWORK));
        verifyNoMoreInteractions(mDppMetrics);
    }

    @Test
    public void testStartDppAsConfiguratorInitiatorFailAddPeer() throws Exception {
        // Generate a mock WifiConfiguration object
        WifiConfiguration selectedNetwork = new WifiConfiguration();
        selectedNetwork.SSID = "\"Test_SSID\"";
        selectedNetwork.networkId = TEST_NETWORK_ID;
        selectedNetwork.preSharedKey = "\"secretPassword\"";
        selectedNetwork.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_PSK);

        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(
                selectedNetwork);

        // Fail to add Peer URI
        when(mWifiNative.addDppPeerUri(anyString(), anyString())).thenReturn(-1);

        mDppManager.startDppAsConfiguratorInitiator(0, mBinder, mUri, 1,
                EASY_CONNECT_NETWORK_ROLE_STA,
                mDppCallback);
        verify(mDppCallback).onFailure(
                eq(EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_INVALID_URI), eq(null),
                eq(null), eq(new int[0]));
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mDppMetrics).updateDppConfiguratorInitiatorRequests();
        verify(mDppMetrics).updateDppFailure(eq(EasyConnectStatusCallback
                .EASY_CONNECT_EVENT_FAILURE_INVALID_URI));
        verify(mDppMetrics).updateDppOperationTime(anyInt());
        verifyNoMoreInteractions(mDppMetrics);
    }

    @Test
    public void testStartDppAsEnrolleeInitiatorFailAddPeer() throws Exception {
        // Fail to add Peer URI
        when(mWifiNative.addDppPeerUri(anyString(), anyString())).thenReturn(-1);

        mDppManager.startDppAsEnrolleeInitiator(0, mBinder, mUri, mDppCallback);
        verify(mDppCallback).onFailure(
                eq(EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_INVALID_URI), eq(null),
                eq(null), eq(new int[0]));
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mDppMetrics).updateDppEnrolleeInitiatorRequests();
        verify(mDppMetrics).updateDppFailure(eq(EasyConnectStatusCallback
                .EASY_CONNECT_EVENT_FAILURE_INVALID_URI));
        verify(mDppMetrics).updateDppOperationTime(anyInt());
        verifyNoMoreInteractions(mDppMetrics);
    }

    @Test
    public void testStartDppAsConfiguratorInitiatorFailStart() throws Exception {
        // Generate a mock WifiConfiguration object
        WifiConfiguration selectedNetwork = new WifiConfiguration();
        selectedNetwork.SSID = "\"Test_SSID\"";
        selectedNetwork.networkId = TEST_NETWORK_ID;
        selectedNetwork.preSharedKey = "\"secretPassword\"";
        selectedNetwork.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_PSK);

        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(
                selectedNetwork);

        // Fail to start
        when(mWifiNative.startDppConfiguratorInitiator(anyString(), anyInt(), anyInt(), anyString(),
                any(), any(), anyInt(), anyInt())).thenReturn(false);

        mDppManager.startDppAsConfiguratorInitiator(0, mBinder, mUri, 1,
                EASY_CONNECT_NETWORK_ROLE_STA,
                mDppCallback);
        verify(mDppCallback).onFailure(eq(EASY_CONNECT_EVENT_FAILURE_GENERIC), eq(null),
                eq(null), eq(new int[0]));
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mDppMetrics).updateDppConfiguratorInitiatorRequests();
        verify(mDppMetrics).updateDppFailure(eq(EASY_CONNECT_EVENT_FAILURE_GENERIC));
        verify(mDppMetrics).updateDppOperationTime(anyInt());
        verifyNoMoreInteractions(mDppMetrics);
    }

    @Test
    public void testStartDppAsEnrolleeInitiatorFailStart() throws Exception {
        // Fail to start
        when(mWifiNative.startDppEnrolleeInitiator(anyString(), anyInt(), anyInt())).thenReturn(
                false);

        mDppManager.startDppAsEnrolleeInitiator(0, mBinder, mUri, mDppCallback);
        verify(mDppCallback).onFailure(eq(EASY_CONNECT_EVENT_FAILURE_GENERIC), eq(null),
                eq(null), eq(new int[0]));
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mDppMetrics).updateDppEnrolleeInitiatorRequests();
        verify(mDppMetrics).updateDppFailure(eq(EASY_CONNECT_EVENT_FAILURE_GENERIC));
        verify(mDppMetrics).updateDppOperationTime(anyInt());
        verifyNoMoreInteractions(mDppMetrics);
    }

    @Test
    public void testStartDppAsConfiguratorInitiatorStartCorrectlyWpa2Psk() throws Exception {
        // Generate a mock WifiConfiguration object
        WifiConfiguration selectedNetwork = new WifiConfiguration();
        selectedNetwork.SSID = TEST_SSID;
        selectedNetwork.networkId = TEST_NETWORK_ID;
        selectedNetwork.preSharedKey = TEST_PASSWORD;
        selectedNetwork.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_PSK);

        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(
                selectedNetwork);

        mDppManager.startDppAsConfiguratorInitiator(0, mBinder, mUri, 1,
                EASY_CONNECT_NETWORK_ROLE_STA,
                mDppCallback);
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mWifiNative).startDppConfiguratorInitiator(eq(TEST_INTERFACE_NAME),
                eq(TEST_PEER_ID), anyInt(), eq(TEST_SSID_ENCODED), eq(TEST_PASSWORD_ENCODED), any(),
                eq(EASY_CONNECT_NETWORK_ROLE_STA), eq(PSK));
        verify(mDppMetrics).updateDppConfiguratorInitiatorRequests();
        verifyNoMoreInteractions(mDppMetrics);
    }

    @Test
    public void testStartDppAsConfiguratorInitiatorStartCorrectlyWpa3Sae() throws Exception {
        // Generate a mock WifiConfiguration object
        WifiConfiguration selectedNetwork = new WifiConfiguration();
        selectedNetwork.SSID = TEST_SSID;
        selectedNetwork.networkId = TEST_NETWORK_ID;
        selectedNetwork.preSharedKey = TEST_PASSWORD;
        selectedNetwork.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.SAE);

        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(
                selectedNetwork);

        mDppManager.startDppAsConfiguratorInitiator(0, mBinder, mUri, 1,
                EASY_CONNECT_NETWORK_ROLE_STA,
                mDppCallback);
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mWifiNative).startDppConfiguratorInitiator(eq(TEST_INTERFACE_NAME),
                eq(TEST_PEER_ID), anyInt(), eq(TEST_SSID_ENCODED), eq(TEST_PASSWORD_ENCODED), any(),
                eq(EASY_CONNECT_NETWORK_ROLE_STA), eq(SAE));
        verify(mDppMetrics).updateDppConfiguratorInitiatorRequests();
        verifyNoMoreInteractions(mDppMetrics);
    }

    @Test
    public void testStartDppAsConfiguratorInitiatorFailOpenNetwork() throws Exception {
        // Generate a mock WifiConfiguration object
        WifiConfiguration selectedNetwork = new WifiConfiguration();
        selectedNetwork.SSID = TEST_SSID;
        selectedNetwork.networkId = TEST_NETWORK_ID;
        selectedNetwork.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);

        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(
                selectedNetwork);

        mDppManager.startDppAsConfiguratorInitiator(0, mBinder, mUri, 1,
                EASY_CONNECT_NETWORK_ROLE_STA,
                mDppCallback);
        verify(mDppCallback).onFailure(eq(EASY_CONNECT_EVENT_FAILURE_INVALID_NETWORK), eq(null),
                eq(null), eq(new int[0]));
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mDppMetrics).updateDppConfiguratorInitiatorRequests();
        verify(mDppMetrics).updateDppFailure(eq(EasyConnectStatusCallback
                .EASY_CONNECT_EVENT_FAILURE_INVALID_NETWORK));
        verifyNoMoreInteractions(mDppMetrics);
    }

    @Test
    public void testStartDppAsConfiguratorInitiatorFailEapNetwork() throws Exception {
        // Generate a mock WifiConfiguration object
        WifiConfiguration selectedNetwork = new WifiConfiguration();
        selectedNetwork.SSID = TEST_SSID;
        selectedNetwork.networkId = TEST_NETWORK_ID;
        selectedNetwork.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_EAP);

        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(
                selectedNetwork);

        mDppManager.startDppAsConfiguratorInitiator(0, mBinder, mUri, 1,
                EASY_CONNECT_NETWORK_ROLE_STA,
                mDppCallback);
        verify(mDppCallback).onFailure(eq(EASY_CONNECT_EVENT_FAILURE_INVALID_NETWORK), eq(null),
                eq(null), eq(new int[0]));
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mDppMetrics).updateDppConfiguratorInitiatorRequests();
        verify(mDppMetrics).updateDppFailure(eq(EasyConnectStatusCallback
                .EASY_CONNECT_EVENT_FAILURE_INVALID_NETWORK));
        verifyNoMoreInteractions(mDppMetrics);
    }

    @Test
    public void testStartDppAsEnrolleeInitiatorStartCorrectly() throws Exception {
        mDppManager.startDppAsEnrolleeInitiator(0, mBinder, mUri, mDppCallback);
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mWifiNative).startDppEnrolleeInitiator(eq(TEST_INTERFACE_NAME),
                eq(TEST_PEER_ID), anyInt());
        verify(mDppMetrics).updateDppEnrolleeInitiatorRequests();
        verifyNoMoreInteractions(mDppMetrics);
    }

    @Test
    public void testStartDppAsConfiguratorInitiatorStartCorrectlyAndRejectConcurrentRequest()
            throws Exception {
        // Generate a mock WifiConfiguration object
        WifiConfiguration selectedNetwork = new WifiConfiguration();
        selectedNetwork.SSID = TEST_SSID;
        selectedNetwork.networkId = 1;
        selectedNetwork.preSharedKey = TEST_PASSWORD;
        selectedNetwork.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.SAE);

        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(
                selectedNetwork);

        mDppManager.startDppAsConfiguratorInitiator(0, mBinder, mUri, 1,
                EASY_CONNECT_NETWORK_ROLE_STA, mDppCallback);
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mWifiNative).startDppConfiguratorInitiator(eq(TEST_INTERFACE_NAME),
                eq(TEST_PEER_ID), anyInt(), eq(TEST_SSID_ENCODED), eq(TEST_PASSWORD_ENCODED), any(),
                eq(EASY_CONNECT_NETWORK_ROLE_STA), eq(SAE));

        mDppManager.startDppAsConfiguratorInitiator(1, mBinder, mUri, 1,
                EASY_CONNECT_NETWORK_ROLE_STA,
                mDppCallbackConcurrent);
        verify(mDppCallbackConcurrent).onFailure(eq(EASY_CONNECT_EVENT_FAILURE_BUSY), eq(null),
                eq(null), eq(new int[0]));
        verify(mDppCallbackConcurrent, never()).onSuccess(anyInt());
        verify(mDppCallbackConcurrent, never()).onSuccessConfigReceived(anyInt());
        verify(mDppMetrics, times(2)).updateDppConfiguratorInitiatorRequests();
        verify(mDppMetrics).updateDppFailure(eq(EASY_CONNECT_EVENT_FAILURE_BUSY));
        verifyNoMoreInteractions(mDppMetrics);
    }

    @Test
    public void testStartDppAsEnrolleeInitiatorStartCorrectlyAndRejectConcurrentRequest()
            throws Exception {
        mDppManager.startDppAsEnrolleeInitiator(0, mBinder, mUri, mDppCallback);
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mWifiNative).startDppEnrolleeInitiator(eq(TEST_INTERFACE_NAME),
                eq(TEST_PEER_ID), anyInt());

        mDppManager.startDppAsEnrolleeInitiator(1, mBinder, mUri, mDppCallbackConcurrent);
        verify(mDppCallbackConcurrent).onFailure(eq(EASY_CONNECT_EVENT_FAILURE_BUSY), eq(null),
                eq(null), eq(new int[0]));
        verify(mDppCallbackConcurrent, never()).onSuccess(anyInt());
        verify(mDppCallbackConcurrent, never()).onSuccessConfigReceived(anyInt());
        verify(mDppMetrics, times(2)).updateDppEnrolleeInitiatorRequests();
        verify(mDppMetrics).updateDppFailure(eq(EASY_CONNECT_EVENT_FAILURE_BUSY));
        verifyNoMoreInteractions(mDppMetrics);
    }

    @Test
    public void testStartDppAsConfiguratorInitiatorStartOnSuccessCallback() throws Exception {
        // Generate a mock WifiConfiguration object
        WifiConfiguration selectedNetwork = new WifiConfiguration();
        selectedNetwork.SSID = TEST_SSID;
        selectedNetwork.networkId = 1;
        selectedNetwork.preSharedKey = TEST_PASSWORD;
        selectedNetwork.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.SAE);

        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(
                selectedNetwork);

        ArgumentCaptor<WifiNative.DppEventCallback> dppEventCallbackCaptor =
                ArgumentCaptor.forClass(
                        WifiNative.DppEventCallback.class);

        mDppManager.startDppAsConfiguratorInitiator(0, mBinder, mUri, 1,
                EASY_CONNECT_NETWORK_ROLE_STA, mDppCallback);
        verify(mWifiNative).registerDppEventCallback(dppEventCallbackCaptor.capture());
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mWifiNative).startDppConfiguratorInitiator(eq(TEST_INTERFACE_NAME),
                eq(TEST_PEER_ID), anyInt(), eq(TEST_SSID_ENCODED), eq(TEST_PASSWORD_ENCODED), any(),
                eq(EASY_CONNECT_NETWORK_ROLE_STA), eq(
                        SAE));

        WifiNative.DppEventCallback dppEventCallback = dppEventCallbackCaptor.getValue();

        // Generate a progress event
        dppEventCallback.onProgress(AUTHENTICATION_SUCCESS);
        mLooper.dispatchAll();
        verify(mDppCallback).onProgress(eq(EASY_CONNECT_EVENT_PROGRESS_AUTHENTICATION_SUCCESS));

        // Generate an onSuccess callback
        dppEventCallback.onSuccess(CONFIGURATION_SENT);
        mLooper.dispatchAll();
        verify(mDppCallback).onSuccess(eq(EASY_CONNECT_EVENT_SUCCESS_CONFIGURATION_SENT));
        verify(mDppMetrics, times(1)).updateDppR1CapableEnrolleeResponderDevices();
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppMetrics).updateDppConfiguratorInitiatorRequests();
        verify(mDppMetrics).updateDppConfiguratorSuccess(eq(EasyConnectStatusCallback
                .EASY_CONNECT_EVENT_SUCCESS_CONFIGURATION_SENT));
        verify(mDppMetrics).updateDppOperationTime(anyInt());
        verifyNoMoreInteractions(mDppMetrics);
    }

    @Test
    public void testStartDppAsEnrolleeInitiatorStartCorrectlyOnSuccessCallback() throws Exception {
        ArgumentCaptor<WifiNative.DppEventCallback> dppEventCallbackCaptor =
                ArgumentCaptor.forClass(
                        WifiNative.DppEventCallback.class);

        mDppManager.startDppAsEnrolleeInitiator(0, mBinder, mUri, mDppCallback);
        verify(mWifiNative).registerDppEventCallback(dppEventCallbackCaptor.capture());
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mWifiNative).startDppEnrolleeInitiator(eq(TEST_INTERFACE_NAME),
                eq(TEST_PEER_ID), anyInt());

        // Generate an onSuccessConfigReceived callback
        WifiNative.DppEventCallback dppEventCallback = dppEventCallbackCaptor.getValue();

        // Generate a mock WifiConfiguration object
        WifiConfiguration selectedNetwork = new WifiConfiguration();
        selectedNetwork.SSID = TEST_SSID;
        selectedNetwork.networkId = TEST_NETWORK_ID;
        selectedNetwork.preSharedKey = TEST_PASSWORD;
        selectedNetwork.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.SAE);

        // Generate a progress event
        dppEventCallback.onProgress(AUTHENTICATION_SUCCESS);
        mLooper.dispatchAll();
        verify(mDppCallback).onProgress(eq(EASY_CONNECT_EVENT_PROGRESS_AUTHENTICATION_SUCCESS));

        // Generate result
        NetworkUpdateResult networkUpdateResult = new NetworkUpdateResult(TEST_NETWORK_ID);
        when(mWifiConfigManager.addOrUpdateNetwork(any(WifiConfiguration.class),
                anyInt())).thenReturn(networkUpdateResult);

        dppEventCallback.onSuccessConfigReceived(selectedNetwork);
        mLooper.dispatchAll();
        verify(mDppCallback).onSuccessConfigReceived(eq(TEST_NETWORK_ID));
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppMetrics).updateDppEnrolleeInitiatorRequests();
        verify(mDppMetrics).updateDppEnrolleeSuccess();
        verify(mDppMetrics).updateDppOperationTime(anyInt());
        verifyNoMoreInteractions(mDppMetrics);
        verifyCleanUpResources();
    }

    @Test
    public void testStartDppAsConfiguratorInitiatorStartOnFailureCallback() throws Exception {
        // Generate a mock WifiConfiguration object
        WifiConfiguration selectedNetwork = new WifiConfiguration();
        selectedNetwork.SSID = TEST_SSID;
        selectedNetwork.networkId = TEST_NETWORK_ID;
        selectedNetwork.preSharedKey = TEST_PASSWORD;
        selectedNetwork.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.SAE);

        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(
                selectedNetwork);
        ArgumentCaptor<WifiNative.DppEventCallback> dppEventCallbackCaptor =
                ArgumentCaptor.forClass(
                        WifiNative.DppEventCallback.class);

        mDppManager.startDppAsConfiguratorInitiator(0, mBinder, mUri, 1,
                EASY_CONNECT_NETWORK_ROLE_STA,
                mDppCallback);
        verify(mWifiNative).registerDppEventCallback(dppEventCallbackCaptor.capture());
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mWifiNative).startDppConfiguratorInitiator(eq(TEST_INTERFACE_NAME),
                eq(TEST_PEER_ID), anyInt(), eq(TEST_SSID_ENCODED), eq(TEST_PASSWORD_ENCODED), any(),
                eq(EASY_CONNECT_NETWORK_ROLE_STA), eq(
                        SAE));

        WifiNative.DppEventCallback dppEventCallback = dppEventCallbackCaptor.getValue();

        // Generate a progress event
        dppEventCallback.onProgress(RESPONSE_PENDING);
        mLooper.dispatchAll();
        verify(mDppCallback).onProgress(eq(EASY_CONNECT_EVENT_PROGRESS_RESPONSE_PENDING));

        // Generate an onFailure callback
        dppEventCallback.onFailure(AUTHENTICATION, null, null, null);
        mLooper.dispatchAll();
        verify(mDppCallback).onFailure(eq(EASY_CONNECT_EVENT_FAILURE_AUTHENTICATION), eq(null),
                eq(null), eq(new int[0]));
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mDppMetrics).updateDppConfiguratorInitiatorRequests();
        verify(mDppMetrics).updateDppFailure(eq(EasyConnectStatusCallback
                .EASY_CONNECT_EVENT_FAILURE_AUTHENTICATION));
        verify(mDppMetrics).updateDppOperationTime(anyInt());
        verifyNoMoreInteractions(mDppMetrics);
        verifyCleanUpResources();
    }

    @Test
    public void testStartDppAsEnrolleeInitiatorStartCorrectlyOnFailureCallback() throws Exception {
        // Add Peer URI successful, return peer ID 1
        when(mWifiNative.addDppPeerUri(anyString(), anyString()))
                .thenReturn(1);
        when(mWifiNative.removeDppUri(anyString(), anyInt()))
                .thenReturn(true);

        // Return test interface name
        when(mWifiNative.getClientInterfaceName()).thenReturn(TEST_INTERFACE_NAME);

        // Successful start
        when(mWifiNative.startDppEnrolleeInitiator(anyString(), anyInt(), anyInt())).thenReturn(
                true);

        ArgumentCaptor<WifiNative.DppEventCallback> dppEventCallbackCaptor =
                ArgumentCaptor.forClass(
                        WifiNative.DppEventCallback.class);

        mDppManager.startDppAsEnrolleeInitiator(0, mBinder, mUri, mDppCallback);
        verify(mWifiNative).registerDppEventCallback(dppEventCallbackCaptor.capture());
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mWifiNative).startDppEnrolleeInitiator(eq(TEST_INTERFACE_NAME),
                eq(TEST_PEER_ID), anyInt());

        WifiNative.DppEventCallback dppEventCallback = dppEventCallbackCaptor.getValue();

        // Generate a progress event
        dppEventCallback.onProgress(RESPONSE_PENDING);
        mLooper.dispatchAll();
        verify(mDppCallback).onProgress(eq(EASY_CONNECT_EVENT_PROGRESS_RESPONSE_PENDING));

        // Generate an onFailure callback
        dppEventCallback.onFailure(AUTHENTICATION, null, null, null);
        mLooper.dispatchAll();
        verify(mDppCallback).onFailure(eq(EASY_CONNECT_EVENT_FAILURE_AUTHENTICATION), eq(null),
                eq(null), eq(new int[0]));
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mDppMetrics).updateDppEnrolleeInitiatorRequests();
        verify(mDppMetrics).updateDppFailure(eq(EasyConnectStatusCallback
                .EASY_CONNECT_EVENT_FAILURE_AUTHENTICATION));
        verify(mDppMetrics).updateDppOperationTime(anyInt());
        verifyNoMoreInteractions(mDppMetrics);
        verifyCleanUpResources();
    }

    @Test
    public void testDppStopSessionNotStarted() throws Exception {
        // Check that nothing happens if no DPP session started.
        mDppManager.stopDppSession(0);

        // Check that nothing is removed or canceled
        mDppManager.stopDppSession(0);
        verify(mWifiNative, never()).removeDppUri(eq(TEST_INTERFACE_NAME), anyInt());
        verify(mWakeupMessage, never()).cancel();
    }

    @Test
    public void testDppStopSessionIncorrectUid() throws Exception {
        // Check that nothing happens if UID is incorrect
        // Add Peer URI successful, return peer ID 1
        when(mWifiNative.addDppPeerUri(anyString(), anyString()))
                .thenReturn(1);
        when(mWifiNative.removeDppUri(anyString(), anyInt()))
                .thenReturn(true);

        // Return test interface name
        when(mWifiNative.getClientInterfaceName()).thenReturn(TEST_INTERFACE_NAME);

        // Successful start
        when(mWifiNative.startDppEnrolleeInitiator(anyString(), anyInt(), anyInt())).thenReturn(
                true);

        ArgumentCaptor<WifiNative.DppEventCallback> dppEventCallbackCaptor =
                ArgumentCaptor.forClass(
                        WifiNative.DppEventCallback.class);

        // Start with UID 10
        mDppManager.startDppAsEnrolleeInitiator(10, mBinder, mUri, mDppCallback);
        verify(mWifiNative).registerDppEventCallback(dppEventCallbackCaptor.capture());
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mWifiNative).startDppEnrolleeInitiator(eq(TEST_INTERFACE_NAME),
                eq(TEST_PEER_ID), anyInt());

        // Check that nothing is removed or canceled
        mDppManager.stopDppSession(0);
        verify(mWifiNative, never()).removeDppUri(eq(TEST_INTERFACE_NAME), anyInt());
        verify(mWakeupMessage, never()).cancel();
    }

    @Test
    public void testDppStopSession() throws Exception {
        // Check that nothing happens if UID is incorrect
        // Add Peer URI successful, return peer ID 1
        when(mWifiNative.addDppPeerUri(anyString(), anyString()))
                .thenReturn(1);
        when(mWifiNative.removeDppUri(anyString(), anyInt()))
                .thenReturn(true);

        // Return test interface name
        when(mWifiNative.getClientInterfaceName()).thenReturn(TEST_INTERFACE_NAME);

        // Successful start
        when(mWifiNative.startDppEnrolleeInitiator(anyString(), anyInt(), anyInt())).thenReturn(
                true);

        ArgumentCaptor<WifiNative.DppEventCallback> dppEventCallbackCaptor =
                ArgumentCaptor.forClass(
                        WifiNative.DppEventCallback.class);

        // Start with UID 10
        mDppManager.startDppAsEnrolleeInitiator(10, mBinder, mUri, mDppCallback);
        verify(mWifiNative).registerDppEventCallback(dppEventCallbackCaptor.capture());
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mWifiNative).startDppEnrolleeInitiator(eq(TEST_INTERFACE_NAME),
                eq(TEST_PEER_ID), anyInt());

        // Check that WifiNative is called to stop the DPP session
        mDppManager.stopDppSession(10);
        verify(mWifiNative).stopDppInitiator(eq(TEST_INTERFACE_NAME));
        verifyCleanUpResources();
    }

    private void verifyCleanUpResources() {
        verify(mWifiNative).removeDppUri(eq(TEST_INTERFACE_NAME), anyInt());
        verify(mWakeupMessage).cancel();
    }

    @Test
    public void testOnFailureCallbackNotCompatible() throws Exception {
        testOnFailureCallback(NOT_COMPATIBLE, EASY_CONNECT_EVENT_FAILURE_NOT_COMPATIBLE);
    }

    @Test
    public void testOnFailureCallbackConfiguration() throws Exception {
        testOnFailureCallback(CONFIGURATION, EASY_CONNECT_EVENT_FAILURE_CONFIGURATION);
    }

    @Test
    public void testOnFailureCallbackTimeout() throws Exception {
        testOnFailureCallback(TIMEOUT, EASY_CONNECT_EVENT_FAILURE_TIMEOUT);
    }

    @Test
    public void testOnFailureCallbackNotSupported() throws Exception {
        testOnFailureCallback(NOT_SUPPORTED, EASY_CONNECT_EVENT_FAILURE_NOT_SUPPORTED);
    }

    @Test
    public void testOnFailureCallbackBusy() throws Exception {
        testOnFailureCallback(BUSY, EASY_CONNECT_EVENT_FAILURE_BUSY);
    }

    @Test
    public void testOnFailureCallbackFailure() throws Exception {
        testOnFailureCallback(FAILURE, EASY_CONNECT_EVENT_FAILURE_GENERIC);
    }

    /**
     * Helper function for setting up a scan result.
     *
     * @param frequency Network's operating channel in Mhz.
     *
     */
    private void addTestNetworkInScanResult(int frequency) {
        String caps = "[WPA2-FT/SAE+SAE][ESS][WPS]";
        ScanResult scanResult = new ScanResult(WifiSsid.createFromAsciiEncoded(TEST_SSID_NO_QUOTE),
                TEST_SSID_NO_QUOTE, TEST_BSSID, 1245, 0, caps, -78, frequency,
                1025, 22, 33, 20, 0, 0, true);
        List<ScanResult> scanResults = new ArrayList<>();
        scanResults.add(scanResult);
        when(mScanRequestProxy.getScanResults()).thenReturn(scanResults);
    }

    @Test
    public void testOnFailureCallbackCannotFindNetworkNoErrCodeChangeOnChannelMatch()
            throws Exception {
        int[] bandList = new int[]{81, 83, 84, 115};
        addTestNetworkInScanResult(2437); //channel number 6
        // Include Network channel(6) in Enrollee scanned channels.
        testOnFailureCallback(CANNOT_FIND_NETWORK, TEST_SSID_NO_QUOTE,
                "81/1,2,3,4,5,6,7,8,9,10,11,115/48",
                bandList, EASY_CONNECT_EVENT_FAILURE_CANNOT_FIND_NETWORK);
    }

    @Test
    public void testOnFailureCallbackCannotFindNetworkErrCodeIsUpdatedOnChannelMismatch()
            throws Exception {
        int[] bandList = new int[]{81, 83, 84, 115};
        addTestNetworkInScanResult(5180); //channel number 36
        // Don't include Network channel(36) in Enrollee scanned channels.
        testOnFailureCallback(CANNOT_FIND_NETWORK, TEST_SSID_NO_QUOTE,
                "81/1,2,3,4,5,6,7,8,9,10,11,115/48",
                bandList, EASY_CONNECT_EVENT_FAILURE_NOT_COMPATIBLE);
    }

    @Test
    public void testOnFailureCallbackCannotFindNetworkNoErrCodeChangeOnInvalidScannedChannelList()
            throws Exception {
        int[] bandList = new int[]{81, 83, 84};
        addTestNetworkInScanResult(5180); //channel number 36
        // Send Invalid operating class/channel list
        testOnFailureCallback(CANNOT_FIND_NETWORK, TEST_SSID_NO_QUOTE,
                "1035/1,2,3,4,5,6,7,8,9,10,11,25/45",
                bandList, EASY_CONNECT_EVENT_FAILURE_CANNOT_FIND_NETWORK);
    }

    @Test
    public void testOnFailureCallbackCannotFindNetwork() throws Exception {
        int[] bandList = new int[]{1, 6, 11};
        testOnFailureCallback(CANNOT_FIND_NETWORK, TEST_SSID_NO_QUOTE, "1, 3, 6, 8, 11",
                bandList, EASY_CONNECT_EVENT_FAILURE_CANNOT_FIND_NETWORK);
    }

    @Test
    public void testOnFailureCallbackConfigurationRejected() throws Exception {
        testOnFailureCallback(CONFIGURATION_REJECTED,
                EASY_CONNECT_EVENT_FAILURE_ENROLLEE_REJECTED_CONFIGURATION);
    }

    @Test
    public void testOnFailureCallbackEnrolleeAuthentication() throws Exception {
        int[] bandList = new int[]{1, 6, 11};
        testOnFailureCallback(ENROLLEE_AUTHENTICATION, new String(TEST_SSID_NO_QUOTE), null,
                bandList, EASY_CONNECT_EVENT_FAILURE_ENROLLEE_AUTHENTICATION);
    }

    @Test
    public void testStartDppAsConfiguratorInitiatorStartOnSuccessCallbackR2() throws Exception {
        // Generate a mock WifiConfiguration object
        WifiConfiguration selectedNetwork = new WifiConfiguration();
        selectedNetwork.SSID = TEST_SSID;
        selectedNetwork.networkId = 1;
        selectedNetwork.preSharedKey = TEST_PASSWORD;
        selectedNetwork.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.SAE);

        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(
                selectedNetwork);

        ArgumentCaptor<WifiNative.DppEventCallback> dppEventCallbackCaptor =
                ArgumentCaptor.forClass(
                        WifiNative.DppEventCallback.class);

        mDppManager.startDppAsConfiguratorInitiator(0, mBinder, mUri, 1,
                EASY_CONNECT_NETWORK_ROLE_STA, mDppCallback);
        verify(mWifiNative).registerDppEventCallback(dppEventCallbackCaptor.capture());
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mWifiNative).startDppConfiguratorInitiator(eq(TEST_INTERFACE_NAME),
                eq(TEST_PEER_ID), anyInt(), eq(TEST_SSID_ENCODED), eq(TEST_PASSWORD_ENCODED), any(),
                eq(EASY_CONNECT_NETWORK_ROLE_STA), eq(
                        SAE));

        WifiNative.DppEventCallback dppEventCallback = dppEventCallbackCaptor.getValue();

        // Generate a progress events
        dppEventCallback.onProgress(AUTHENTICATION_SUCCESS);
        mLooper.dispatchAll();
        verify(mDppCallback).onProgress(eq(EASY_CONNECT_EVENT_PROGRESS_AUTHENTICATION_SUCCESS));

        dppEventCallback.onProgress(CONFIGURATION_ACCEPTED);
        mLooper.dispatchAll();
        verify(mDppCallback).onProgress(eq(EASY_CONNECT_EVENT_PROGRESS_CONFIGURATION_ACCEPTED));

        dppEventCallback.onProgress(CONFIGURATION_SENT_WAITING_RESPONSE);
        mLooper.dispatchAll();
        verify(mDppCallback)
                .onProgress(eq(EASY_CONNECT_EVENT_PROGRESS_CONFIGURATION_SENT_WAITING_RESPONSE));
        verify(mDppMetrics, times(1)).updateDppR2CapableEnrolleeResponderDevices();

        // Generate an onSuccess callback
        dppEventCallback.onSuccess(CONFIGURATION_APPLIED);
        mLooper.dispatchAll();
        verify(mDppCallback).onSuccess(eq(EASY_CONNECT_EVENT_SUCCESS_CONFIGURATION_APPLIED));
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppMetrics).updateDppConfiguratorInitiatorRequests();
        verify(mDppMetrics).updateDppConfiguratorSuccess(eq(EasyConnectStatusCallback
                .EASY_CONNECT_EVENT_SUCCESS_CONFIGURATION_APPLIED));
        verify(mDppMetrics).updateDppOperationTime(anyInt());
        verifyNoMoreInteractions(mDppMetrics);
    }

    private void testOnFailureCallback(int internalFailure, String ssid, String channelList,
            int[] bandList, int appFailure) throws Exception {
        // Generate a mock WifiConfiguration object
        WifiConfiguration selectedNetwork = new WifiConfiguration();
        selectedNetwork.SSID = TEST_SSID;
        selectedNetwork.networkId = TEST_NETWORK_ID;
        selectedNetwork.preSharedKey = TEST_PASSWORD;
        selectedNetwork.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.SAE);

        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(
                selectedNetwork);
        ArgumentCaptor<WifiNative.DppEventCallback> dppEventCallbackCaptor =
                ArgumentCaptor.forClass(
                        WifiNative.DppEventCallback.class);

        mDppManager.startDppAsConfiguratorInitiator(0, mBinder, mUri, 1,
                EASY_CONNECT_NETWORK_ROLE_STA, mDppCallback);
        verify(mWifiNative).registerDppEventCallback(dppEventCallbackCaptor.capture());
        verify(mDppCallback, never()).onFailure(anyInt(), anyString(), anyString(), any());
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mWifiNative).startDppConfiguratorInitiator(eq(TEST_INTERFACE_NAME),
                eq(TEST_PEER_ID), anyInt(), eq(TEST_SSID_ENCODED), eq(TEST_PASSWORD_ENCODED), any(),
                eq(EASY_CONNECT_NETWORK_ROLE_STA), eq(SAE));

        WifiNative.DppEventCallback dppEventCallback = dppEventCallbackCaptor.getValue();

        // Generate an onFailure callback
        dppEventCallback.onFailure(internalFailure, ssid, channelList, bandList);
        mLooper.dispatchAll();
        verify(mDppCallback).onFailure(eq(appFailure), eq(ssid), eq(channelList),
                eq(bandList));
        verify(mDppCallback, never()).onSuccess(anyInt());
        verify(mDppCallback, never()).onSuccessConfigReceived(anyInt());
        verify(mDppMetrics).updateDppConfiguratorInitiatorRequests();
        verify(mDppMetrics).updateDppFailure(eq(appFailure));
        verify(mDppMetrics).updateDppOperationTime(anyInt());
        if ((internalFailure == CANNOT_FIND_NETWORK)
                && (appFailure == EASY_CONNECT_EVENT_FAILURE_NOT_COMPATIBLE)) {
            verify(mDppMetrics, times(1)).updateDppR2EnrolleeResponderIncompatibleConfiguration();
        }
        verifyNoMoreInteractions(mDppMetrics);
        verifyCleanUpResources();
    }

    private void testOnFailureCallback(int internalFailure, int appFailure) throws Exception {
        testOnFailureCallback(internalFailure, null, null, new int[0], appFailure);
    }
}
