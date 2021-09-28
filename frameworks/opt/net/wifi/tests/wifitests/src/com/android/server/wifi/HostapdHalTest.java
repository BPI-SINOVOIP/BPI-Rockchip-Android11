/*
 * Copyright (C) 2017 The Android Open Source Project
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

import static org.junit.Assert.*;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyLong;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.*;

import android.app.test.MockAnswerUtil;
import android.content.Context;
import android.hardware.wifi.hostapd.V1_0.HostapdStatus;
import android.hardware.wifi.hostapd.V1_0.HostapdStatusCode;
import android.hardware.wifi.hostapd.V1_0.IHostapd;
import android.hardware.wifi.hostapd.V1_1.IHostapdCallback;
import android.hardware.wifi.hostapd.V1_2.DebugLevel;
import android.hidl.manager.V1_0.IServiceManager;
import android.hidl.manager.V1_0.IServiceNotification;
import android.net.MacAddress;
import android.net.wifi.SoftApConfiguration;
import android.net.wifi.SoftApConfiguration.Builder;
import android.net.wifi.WifiManager;
import android.net.wifi.nl80211.WifiNl80211Manager;
import android.os.Handler;
import android.os.IHwBinder;
import android.os.RemoteException;
import android.os.test.TestLooper;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.util.NativeUtil;
import com.android.wifi.resources.R;

import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;

/**
 * Unit tests for HostapdHal
 */
@SmallTest
public class HostapdHalTest extends WifiBaseTest {
    private static final String IFACE_NAME = "mock-wlan0";
    private static final String NETWORK_SSID = "test-ssid";
    private static final String NETWORK_PSK = "test-psk";

    private @Mock Context mContext;
    private @Mock IServiceManager mServiceManagerMock;
    private @Mock IHostapd mIHostapdMock;
    private @Mock WifiNative.HostapdDeathEventHandler mHostapdHalDeathHandler;
    private @Mock WifiNl80211Manager.SoftApCallback mSoftApListener;
    private android.hardware.wifi.hostapd.V1_1.IHostapd mIHostapdMockV11;
    private android.hardware.wifi.hostapd.V1_2.IHostapd mIHostapdMockV12;
    private IHostapdCallback mIHostapdCallback;
    private MockResources mResources;
    HostapdStatus mStatusSuccess;
    HostapdStatus mStatusFailure;
    android.hardware.wifi.hostapd.V1_2.HostapdStatus mStatusSuccess12;
    android.hardware.wifi.hostapd.V1_2.HostapdStatus mStatusFailure12;
    private TestLooper mLooper = new TestLooper();
    private HostapdHal mHostapdHal;
    private ArgumentCaptor<IHwBinder.DeathRecipient> mServiceManagerDeathCaptor =
            ArgumentCaptor.forClass(IHwBinder.DeathRecipient.class);
    private ArgumentCaptor<IHwBinder.DeathRecipient> mHostapdDeathCaptor =
            ArgumentCaptor.forClass(IHwBinder.DeathRecipient.class);
    private ArgumentCaptor<IServiceNotification.Stub> mServiceNotificationCaptor =
            ArgumentCaptor.forClass(IServiceNotification.Stub.class);
    private ArgumentCaptor<IHostapd.IfaceParams> mIfaceParamsCaptor =
            ArgumentCaptor.forClass(IHostapd.IfaceParams.class);
    private ArgumentCaptor<android.hardware.wifi.hostapd.V1_1.IHostapd.IfaceParams>
            mIfaceParamsCaptorV1_1 =
            ArgumentCaptor.forClass(android.hardware.wifi.hostapd.V1_1.IHostapd.IfaceParams.class);
    private ArgumentCaptor<android.hardware.wifi.hostapd.V1_2.IHostapd.IfaceParams>
            mIfaceParamsCaptorV12 =
            ArgumentCaptor.forClass(android.hardware.wifi.hostapd.V1_2.IHostapd.IfaceParams.class);
    private ArgumentCaptor<IHostapd.NetworkParams> mNetworkParamsCaptor =
            ArgumentCaptor.forClass(IHostapd.NetworkParams.class);
    private ArgumentCaptor<android.hardware.wifi.hostapd.V1_2.IHostapd.NetworkParams>
            mNetworkParamsV12Captor = ArgumentCaptor.forClass(
            android.hardware.wifi.hostapd.V1_2.IHostapd.NetworkParams.class);
    private ArgumentCaptor<Long> mDeathRecipientCookieCaptor = ArgumentCaptor.forClass(Long.class);
    private InOrder mInOrder;

    private class HostapdHalSpy extends HostapdHal {
        HostapdHalSpy() {
            super(mContext, new Handler(mLooper.getLooper()));
        }

        @Override
        protected IServiceManager getServiceManagerMockable() throws RemoteException {
            return mServiceManagerMock;
        }

        @Override
        protected IHostapd getHostapdMockable() throws RemoteException {
            return mIHostapdMock;
        }

        @Override
        protected android.hardware.wifi.hostapd.V1_1.IHostapd getHostapdMockableV1_1()
                throws RemoteException {
            return mIHostapdMockV11;
        }

        @Override
        protected android.hardware.wifi.hostapd.V1_2.IHostapd getHostapdMockableV1_2()
                throws RemoteException {
            return mIHostapdMockV12;
        }
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mResources = new MockResources();
        mResources.setBoolean(R.bool.config_wifi_softap_acs_supported, false);
        mResources.setBoolean(R.bool.config_wifi_softap_ieee80211ac_supported, false);
        mResources.setBoolean(R.bool.config_wifiSoftapIeee80211axSupported, false);
        mResources.setBoolean(R.bool.config_wifiSoftap6ghzSupported, false);
        mResources.setBoolean(R.bool.config_wifiSoftapAcsIncludeDfs, false);
        mResources.setString(R.string.config_wifiSoftap2gChannelList, "");
        mResources.setString(R.string.config_wifiSoftap5gChannelList, "");
        mResources.setString(R.string.config_wifiSoftap6gChannelList, "");

        mStatusSuccess = createHostapdStatus(HostapdStatusCode.SUCCESS);
        mStatusFailure = createHostapdStatus(HostapdStatusCode.FAILURE_UNKNOWN);


        mStatusSuccess12 = createHostapdStatus_1_2(HostapdStatusCode.SUCCESS);
        mStatusFailure12 = createHostapdStatus_1_2(HostapdStatusCode.FAILURE_UNKNOWN);

