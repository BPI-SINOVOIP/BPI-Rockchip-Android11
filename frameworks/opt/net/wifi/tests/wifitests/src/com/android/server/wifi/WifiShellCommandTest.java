/*
 * Copyright (C) 2020 The Android Open Source Project
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.validateMockitoUsage;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.wifi.SoftApConfiguration;
import android.os.Binder;
import android.os.Process;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.io.FileDescriptor;

/**
 * Unit tests for {@link com.android.server.wifi.WifiShellCommand}.
 */
@SmallTest
public class WifiShellCommandTest extends WifiBaseTest {
    private static final String TEST_PACKAGE = "com.android.test";

    @Mock WifiInjector mWifiInjector;
    @Mock ClientModeImpl mClientModeImpl;
    @Mock WifiLockManager mWifiLockManager;
    @Mock WifiNetworkSuggestionsManager mWifiNetworkSuggestionsManager;
    @Mock WifiConfigManager mWifiConfigManager;
    @Mock WifiNative mWifiNative;
    @Mock HostapdHal mHostapdHal;
    @Mock WifiCountryCode mWifiCountryCode;
    @Mock WifiLastResortWatchdog mWifiLastResortWatchdog;
    @Mock WifiServiceImpl mWifiService;
    @Mock Context mContext;
    @Mock ConnectivityManager mConnectivityManager;
    @Mock WifiCarrierInfoManager mWifiCarrierInfoManager;

    WifiShellCommand mWifiShellCommand;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        when(mWifiInjector.getClientModeImpl()).thenReturn(mClientModeImpl);
        when(mWifiInjector.getWifiLockManager()).thenReturn(mWifiLockManager);
        when(mWifiInjector.getWifiNetworkSuggestionsManager())
                .thenReturn(mWifiNetworkSuggestionsManager);
        when(mWifiInjector.getWifiConfigManager()).thenReturn(mWifiConfigManager);
        when(mWifiInjector.getHostapdHal()).thenReturn(mHostapdHal);
        when(mWifiInjector.getWifiNative()).thenReturn(mWifiNative);
        when(mWifiInjector.getWifiCountryCode()).thenReturn(mWifiCountryCode);
        when(mWifiInjector.getWifiLastResortWatchdog()).thenReturn(mWifiLastResortWatchdog);
        when(mWifiInjector.getWifiCarrierInfoManager()).thenReturn(mWifiCarrierInfoManager);

        mWifiShellCommand = new WifiShellCommand(mWifiInjector, mWifiService, mContext);

        // by default emulate shell uid.
        BinderUtil.setUid(Process.SHELL_UID);
    }

    @After
    public void tearDown() throws Exception {
        validateMockitoUsage();
    }

    @Test
    public void testSetIpReachDisconnect() {
        // not allowed for unrooted shell.
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"set-ipreach-disconnect", "enabled"});
        verify(mClientModeImpl, never()).setIpReachabilityDisconnectEnabled(anyBoolean());
        assertFalse(mWifiShellCommand.getErrPrintWriter().toString().isEmpty());

        BinderUtil.setUid(Process.ROOT_UID);

        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"set-ipreach-disconnect", "enabled"});
        verify(mClientModeImpl).setIpReachabilityDisconnectEnabled(true);

        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"set-ipreach-disconnect", "disabled"});
        verify(mClientModeImpl).setIpReachabilityDisconnectEnabled(false);

        // invalid arg
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"set-ipreach-disconnect", "yes"});
        verifyNoMoreInteractions(mClientModeImpl);
        assertFalse(mWifiShellCommand.getErrPrintWriter().toString().isEmpty());
    }

    @Test
    public void testGetIpReachDisconnect() {
        // not allowed for unrooted shell.
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"get-ipreach-disconnect"});
        verify(mClientModeImpl, never()).getIpReachabilityDisconnectEnabled();
        assertFalse(mWifiShellCommand.getErrPrintWriter().toString().isEmpty());

        BinderUtil.setUid(Process.ROOT_UID);

        when(mClientModeImpl.getIpReachabilityDisconnectEnabled()).thenReturn(true);
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"get-ipreach-disconnect"});
        verify(mClientModeImpl).getIpReachabilityDisconnectEnabled();
        mWifiShellCommand.getOutPrintWriter().toString().contains(
                "IPREACH_DISCONNECT state is true");

        when(mClientModeImpl.getIpReachabilityDisconnectEnabled()).thenReturn(false);
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"get-ipreach-disconnect"});
        verify(mClientModeImpl, times(2)).getIpReachabilityDisconnectEnabled();
        mWifiShellCommand.getOutPrintWriter().toString().contains(
                "IPREACH_DISCONNECT state is false");
    }

    @Test
    public void testSetPollRssiIntervalMsecs() {
        // not allowed for unrooted shell.
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"set-poll-rssi-interval-msecs", "5"});
        verify(mClientModeImpl, never()).setPollRssiIntervalMsecs(anyInt());
        assertFalse(mWifiShellCommand.getErrPrintWriter().toString().isEmpty());

        BinderUtil.setUid(Process.ROOT_UID);

        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"set-poll-rssi-interval-msecs", "5"});
        verify(mClientModeImpl).setPollRssiIntervalMsecs(5);

        // invalid arg
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"set-poll-rssi-interval-msecs", "0"});
        verifyNoMoreInteractions(mClientModeImpl);
        assertFalse(mWifiShellCommand.getErrPrintWriter().toString().isEmpty());
    }

    @Test
    public void testGetPollRssiIntervalMsecs() {
        // not allowed for unrooted shell.
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"get-poll-rssi-interval-msecs"});
        verify(mClientModeImpl, never()).getPollRssiIntervalMsecs();
        assertFalse(mWifiShellCommand.getErrPrintWriter().toString().isEmpty());

        BinderUtil.setUid(Process.ROOT_UID);

        when(mClientModeImpl.getPollRssiIntervalMsecs()).thenReturn(5);
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"get-poll-rssi-interval-msecs"});
        verify(mClientModeImpl).getPollRssiIntervalMsecs();
        mWifiShellCommand.getOutPrintWriter().toString().contains(
                "ClientModeImpl.mPollRssiIntervalMsecs = 5");
    }

    @Test
    public void testForceHiPerfMode() {
        // not allowed for unrooted shell.
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"force-hi-perf-mode", "enabled"});
        verify(mWifiLockManager, never()).forceHiPerfMode(anyBoolean());
        assertFalse(mWifiShellCommand.getErrPrintWriter().toString().isEmpty());

        BinderUtil.setUid(Process.ROOT_UID);

        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"force-hi-perf-mode", "enabled"});
        verify(mWifiLockManager).forceHiPerfMode(true);

        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"force-hi-perf-mode", "disabled"});
        verify(mWifiLockManager).forceHiPerfMode(false);
    }

    @Test
    public void testForceLowLatencyMode() {
        // not allowed for unrooted shell.
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"force-low-latency-mode", "enabled"});
        verify(mWifiLockManager, never()).forceLowLatencyMode(anyBoolean());
        assertFalse(mWifiShellCommand.getErrPrintWriter().toString().isEmpty());

        BinderUtil.setUid(Process.ROOT_UID);

        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"force-low-latency-mode", "enabled"});
        verify(mWifiLockManager).forceLowLatencyMode(true);

        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"force-low-latency-mode", "disabled"});
        verify(mWifiLockManager).forceLowLatencyMode(false);
    }

    @Test
    public void testNetworkSuggestionsSetUserApproved() {
        // not allowed for unrooted shell.
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"network-suggestions-set-user-approved", TEST_PACKAGE, "yes"});
        verify(mWifiNetworkSuggestionsManager, never()).setHasUserApprovedForApp(
                anyBoolean(), anyString());
        assertFalse(mWifiShellCommand.getErrPrintWriter().toString().isEmpty());

        BinderUtil.setUid(Process.ROOT_UID);

        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"network-suggestions-set-user-approved", TEST_PACKAGE, "yes"});
        verify(mWifiNetworkSuggestionsManager).setHasUserApprovedForApp(
                true, TEST_PACKAGE);

        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"network-suggestions-set-user-approved", TEST_PACKAGE, "no"});
        verify(mWifiNetworkSuggestionsManager).setHasUserApprovedForApp(
                false, TEST_PACKAGE);
    }

    @Test
    public void testNetworkSuggestionsHasUserApproved() {
        // not allowed for unrooted shell.
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"network-suggestions-has-user-approved", TEST_PACKAGE});
        verify(mWifiNetworkSuggestionsManager, never()).hasUserApprovedForApp(anyString());
        assertFalse(mWifiShellCommand.getErrPrintWriter().toString().isEmpty());

        BinderUtil.setUid(Process.ROOT_UID);

        when(mWifiNetworkSuggestionsManager.hasUserApprovedForApp(TEST_PACKAGE))
                .thenReturn(true);
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"network-suggestions-has-user-approved", TEST_PACKAGE});
        verify(mWifiNetworkSuggestionsManager).hasUserApprovedForApp(TEST_PACKAGE);
        mWifiShellCommand.getOutPrintWriter().toString().contains("yes");

        when(mWifiNetworkSuggestionsManager.hasUserApprovedForApp(TEST_PACKAGE))
                .thenReturn(false);
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"network-suggestions-has-user-approved", TEST_PACKAGE});
        verify(mWifiNetworkSuggestionsManager, times(2)).hasUserApprovedForApp(TEST_PACKAGE);
        mWifiShellCommand.getOutPrintWriter().toString().contains("no");
    }

    @Test
    public void testImsiProtectionExemptionsSetUserApproved() {
        // not allowed for unrooted shell.
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"imsi-protection-exemption-set-user-approved-for-carrier", "5",
                        "yes"});
        verify(mWifiCarrierInfoManager, never()).setHasUserApprovedImsiPrivacyExemptionForCarrier(
                anyBoolean(), anyInt());
        assertFalse(mWifiShellCommand.getErrPrintWriter().toString().isEmpty());

        BinderUtil.setUid(Process.ROOT_UID);

        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"imsi-protection-exemption-set-user-approved-for-carrier", "5",
                        "yes"});
        verify(mWifiCarrierInfoManager).setHasUserApprovedImsiPrivacyExemptionForCarrier(
                true, 5);

        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"imsi-protection-exemption-set-user-approved-for-carrier", "5",
                        "no"});
        verify(mWifiCarrierInfoManager).setHasUserApprovedImsiPrivacyExemptionForCarrier(
                false, 5);
    }

    @Test
    public void testImsiProtectionExemptionsHasUserApproved() {
        // not allowed for unrooted shell.
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"imsi-protection-exemption-has-user-approved-for-carrier", "5"});
        verify(mWifiCarrierInfoManager, never()).hasUserApprovedImsiPrivacyExemptionForCarrier(
                anyInt());
        assertFalse(mWifiShellCommand.getErrPrintWriter().toString().isEmpty());

        BinderUtil.setUid(Process.ROOT_UID);

        when(mWifiCarrierInfoManager.hasUserApprovedImsiPrivacyExemptionForCarrier(5))
                .thenReturn(true);
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"imsi-protection-exemption-has-user-approved-for-carrier", "5"});
        verify(mWifiCarrierInfoManager).hasUserApprovedImsiPrivacyExemptionForCarrier(5);
        mWifiShellCommand.getOutPrintWriter().toString().contains("yes");

        when(mWifiCarrierInfoManager.hasUserApprovedImsiPrivacyExemptionForCarrier(5))
                .thenReturn(false);
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"imsi-protection-exemption-has-user-approved-for-carrier", "5"});
        verify(mWifiCarrierInfoManager, times(2)).hasUserApprovedImsiPrivacyExemptionForCarrier(5);
        mWifiShellCommand.getOutPrintWriter().toString().contains("no");
    }

    @Test
    public void testNetworkRequestsSetUserApproved() {
        // not allowed for unrooted shell.
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"network-requests-set-user-approved", TEST_PACKAGE, "yes"});
        verify(mClientModeImpl, never()).setNetworkRequestUserApprovedApp(
                anyString(), anyBoolean());
        assertFalse(mWifiShellCommand.getErrPrintWriter().toString().isEmpty());

        BinderUtil.setUid(Process.ROOT_UID);

        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"network-requests-set-user-approved", TEST_PACKAGE, "yes"});
        verify(mClientModeImpl).setNetworkRequestUserApprovedApp(TEST_PACKAGE, true);

        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"network-requests-set-user-approved", TEST_PACKAGE, "no"});
        verify(mClientModeImpl).setNetworkRequestUserApprovedApp(TEST_PACKAGE, false);
    }

    @Test
    public void testNetworkRequestsHasUserApproved() {
        // not allowed for unrooted shell.
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"network-requests-has-user-approved", TEST_PACKAGE});
        verify(mClientModeImpl, never()).hasNetworkRequestUserApprovedApp(anyString());
        assertFalse(mWifiShellCommand.getErrPrintWriter().toString().isEmpty());

        BinderUtil.setUid(Process.ROOT_UID);

        when(mClientModeImpl.hasNetworkRequestUserApprovedApp(TEST_PACKAGE))
                .thenReturn(true);
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"network-requests-has-user-approved", TEST_PACKAGE});
        verify(mClientModeImpl).hasNetworkRequestUserApprovedApp(TEST_PACKAGE);
        mWifiShellCommand.getOutPrintWriter().toString().contains("yes");

        when(mClientModeImpl.hasNetworkRequestUserApprovedApp(TEST_PACKAGE))
                .thenReturn(false);
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"network-requests-has-user-approved", TEST_PACKAGE});
        verify(mClientModeImpl, times(2)).hasNetworkRequestUserApprovedApp(TEST_PACKAGE);
        mWifiShellCommand.getOutPrintWriter().toString().contains("no");
    }

    @Test
    public void testStartSoftAp() {
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"start-softap", "ap1", "wpa2", "xyzabc321", "-b", "5"});
        ArgumentCaptor<SoftApConfiguration> softApConfigurationCaptor = ArgumentCaptor.forClass(
                SoftApConfiguration.class);
        verify(mWifiService).startTetheredHotspot(softApConfigurationCaptor.capture());
        assertEquals(SoftApConfiguration.BAND_5GHZ,
                softApConfigurationCaptor.getValue().getBand());
        assertEquals(SoftApConfiguration.SECURITY_TYPE_WPA2_PSK,
                softApConfigurationCaptor.getValue().getSecurityType());
        assertEquals("\"ap1\"", softApConfigurationCaptor.getValue().getSsid());
        assertEquals("xyzabc321", softApConfigurationCaptor.getValue().getPassphrase());
    }

    @Test
    public void testStopSoftAp() {
        mWifiShellCommand.exec(
                new Binder(), new FileDescriptor(), new FileDescriptor(), new FileDescriptor(),
                new String[]{"stop-softap"});
        verify(mWifiService).stopSoftAp();
    }
}