        when(mContext.getResources()).thenReturn(mResources);
        when(mServiceManagerMock.linkToDeath(any(IHwBinder.DeathRecipient.class),
                anyLong())).thenReturn(true);
        when(mServiceManagerMock.registerForNotifications(anyString(), anyString(),
                any(IServiceNotification.Stub.class))).thenReturn(true);
        when(mServiceManagerMock.getTransport(anyString(), anyString()))
                .thenReturn(IServiceManager.Transport.EMPTY);
        when(mIHostapdMock.linkToDeath(any(IHwBinder.DeathRecipient.class),
                anyLong())).thenReturn(true);
        when(mIHostapdMock.linkToDeath(any(IHwBinder.DeathRecipient.class),
                anyLong())).thenReturn(true);
        when(mIHostapdMock.addAccessPoint(
                mIfaceParamsCaptor.capture(), mNetworkParamsCaptor.capture()))
                .thenReturn(mStatusSuccess);
        when(mIHostapdMock.removeAccessPoint(any())).thenReturn(mStatusSuccess);
        mHostapdHal = new HostapdHalSpy();
    }

    /**
     * Sunny day scenario for HostapdHal initialization
     * Asserts successful initialization
     */
    @Test
    public void testInitialize_success() throws Exception {
        executeAndValidateInitializationSequence(false, false);
    }

    /**
     * Failure scenario for HostapdHal initialization
     */
    @Test
    public void testInitialize_registerException() throws Exception {
        executeAndValidateInitializationSequence(true, false);
    }

    /**
     * Failure scenario for HostapdHal initialization
     */
    @Test
    public void testInitialize_registerFailure() throws Exception {
        executeAndValidateInitializationSequence(false, true);
    }

    /**
     * Sunny day scenario for V1.1 HostapdHal initialization
     * Asserts successful initialization
     */
    @Test
    public void testInitialize_successV1_1() throws Exception {
        when(mServiceManagerMock.getTransport(eq(
                android.hardware.wifi.hostapd.V1_1.IHostapd.kInterfaceName), anyString()))
                .thenReturn(IServiceManager.Transport.HWBINDER);
        mIHostapdMockV11 = mock(android.hardware.wifi.hostapd.V1_1.IHostapd.class);
        executeAndValidateInitializationSequenceV1_1(false);
    }

    /**
     * Failure scenario for V1.1 HostapdHal initialization
     */
    @Test
    public void testInitialize_registerCallbackFailureV1_1() throws Exception {
        when(mServiceManagerMock.getTransport(eq(
                android.hardware.wifi.hostapd.V1_1.IHostapd.kInterfaceName), anyString()))
                .thenReturn(IServiceManager.Transport.HWBINDER);
        mIHostapdMockV11 = mock(android.hardware.wifi.hostapd.V1_1.IHostapd.class);
        executeAndValidateInitializationSequenceV1_1(true);
    }

    /**
     * Verifies the hostapd death handling.
     */
    @Test
    public void testDeathHandling() throws Exception {
        executeAndValidateInitializationSequence();

        mHostapdHal.registerDeathHandler(mHostapdHalDeathHandler);
        mHostapdDeathCaptor.getValue().serviceDied(mDeathRecipientCookieCaptor.getValue());
        mLooper.dispatchAll();
        verify(mHostapdHalDeathHandler).onDeath();
    }

    /**
     * Verifies the hostapd death handling.
     */
    @Test
    public void testStaleDeathHandling() throws Exception {
        executeAndValidateInitializationSequence();

        mHostapdHal.registerDeathHandler(mHostapdHalDeathHandler);
        mHostapdDeathCaptor.getValue().serviceDied(mDeathRecipientCookieCaptor.getValue() - 1);
        mLooper.dispatchAll();
        verify(mHostapdHalDeathHandler, never()).onDeath();
    }

    /**
     * Verifies the successful addition of access point.
     */
    @Test
    public void testAddAccessPointSuccess_Psk_Band2G() throws Exception {
        executeAndValidateInitializationSequence();
        final int apChannel = 6;

        Builder configurationBuilder = new SoftApConfiguration.Builder();
        configurationBuilder.setSsid(NETWORK_SSID);
        configurationBuilder.setHiddenSsid(false);
        configurationBuilder.setPassphrase(NETWORK_PSK,
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        configurationBuilder.setChannel(apChannel, SoftApConfiguration.BAND_2GHZ);

        assertTrue(mHostapdHal.addAccessPoint(IFACE_NAME,
                configurationBuilder.build(),
                () -> mSoftApListener.onFailure()));
        verify(mIHostapdMock).addAccessPoint(any(), any());

        assertEquals(IFACE_NAME, mIfaceParamsCaptor.getValue().ifaceName);
        assertTrue(mIfaceParamsCaptor.getValue().hwModeParams.enable80211N);
        assertFalse(mIfaceParamsCaptor.getValue().hwModeParams.enable80211AC);
        assertEquals(IHostapd.Band.BAND_2_4_GHZ, mIfaceParamsCaptor.getValue().channelParams.band);
        assertFalse(mIfaceParamsCaptor.getValue().channelParams.enableAcs);
        assertFalse(mIfaceParamsCaptor.getValue().channelParams.acsShouldExcludeDfs);
        assertEquals(apChannel, mIfaceParamsCaptor.getValue().channelParams.channel);

        assertEquals(NativeUtil.stringToByteArrayList(NETWORK_SSID),
                mNetworkParamsCaptor.getValue().ssid);
        assertFalse(mNetworkParamsCaptor.getValue().isHidden);
        assertEquals(IHostapd.EncryptionType.WPA2, mNetworkParamsCaptor.getValue().encryptionType);
        assertEquals(NETWORK_PSK, mNetworkParamsCaptor.getValue().pskPassphrase);
    }

    /**
     * Verifies the successful addition of access point.
     */
    @Test
    public void testAddAccessPointSuccess_Open_Band5G() throws Exception {
        executeAndValidateInitializationSequence();
        final int apChannel = 149;

        Builder configurationBuilder = new SoftApConfiguration.Builder();
        configurationBuilder.setSsid(NETWORK_SSID);
        configurationBuilder.setHiddenSsid(true);
        configurationBuilder.setChannel(apChannel, SoftApConfiguration.BAND_5GHZ);

        assertTrue(mHostapdHal.addAccessPoint(IFACE_NAME,
                configurationBuilder.build(),
                () -> mSoftApListener.onFailure()));
        verify(mIHostapdMock).addAccessPoint(any(), any());

        assertEquals(IFACE_NAME, mIfaceParamsCaptor.getValue().ifaceName);
        assertTrue(mIfaceParamsCaptor.getValue().hwModeParams.enable80211N);
        assertFalse(mIfaceParamsCaptor.getValue().hwModeParams.enable80211AC);
        assertEquals(IHostapd.Band.BAND_5_GHZ, mIfaceParamsCaptor.getValue().channelParams.band);
        assertFalse(mIfaceParamsCaptor.getValue().channelParams.enableAcs);
        assertFalse(mIfaceParamsCaptor.getValue().channelParams.acsShouldExcludeDfs);
        assertEquals(apChannel, mIfaceParamsCaptor.getValue().channelParams.channel);

        assertEquals(NativeUtil.stringToByteArrayList(NETWORK_SSID),
                mNetworkParamsCaptor.getValue().ssid);
        assertTrue(mNetworkParamsCaptor.getValue().isHidden);
        assertEquals(IHostapd.EncryptionType.NONE, mNetworkParamsCaptor.getValue().encryptionType);
        assertNotNull(mNetworkParamsCaptor.getValue().pskPassphrase);
    }

    /**
     * Verifies the successful addition of access point.
     */
    @Test
    public void testAddAccessPointSuccess_Psk_Band5G_Hidden() throws Exception {
        executeAndValidateInitializationSequence();
        final int apChannel = 149;

        Builder configurationBuilder = new SoftApConfiguration.Builder();
        configurationBuilder.setSsid(NETWORK_SSID);
        configurationBuilder.setHiddenSsid(true);
        configurationBuilder.setPassphrase(NETWORK_PSK,
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        configurationBuilder.setChannel(apChannel, SoftApConfiguration.BAND_5GHZ);

        assertTrue(mHostapdHal.addAccessPoint(IFACE_NAME,
                configurationBuilder.build(),
                () -> mSoftApListener.onFailure()));
        verify(mIHostapdMock).addAccessPoint(any(), any());

        assertEquals(IFACE_NAME, mIfaceParamsCaptor.getValue().ifaceName);
        assertTrue(mIfaceParamsCaptor.getValue().hwModeParams.enable80211N);
        assertFalse(mIfaceParamsCaptor.getValue().hwModeParams.enable80211AC);
        assertEquals(IHostapd.Band.BAND_5_GHZ, mIfaceParamsCaptor.getValue().channelParams.band);
        assertFalse(mIfaceParamsCaptor.getValue().channelParams.enableAcs);
        assertFalse(mIfaceParamsCaptor.getValue().channelParams.acsShouldExcludeDfs);
        assertEquals(apChannel, mIfaceParamsCaptor.getValue().channelParams.channel);

        assertEquals(NativeUtil.stringToByteArrayList(NETWORK_SSID),
                mNetworkParamsCaptor.getValue().ssid);
        assertTrue(mNetworkParamsCaptor.getValue().isHidden);
        assertEquals(IHostapd.EncryptionType.WPA2, mNetworkParamsCaptor.getValue().encryptionType);
        assertEquals(NETWORK_PSK, mNetworkParamsCaptor.getValue().pskPassphrase);
    }

    /**
     * Verifies the successful addition of access point.
     */
    @Test
    public void testAddAccessPointSuccess_Psk_Band2G_WithACS() throws Exception {
        // Enable ACS in the config.
        mResources.setBoolean(R.bool.config_wifi_softap_acs_supported, true);
        mHostapdHal = new HostapdHalSpy();

        executeAndValidateInitializationSequence();
        final int apChannel = 6;

        Builder configurationBuilder = new SoftApConfiguration.Builder();
        configurationBuilder.setSsid(NETWORK_SSID);
        configurationBuilder.setHiddenSsid(false);
        configurationBuilder.setPassphrase(NETWORK_PSK,
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        configurationBuilder.setChannel(apChannel, SoftApConfiguration.BAND_2GHZ);

        assertTrue(mHostapdHal.addAccessPoint(IFACE_NAME,
                configurationBuilder.build(),
                () -> mSoftApListener.onFailure()));
        verify(mIHostapdMock).addAccessPoint(any(), any());

        assertEquals(IFACE_NAME, mIfaceParamsCaptor.getValue().ifaceName);
        assertTrue(mIfaceParamsCaptor.getValue().hwModeParams.enable80211N);
        assertFalse(mIfaceParamsCaptor.getValue().hwModeParams.enable80211AC);
        assertEquals(IHostapd.Band.BAND_2_4_GHZ, mIfaceParamsCaptor.getValue().channelParams.band);
        assertEquals(apChannel, mIfaceParamsCaptor.getValue().channelParams.channel);
        assertFalse(mIfaceParamsCaptor.getValue().channelParams.enableAcs);
        assertFalse(mIfaceParamsCaptor.getValue().channelParams.acsShouldExcludeDfs);

        assertEquals(NativeUtil.stringToByteArrayList(NETWORK_SSID),
                mNetworkParamsCaptor.getValue().ssid);
        assertFalse(mNetworkParamsCaptor.getValue().isHidden);
        assertEquals(IHostapd.EncryptionType.WPA2, mNetworkParamsCaptor.getValue().encryptionType);
        assertEquals(NETWORK_PSK, mNetworkParamsCaptor.getValue().pskPassphrase);
    }

    /**
     * Verifies the successful addition of access point.
     */
    @Test
    public void testAddAccessPointSuccess_Psk_Band2G_WithIeee80211AC() throws Exception {
        // Enable ACS & 80211AC in the config.
        mResources.setBoolean(R.bool.config_wifi_softap_acs_supported, true);
        mResources.setBoolean(R.bool.config_wifi_softap_ieee80211ac_supported, true);
        mHostapdHal = new HostapdHalSpy();

        executeAndValidateInitializationSequence();
        final int apChannel = 6;

        Builder configurationBuilder = new SoftApConfiguration.Builder();
        configurationBuilder.setSsid(NETWORK_SSID);
        configurationBuilder.setHiddenSsid(false);
        configurationBuilder.setPassphrase(NETWORK_PSK,
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        configurationBuilder.setChannel(apChannel, SoftApConfiguration.BAND_2GHZ);

        assertTrue(mHostapdHal.addAccessPoint(IFACE_NAME,
                configurationBuilder.build(),
                () -> mSoftApListener.onFailure()));
        verify(mIHostapdMock).addAccessPoint(any(), any());

        assertEquals(IFACE_NAME, mIfaceParamsCaptor.getValue().ifaceName);
        assertTrue(mIfaceParamsCaptor.getValue().hwModeParams.enable80211N);
        assertTrue(mIfaceParamsCaptor.getValue().hwModeParams.enable80211AC);
        assertEquals(IHostapd.Band.BAND_2_4_GHZ, mIfaceParamsCaptor.getValue().channelParams.band);
        assertEquals(apChannel, mIfaceParamsCaptor.getValue().channelParams.channel);
        assertFalse(mIfaceParamsCaptor.getValue().channelParams.enableAcs);
        assertFalse(mIfaceParamsCaptor.getValue().channelParams.acsShouldExcludeDfs);

        assertEquals(NativeUtil.stringToByteArrayList(NETWORK_SSID),
                mNetworkParamsCaptor.getValue().ssid);
        assertFalse(mNetworkParamsCaptor.getValue().isHidden);
        assertEquals(IHostapd.EncryptionType.WPA2, mNetworkParamsCaptor.getValue().encryptionType);
        assertEquals(NETWORK_PSK, mNetworkParamsCaptor.getValue().pskPassphrase);
    }

    /**
     * Verifies the successful addition of access point.
     */
    @Test
    public void testAddAccessPointSuccess_Psk_BandAny_WithACS() throws Exception {
        // Enable ACS in the config.
        mResources.setBoolean(R.bool.config_wifi_softap_acs_supported, true);
        mHostapdHal = new HostapdHalSpy();

        executeAndValidateInitializationSequence();

        Builder configurationBuilder = new SoftApConfiguration.Builder();
        configurationBuilder.setSsid(NETWORK_SSID);
        configurationBuilder.setHiddenSsid(false);
        configurationBuilder.setPassphrase(NETWORK_PSK,
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        configurationBuilder.setBand(SoftApConfiguration.BAND_ANY);

        assertTrue(mHostapdHal.addAccessPoint(IFACE_NAME,
                configurationBuilder.build(),
                () -> mSoftApListener.onFailure()));
        verify(mIHostapdMock).addAccessPoint(any(), any());

        assertEquals(IFACE_NAME, mIfaceParamsCaptor.getValue().ifaceName);
        assertTrue(mIfaceParamsCaptor.getValue().hwModeParams.enable80211N);
        assertFalse(mIfaceParamsCaptor.getValue().hwModeParams.enable80211AC);
        assertEquals(IHostapd.Band.BAND_ANY, mIfaceParamsCaptor.getValue().channelParams.band);
        assertTrue(mIfaceParamsCaptor.getValue().channelParams.enableAcs);
        assertTrue(mIfaceParamsCaptor.getValue().channelParams.acsShouldExcludeDfs);

        assertEquals(NativeUtil.stringToByteArrayList(NETWORK_SSID),
                mNetworkParamsCaptor.getValue().ssid);
        assertFalse(mNetworkParamsCaptor.getValue().isHidden);
        assertEquals(IHostapd.EncryptionType.WPA2, mNetworkParamsCaptor.getValue().encryptionType);
        assertEquals(NETWORK_PSK, mNetworkParamsCaptor.getValue().pskPassphrase);
    }

    /**
     * Verifies the successful addition of access point.
     */
    @Test
    public void testAddAccessPointSuccess_Psk_WithoutACS() throws Exception {
        // Disable ACS in the config.
        mResources.setBoolean(R.bool.config_wifi_softap_acs_supported, false);
        mHostapdHal = new HostapdHalSpy();

        executeAndValidateInitializationSequence();

        Builder configurationBuilder = new SoftApConfiguration.Builder();
        configurationBuilder.setSsid(NETWORK_SSID);
        configurationBuilder.setHiddenSsid(false);
        configurationBuilder.setPassphrase(NETWORK_PSK,
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        configurationBuilder.setBand(SoftApConfiguration.BAND_ANY);

        assertTrue(mHostapdHal.addAccessPoint(IFACE_NAME,
                configurationBuilder.build(),
                () -> mSoftApListener.onFailure()));
        verify(mIHostapdMock).addAccessPoint(any(), any());

        assertEquals(IFACE_NAME, mIfaceParamsCaptor.getValue().ifaceName);
        assertTrue(mIfaceParamsCaptor.getValue().hwModeParams.enable80211N);
        assertFalse(mIfaceParamsCaptor.getValue().hwModeParams.enable80211AC);
        assertFalse(mIfaceParamsCaptor.getValue().channelParams.enableAcs);

        assertEquals(NativeUtil.stringToByteArrayList(NETWORK_SSID),
                mNetworkParamsCaptor.getValue().ssid);
        assertFalse(mNetworkParamsCaptor.getValue().isHidden);
        assertEquals(IHostapd.EncryptionType.WPA2, mNetworkParamsCaptor.getValue().encryptionType);
        assertEquals(NETWORK_PSK, mNetworkParamsCaptor.getValue().pskPassphrase);
    }

    /**
     * Verifies the successful addition of access point.
     * Verifies that channel info for ACS is handled.
     */
    @Test
    public void testAddAccessPointSuccess_Psk_BandAny_WithACS_AcsChannels() throws Exception {
        when(mServiceManagerMock.getTransport(eq(
                android.hardware.wifi.hostapd.V1_1.IHostapd.kInterfaceName), anyString()))
                .thenReturn(IServiceManager.Transport.HWBINDER);
        mIHostapdMockV11 = mock(android.hardware.wifi.hostapd.V1_1.IHostapd.class);
        // Enable ACS and set available channels in the config.
        final String acsChannelStr2g = "1,6,11-13";
        final String acsChannelStr5g = "40";
        final String acsChannelStr6g = "";
        android.hardware.wifi.hostapd.V1_1.IHostapd.AcsChannelRange channelRange1 =
                new android.hardware.wifi.hostapd.V1_1.IHostapd.AcsChannelRange();
        android.hardware.wifi.hostapd.V1_1.IHostapd.AcsChannelRange channelRange2 =
                new android.hardware.wifi.hostapd.V1_1.IHostapd.AcsChannelRange();
        android.hardware.wifi.hostapd.V1_1.IHostapd.AcsChannelRange channelRange3 =
                new android.hardware.wifi.hostapd.V1_1.IHostapd.AcsChannelRange();
        android.hardware.wifi.hostapd.V1_1.IHostapd.AcsChannelRange channelRange4 =
                new android.hardware.wifi.hostapd.V1_1.IHostapd.AcsChannelRange();
        channelRange1.start = channelRange1.end = 1;
        channelRange2.start = channelRange2.end = 6;
        channelRange3.start = 11;
        channelRange3.end = 13;
        channelRange4.start = channelRange4.end = 40;
        ArrayList<android.hardware.wifi.hostapd.V1_1.IHostapd.AcsChannelRange> acsChannelRanges =
                new ArrayList<>();
        acsChannelRanges.add(channelRange1);
        acsChannelRanges.add(channelRange2);
        acsChannelRanges.add(channelRange3);
        acsChannelRanges.add(channelRange4);
        mResources.setBoolean(R.bool.config_wifi_softap_acs_supported, true);
        mResources.setString(R.string.config_wifiSoftap2gChannelList, acsChannelStr2g);
        mResources.setString(R.string.config_wifiSoftap5gChannelList, acsChannelStr5g);
        mResources.setString(R.string.config_wifiSoftap6gChannelList, acsChannelStr6g);
        mHostapdHal = new HostapdHalSpy();

        when(mIHostapdMockV11.addAccessPoint_1_1(
                mIfaceParamsCaptorV1_1.capture(), mNetworkParamsCaptor.capture()))
                .thenReturn(mStatusSuccess);

        executeAndValidateInitializationSequenceV1_1(false);

        Builder configurationBuilder = new SoftApConfiguration.Builder();
        configurationBuilder.setSsid(NETWORK_SSID);
        configurationBuilder.setHiddenSsid(false);
        configurationBuilder.setPassphrase(NETWORK_PSK,
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        configurationBuilder.setBand(SoftApConfiguration.BAND_ANY);

        assertTrue(mHostapdHal.addAccessPoint(IFACE_NAME,
                configurationBuilder.build(),
                () -> mSoftApListener.onFailure()));
        verify(mIHostapdMockV11).addAccessPoint_1_1(any(), any());

        assertEquals(IFACE_NAME, mIfaceParamsCaptorV1_1.getValue().V1_0.ifaceName);
        assertTrue(mIfaceParamsCaptorV1_1.getValue().V1_0.hwModeParams.enable80211N);
        assertFalse(mIfaceParamsCaptorV1_1.getValue().V1_0.hwModeParams.enable80211AC);
        assertEquals(IHostapd.Band.BAND_ANY,
                mIfaceParamsCaptorV1_1.getValue().V1_0.channelParams.band);
        assertTrue(mIfaceParamsCaptorV1_1.getValue().V1_0.channelParams.enableAcs);
        assertTrue(mIfaceParamsCaptorV1_1.getValue().V1_0.channelParams.acsShouldExcludeDfs);
        assertEquals(acsChannelRanges,
                mIfaceParamsCaptorV1_1.getValue().channelParams.acsChannelRanges);

        assertEquals(NativeUtil.stringToByteArrayList(NETWORK_SSID),
                mNetworkParamsCaptor.getValue().ssid);
        assertFalse(mNetworkParamsCaptor.getValue().isHidden);
        assertEquals(IHostapd.EncryptionType.WPA2, mNetworkParamsCaptor.getValue().encryptionType);
        assertEquals(NETWORK_PSK, mNetworkParamsCaptor.getValue().pskPassphrase);
    }

    /**
     * Verifies the failure handling in addition of access point.
     */
    @Test
    public void testAddAccessPointFailure() throws Exception {
        executeAndValidateInitializationSequence();
        when(mIHostapdMock.addAccessPoint(any(), any())).thenReturn(mStatusFailure);

        Builder configurationBuilder = new SoftApConfiguration.Builder();
        configurationBuilder.setSsid(NETWORK_SSID);
        configurationBuilder.setHiddenSsid(true);
        configurationBuilder.setChannel(6, SoftApConfiguration.BAND_2GHZ);

        assertFalse(mHostapdHal.addAccessPoint(IFACE_NAME,
                configurationBuilder.build(),
                () -> mSoftApListener.onFailure()));
        verify(mIHostapdMock).addAccessPoint(any(), any());
    }

    /**
     * Verifies the failure handling in addition of access point.
     */
    @Test
    public void testAddAccessPointRemoteException() throws Exception {
        executeAndValidateInitializationSequence();
        doThrow(new RemoteException()).when(mIHostapdMock).addAccessPoint(any(), any());

        Builder configurationBuilder = new SoftApConfiguration.Builder();
        configurationBuilder.setSsid(NETWORK_SSID);
        configurationBuilder.setHiddenSsid(true);
        configurationBuilder.setChannel(6, SoftApConfiguration.BAND_2GHZ);

        assertFalse(mHostapdHal.addAccessPoint(IFACE_NAME,
                configurationBuilder.build(),
                () -> mSoftApListener.onFailure()));
        verify(mIHostapdMock).addAccessPoint(any(), any());
    }

    /**
     * Verifies the successful removal of access point.
     */
    @Test
    public void testRemoveAccessPointSuccess() throws Exception {
        executeAndValidateInitializationSequence();

        assertTrue(mHostapdHal.removeAccessPoint(IFACE_NAME));
        verify(mIHostapdMock).removeAccessPoint(any());
    }

    /**
     * Verifies the failure handling in removal of access point.
     */
    @Test
    public void testRemoveAccessPointFailure() throws Exception {
        executeAndValidateInitializationSequence();
        when(mIHostapdMock.removeAccessPoint(any())).thenReturn(mStatusFailure);

        assertFalse(mHostapdHal.removeAccessPoint(IFACE_NAME));
        verify(mIHostapdMock).removeAccessPoint(any());
    }

    /**
     * Verifies the failure handling in addition of access point.
     */
    @Test
    public void testRemoveAccessPointRemoteException() throws Exception {
        executeAndValidateInitializationSequence();
        doThrow(new RemoteException()).when(mIHostapdMock).removeAccessPoint(any());

        assertFalse(mHostapdHal.removeAccessPoint(IFACE_NAME));
        verify(mIHostapdMock).removeAccessPoint(any());
    }

    /**
     * Verifies the handling of onFailure callback from hostapd.
     */
    @Test
    public void testOnFailureCallbackHandling() throws Exception {
        when(mServiceManagerMock.getTransport(eq(
                android.hardware.wifi.hostapd.V1_1.IHostapd.kInterfaceName), anyString()))
                .thenReturn(IServiceManager.Transport.HWBINDER);
        mIHostapdMockV11 = mock(android.hardware.wifi.hostapd.V1_1.IHostapd.class);
        when(mIHostapdMockV11.addAccessPoint_1_1(
                mIfaceParamsCaptorV1_1.capture(), mNetworkParamsCaptor.capture()))
                .thenReturn(mStatusSuccess);
        executeAndValidateInitializationSequenceV1_1(false);

        Builder configurationBuilder = new SoftApConfiguration.Builder();
        configurationBuilder.setSsid(NETWORK_SSID);
        configurationBuilder.setBand(SoftApConfiguration.BAND_2GHZ);

        assertTrue(mHostapdHal.addAccessPoint(IFACE_NAME,
                configurationBuilder.build(),
                () -> mSoftApListener.onFailure()));
        verify(mIHostapdMockV11).addAccessPoint_1_1(any(), any());

        // Trigger on failure.
        mIHostapdCallback.onFailure(IFACE_NAME);
        verify(mSoftApListener).onFailure();

        // Now remove the access point and ensure that the callback is no longer handled.
        reset(mSoftApListener);
        assertTrue(mHostapdHal.removeAccessPoint(IFACE_NAME));
        mIHostapdCallback.onFailure(IFACE_NAME);
        verify(mSoftApListener, never()).onFailure();
    }

    private void executeAndValidateInitializationSequence() throws Exception {
        executeAndValidateInitializationSequence(false, false);
    }

    /**
     * Calls.initialize(), mocking various callback answers and verifying flow, asserting for the
     * expected result. Verifies if IHostapd manager is initialized or reset.
     */
    private void executeAndValidateInitializationSequence(
            boolean causeRegisterRemoteException, boolean causeRegisterFailure) throws Exception {
        boolean shouldSucceed = !causeRegisterRemoteException && !causeRegisterFailure;
        mInOrder = inOrder(mServiceManagerMock, mIHostapdMock);
        if (causeRegisterFailure) {
            when(mServiceManagerMock.registerForNotifications(anyString(), anyString(),
                    any(IServiceNotification.Stub.class))).thenReturn(false);
        } else if (causeRegisterRemoteException) {
            doThrow(new RemoteException()).when(mServiceManagerMock)
                    .registerForNotifications(
                            anyString(), anyString(), any(IServiceNotification.Stub.class));
        }
        // Initialize HostapdHal, should call serviceManager.registerForNotifications
        assertEquals(shouldSucceed, mHostapdHal.initialize());
        // verify: service manager initialization sequence
        mInOrder.verify(mServiceManagerMock).linkToDeath(mServiceManagerDeathCaptor.capture(),
                anyLong());
        mInOrder.verify(mServiceManagerMock).registerForNotifications(
                eq(IHostapd.kInterfaceName), eq(""), mServiceNotificationCaptor.capture());
        if (shouldSucceed) {
            // act: cause the onRegistration(...) callback to execute
            mServiceNotificationCaptor.getValue().onRegistration(IHostapd.kInterfaceName, "", true);
            assertTrue(mHostapdHal.isInitializationComplete());
            mInOrder.verify(mIHostapdMock).linkToDeath(mHostapdDeathCaptor.capture(),
                    mDeathRecipientCookieCaptor.capture());
        } else {
            assertFalse(mHostapdHal.isInitializationComplete());
            mInOrder.verify(mIHostapdMock, never()).linkToDeath(
                    mHostapdDeathCaptor.capture(), anyLong());
        }
    }

    /**
     * Calls.initialize(), mocking various callback answers and verifying flow, asserting for the
     * expected result. Verifies if IHostapd manager is initialized or reset.
     */
    private void executeAndValidateInitializationSequenceV1_1(
            boolean causeCallbackFailure) throws Exception {
        boolean shouldSucceed = !causeCallbackFailure;
        mInOrder = inOrder(mServiceManagerMock, mIHostapdMock);
        if (causeCallbackFailure) {
            doAnswer(new MockAnswerUtil.AnswerWithArguments() {
                public HostapdStatus answer(IHostapdCallback cb)
                        throws RemoteException {
                    return mStatusFailure;
                }
            }).when(mIHostapdMockV11).registerCallback(any(IHostapdCallback.class));
        } else {
            doAnswer(new MockAnswerUtil.AnswerWithArguments() {
                public HostapdStatus answer(IHostapdCallback cb)
                        throws RemoteException {
                    mIHostapdCallback = cb;
                    return mStatusSuccess;
                }
            }).when(mIHostapdMockV11).registerCallback(any(IHostapdCallback.class));
        }
        // Initialize HostapdHal, should call serviceManager.registerForNotifications
        assertTrue(mHostapdHal.initialize());
        // verify: service manager initialization sequence
        mInOrder.verify(mServiceManagerMock).linkToDeath(mServiceManagerDeathCaptor.capture(),
                anyLong());
        mInOrder.verify(mServiceManagerMock).registerForNotifications(
                eq(IHostapd.kInterfaceName), eq(""), mServiceNotificationCaptor.capture());
        // act: cause the onRegistration(...) callback to execute
        mServiceNotificationCaptor.getValue().onRegistration(IHostapd.kInterfaceName, "", true);
        assertEquals(shouldSucceed, mHostapdHal.isInitializationComplete());
        mInOrder.verify(mIHostapdMock).linkToDeath(mHostapdDeathCaptor.capture(), anyLong());
        verify(mIHostapdMockV11).registerCallback(any(IHostapdCallback.class));
    }

    /**
     * Calls.initialize(), mocking various callback answers and verifying flow, asserting for the
     * expected result. Verifies if IHostapd manager is initialized or reset.
     */
    private void executeAndValidateInitializationSequenceV1_2(
            boolean causeCallbackFailure) throws Exception {
        boolean shouldSucceed = !causeCallbackFailure;
        mInOrder = inOrder(mServiceManagerMock, mIHostapdMock);
        when(mIHostapdMockV12.setDebugParams(anyInt()))
                .thenReturn(mStatusSuccess12);
        if (causeCallbackFailure) {
            doAnswer(new MockAnswerUtil.AnswerWithArguments() {
                public HostapdStatus answer(IHostapdCallback cb)
                        throws RemoteException {
                    return mStatusFailure;
                }
            }).when(mIHostapdMockV11).registerCallback(any(IHostapdCallback.class));
        } else {
            doAnswer(new MockAnswerUtil.AnswerWithArguments() {
                public HostapdStatus answer(IHostapdCallback cb)
                        throws RemoteException {
                    mIHostapdCallback = cb;
                    return mStatusSuccess;
                }
            }).when(mIHostapdMockV11).registerCallback(any(IHostapdCallback.class));
        }
        // Initialize HostapdHal, should call serviceManager.registerForNotifications
        assertTrue(mHostapdHal.initialize());
        // verify: service manager initialization sequence
        mInOrder.verify(mServiceManagerMock).linkToDeath(mServiceManagerDeathCaptor.capture(),
                anyLong());
        mInOrder.verify(mServiceManagerMock).registerForNotifications(
                eq(IHostapd.kInterfaceName), eq(""), mServiceNotificationCaptor.capture());
        // act: cause the onRegistration(...) callback to execute
        mServiceNotificationCaptor.getValue().onRegistration(IHostapd.kInterfaceName, "", true);
        assertEquals(shouldSucceed, mHostapdHal.isInitializationComplete());
        mInOrder.verify(mIHostapdMock).linkToDeath(mHostapdDeathCaptor.capture(), anyLong());
        verify(mIHostapdMockV11).registerCallback(any(IHostapdCallback.class));
    }

    private HostapdStatus createHostapdStatus(int code) {
        HostapdStatus status = new HostapdStatus();
        status.code = code;
        return status;
    }

    private android.hardware.wifi.hostapd.V1_2.HostapdStatus createHostapdStatus_1_2(int code) {
        android.hardware.wifi.hostapd.V1_2.HostapdStatus status =
                new android.hardware.wifi.hostapd.V1_2.HostapdStatus();
        status.code = code;
        return status;
    }

    /**
     * Verifies the successful execute forceClientDisconnect.
     */
    @Test
    public void testForceClientDisconnectSuccess() throws Exception {
        executeAndValidateInitializationSequence();
        when(mServiceManagerMock.getTransport(anyString(), anyString()))
                .thenReturn(IServiceManager.Transport.HWBINDER);
        MacAddress test_client = MacAddress.fromString("da:a1:19:0:0:0");
        mIHostapdMockV12 = mock(android.hardware.wifi.hostapd.V1_2.IHostapd.class);
        when(mIHostapdMockV12.forceClientDisconnect(any(), any(), anyShort()))
                .thenReturn(mStatusSuccess12);

        assertTrue(mHostapdHal.forceClientDisconnect(IFACE_NAME, test_client,
                WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER));
        verify(mIHostapdMockV12).forceClientDisconnect(any(), any(), anyShort());
    }

    @Test
    public void testForceClientDisconnectFailureDueToInvalidArg() throws Exception {
        executeAndValidateInitializationSequence();
        when(mServiceManagerMock.getTransport(anyString(), anyString()))
                .thenReturn(IServiceManager.Transport.HWBINDER);
        MacAddress test_client = MacAddress.fromString("da:a1:19:0:0:0");
        mIHostapdMockV12 = mock(android.hardware.wifi.hostapd.V1_2.IHostapd.class);
        when(mIHostapdMockV12.forceClientDisconnect(any(), any(), anyShort()))
                .thenReturn(mStatusSuccess12);

        try {
            mHostapdHal.forceClientDisconnect(IFACE_NAME, test_client, -1);
            fail();
        } catch (IllegalArgumentException e) {
        }
    }

    /**
     * Verifies the failure handling in forceClientDisconnect.
     */
    @Test
    public void testForceClientDisconnectFailure() throws Exception {
        executeAndValidateInitializationSequence();
        when(mServiceManagerMock.getTransport(anyString(), anyString()))
                .thenReturn(IServiceManager.Transport.HWBINDER);
        MacAddress test_client = MacAddress.fromString("da:a1:19:0:0:0");
        mIHostapdMockV12 = mock(android.hardware.wifi.hostapd.V1_2.IHostapd.class);
        when(mIHostapdMockV12.forceClientDisconnect(any(), any(), anyShort()))
                .thenReturn(mStatusFailure12);

        assertFalse(mHostapdHal.forceClientDisconnect(IFACE_NAME, test_client,
                WifiManager.SAP_CLIENT_BLOCK_REASON_CODE_BLOCKED_BY_USER));
        verify(mIHostapdMockV12).forceClientDisconnect(any(), any(), anyShort());
    }

    /**
     * Verifies the exception handling in forceClientDisconnect.
     */
    @Test
    public void testforceClientDisconnectRemoteException() throws Exception {
        executeAndValidateInitializationSequence();
        when(mServiceManagerMock.getTransport(anyString(), anyString()))
                .thenReturn(IServiceManager.Transport.HWBINDER);
        MacAddress test_client = MacAddress.fromString("da:a1:19:0:0:0");
        mIHostapdMockV12 = mock(android.hardware.wifi.hostapd.V1_2.IHostapd.class);
        doThrow(new RemoteException()).when(mIHostapdMockV12)
                .forceClientDisconnect(any(), any(), anyShort());

        assertFalse(mHostapdHal.forceClientDisconnect(IFACE_NAME, test_client, 0));
        verify(mIHostapdMockV12).forceClientDisconnect(any(), any(), anyShort());
    }

    /**
     * Verifies the HIDL not support handling in forceClientDisconnect.
     */
    @Test
    public void testforceClientDisconnectHIDLNotSupport() throws Exception {
        executeAndValidateInitializationSequence();
        MacAddress test_client = MacAddress.fromString("da:a1:19:0:0:0");

        assertFalse(mHostapdHal.forceClientDisconnect(IFACE_NAME, test_client, 0));
    }

    /**
     * Verifies the setting of log level.
     */
    @Test
    public void testSetLogLevel() throws Exception {
        executeAndValidateInitializationSequence();
        when(mServiceManagerMock.getTransport(anyString(), anyString()))
                .thenReturn(IServiceManager.Transport.HWBINDER);
        mIHostapdMockV12 = mock(android.hardware.wifi.hostapd.V1_2.IHostapd.class);
        when(mIHostapdMockV12.setDebugParams(anyInt()))
                .thenReturn(mStatusSuccess12);

        mHostapdHal.enableVerboseLogging(true);
        verify(mIHostapdMockV12)
                .setDebugParams(eq(DebugLevel.DEBUG));

        mHostapdHal.enableVerboseLogging(false);
        verify(mIHostapdMockV12)
                .setDebugParams(eq(DebugLevel.INFO));
    }

    /*
     * Sunny day scenario for V1.2 HostapdHal initialization
     * Asserts successful initialization
     */
    @Test
    public void testInitialize_successV1_2() throws Exception {
        when(mServiceManagerMock.getTransport(anyString(), anyString()))
                .thenReturn(IServiceManager.Transport.HWBINDER);
        mIHostapdMockV11 = mock(android.hardware.wifi.hostapd.V1_1.IHostapd.class);
        mIHostapdMockV12 = mock(android.hardware.wifi.hostapd.V1_2.IHostapd.class);
        executeAndValidateInitializationSequenceV1_2(false);
    }

    /**
     * Verifies the successful addition of access point with SAE.
     */
    @Test
    public void testAddAccessPointSuccess_SAE_WithoutACS() throws Exception {
        when(mServiceManagerMock.getTransport(anyString(), anyString()))
                .thenReturn(IServiceManager.Transport.HWBINDER);
        mIHostapdMockV11 = mock(android.hardware.wifi.hostapd.V1_1.IHostapd.class);
        mIHostapdMockV12 = mock(android.hardware.wifi.hostapd.V1_2.IHostapd.class);
        // Disable ACS in the config.
        mResources.setBoolean(R.bool.config_wifi_softap_acs_supported, false);
        mHostapdHal = new HostapdHalSpy();

        executeAndValidateInitializationSequenceV1_2(false);

        Builder configurationBuilder = new SoftApConfiguration.Builder();
        configurationBuilder.setSsid(NETWORK_SSID);
        configurationBuilder.setHiddenSsid(false);
        configurationBuilder.setPassphrase(NETWORK_PSK,
                SoftApConfiguration.SECURITY_TYPE_WPA3_SAE);
        configurationBuilder.setBand(SoftApConfiguration.BAND_ANY);

        when(mIHostapdMockV12.addAccessPoint_1_2(
                mIfaceParamsCaptorV12.capture(), mNetworkParamsV12Captor.capture()))
                .thenReturn(mStatusSuccess12);


        assertTrue(mHostapdHal.addAccessPoint(IFACE_NAME,
                configurationBuilder.build(),
                () -> mSoftApListener.onFailure()));
        verify(mIHostapdMockV12).addAccessPoint_1_2(any(), any());

        assertEquals(IFACE_NAME, mIfaceParamsCaptorV12.getValue().V1_1.V1_0.ifaceName);
        assertTrue(mIfaceParamsCaptorV12.getValue().V1_1.V1_0.hwModeParams.enable80211N);
        assertFalse(mIfaceParamsCaptorV12.getValue().V1_1.V1_0.hwModeParams.enable80211AC);
        assertFalse(mIfaceParamsCaptorV12.getValue().V1_1.V1_0.channelParams.enableAcs);

        assertEquals(NativeUtil.stringToByteArrayList(NETWORK_SSID),
                mNetworkParamsV12Captor.getValue().V1_0.ssid);
        assertFalse(mNetworkParamsV12Captor.getValue().V1_0.isHidden);
        assertEquals(android.hardware.wifi.hostapd.V1_2.IHostapd.EncryptionType.WPA3_SAE,
                mNetworkParamsV12Captor.getValue().encryptionType);
        assertEquals(NETWORK_PSK, mNetworkParamsV12Captor.getValue().passphrase);
        assertEquals("", mNetworkParamsV12Captor.getValue().V1_0.pskPassphrase);
    }

    /**
     * Verifies the successful addition of access point with SAE Transition.
     */
    @Test
    public void testAddAccessPointSuccess_SAE_Transition_WithoutACS() throws Exception {
        when(mServiceManagerMock.getTransport(anyString(), anyString()))
                .thenReturn(IServiceManager.Transport.HWBINDER);
        mIHostapdMockV11 = mock(android.hardware.wifi.hostapd.V1_1.IHostapd.class);
        mIHostapdMockV12 = mock(android.hardware.wifi.hostapd.V1_2.IHostapd.class);
        // Disable ACS in the config.
        mResources.setBoolean(R.bool.config_wifi_softap_acs_supported, false);
        mHostapdHal = new HostapdHalSpy();

        executeAndValidateInitializationSequenceV1_2(false);

        Builder configurationBuilder = new SoftApConfiguration.Builder();
        configurationBuilder.setSsid(NETWORK_SSID);
        configurationBuilder.setHiddenSsid(false);
        configurationBuilder.setPassphrase(NETWORK_PSK,
                SoftApConfiguration.SECURITY_TYPE_WPA3_SAE_TRANSITION);
        configurationBuilder.setBand(SoftApConfiguration.BAND_ANY);

        when(mIHostapdMockV12.addAccessPoint_1_2(
                mIfaceParamsCaptorV12.capture(), mNetworkParamsV12Captor.capture()))
                .thenReturn(mStatusSuccess12);


        assertTrue(mHostapdHal.addAccessPoint(IFACE_NAME,
                configurationBuilder.build(),
                () -> mSoftApListener.onFailure()));
        verify(mIHostapdMockV12).addAccessPoint_1_2(any(), any());

        assertEquals(IFACE_NAME, mIfaceParamsCaptorV12.getValue().V1_1.V1_0.ifaceName);
        assertTrue(mIfaceParamsCaptorV12.getValue().V1_1.V1_0.hwModeParams.enable80211N);
        assertFalse(mIfaceParamsCaptorV12.getValue().V1_1.V1_0.hwModeParams.enable80211AC);
        assertFalse(mIfaceParamsCaptorV12.getValue().V1_1.V1_0.channelParams.enableAcs);

        assertEquals(NativeUtil.stringToByteArrayList(NETWORK_SSID),
                mNetworkParamsV12Captor.getValue().V1_0.ssid);
        assertFalse(mNetworkParamsV12Captor.getValue().V1_0.isHidden);
        assertEquals(android.hardware.wifi.hostapd.V1_2.IHostapd.EncryptionType.WPA3_SAE_TRANSITION,
                mNetworkParamsV12Captor.getValue().encryptionType);
        assertEquals(NETWORK_PSK, mNetworkParamsV12Captor.getValue().passphrase);
        assertEquals("", mNetworkParamsV12Captor.getValue().V1_0.pskPassphrase);
    }

    /**
     * Verifies the failure handling addition of access point with SAE Transition in old hal.
     */
    @Test
    public void testAddAccessPointFailure_SAEWithOldHal() throws Exception {
        when(mServiceManagerMock.getTransport(anyString(), anyString()))
                .thenReturn(IServiceManager.Transport.HWBINDER);
        mIHostapdMockV11 = mock(android.hardware.wifi.hostapd.V1_1.IHostapd.class);
        // Disable ACS in the config.
        mResources.setBoolean(R.bool.config_wifi_softap_acs_supported, false);
        mHostapdHal = new HostapdHalSpy();

        executeAndValidateInitializationSequenceV1_1(false);

        Builder configurationBuilder = new SoftApConfiguration.Builder();
        configurationBuilder.setSsid(NETWORK_SSID);
        configurationBuilder.setHiddenSsid(false);
        configurationBuilder.setPassphrase(NETWORK_PSK,
                SoftApConfiguration.SECURITY_TYPE_WPA3_SAE);
        configurationBuilder.setBand(SoftApConfiguration.BAND_ANY);

        assertFalse(mHostapdHal.addAccessPoint(IFACE_NAME,
                configurationBuilder.build(),
                () -> mSoftApListener.onFailure()));
    }

    /**
     * Verifies the successful addition of access point when ACS is allowed to include DFS channels.
     */
    @Test
    public void testAddAccessPointSuccess_WithACS_IncludeDFSChannels() throws Exception {
        // Enable ACS in the config.
        mResources.setBoolean(R.bool.config_wifi_softap_acs_supported, true);
        mResources.setBoolean(R.bool.config_wifiSoftapAcsIncludeDfs, true);
        mHostapdHal = new HostapdHalSpy();

        executeAndValidateInitializationSequence();

        Builder configurationBuilder = new SoftApConfiguration.Builder();
        configurationBuilder.setSsid(NETWORK_SSID);
        configurationBuilder.setHiddenSsid(false);
        configurationBuilder.setPassphrase(NETWORK_PSK,
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        configurationBuilder.setBand(SoftApConfiguration.BAND_ANY);

        assertTrue(mHostapdHal.addAccessPoint(IFACE_NAME,
                configurationBuilder.build(),
                () -> mSoftApListener.onFailure()));
        verify(mIHostapdMock).addAccessPoint(any(), any());

        assertEquals(IFACE_NAME, mIfaceParamsCaptor.getValue().ifaceName);
        assertTrue(mIfaceParamsCaptor.getValue().hwModeParams.enable80211N);
        assertFalse(mIfaceParamsCaptor.getValue().hwModeParams.enable80211AC);
        assertEquals(IHostapd.Band.BAND_ANY, mIfaceParamsCaptor.getValue().channelParams.band);
        assertTrue(mIfaceParamsCaptor.getValue().channelParams.enableAcs);
        assertFalse(mIfaceParamsCaptor.getValue().channelParams.acsShouldExcludeDfs);

        assertEquals(NativeUtil.stringToByteArrayList(NETWORK_SSID),
                mNetworkParamsCaptor.getValue().ssid);
        assertFalse(mNetworkParamsCaptor.getValue().isHidden);
        assertEquals(IHostapd.EncryptionType.WPA2, mNetworkParamsCaptor.getValue().encryptionType);
        assertEquals(NETWORK_PSK, mNetworkParamsCaptor.getValue().pskPassphrase);
    }
}

