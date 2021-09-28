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

package android.net.ip;

import static android.net.INetd.IF_STATE_UP;
import static android.net.RouteInfo.RTN_UNICAST;
import static android.net.TetheringManager.TETHERING_BLUETOOTH;
import static android.net.TetheringManager.TETHERING_NCM;
import static android.net.TetheringManager.TETHERING_USB;
import static android.net.TetheringManager.TETHERING_WIFI;
import static android.net.TetheringManager.TETHERING_WIFI_P2P;
import static android.net.TetheringManager.TETHER_ERROR_ENABLE_FORWARDING_ERROR;
import static android.net.TetheringManager.TETHER_ERROR_NO_ERROR;
import static android.net.TetheringManager.TETHER_ERROR_TETHER_IFACE_ERROR;
import static android.net.dhcp.IDhcpServer.STATUS_SUCCESS;
import static android.net.ip.IpServer.STATE_AVAILABLE;
import static android.net.ip.IpServer.STATE_LOCAL_ONLY;
import static android.net.ip.IpServer.STATE_TETHERED;
import static android.net.ip.IpServer.STATE_UNAVAILABLE;
import static android.net.netlink.NetlinkConstants.RTM_DELNEIGH;
import static android.net.netlink.NetlinkConstants.RTM_NEWNEIGH;
import static android.net.netlink.StructNdMsg.NUD_FAILED;
import static android.net.netlink.StructNdMsg.NUD_REACHABLE;
import static android.net.netlink.StructNdMsg.NUD_STALE;

import static com.android.net.module.util.Inet4AddressUtils.intToInet4AddressHTH;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.usage.NetworkStatsManager;
import android.net.INetd;
import android.net.InetAddresses;
import android.net.InterfaceConfigurationParcel;
import android.net.IpPrefix;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.MacAddress;
import android.net.RouteInfo;
import android.net.TetherOffloadRuleParcel;
import android.net.TetherStatsParcel;
import android.net.dhcp.DhcpServingParamsParcel;
import android.net.dhcp.IDhcpEventCallbacks;
import android.net.dhcp.IDhcpServer;
import android.net.dhcp.IDhcpServerCallbacks;
import android.net.ip.IpNeighborMonitor.NeighborEvent;
import android.net.ip.IpNeighborMonitor.NeighborEventConsumer;
import android.net.ip.RouterAdvertisementDaemon.RaParams;
import android.net.util.InterfaceParams;
import android.net.util.InterfaceSet;
import android.net.util.PrefixUtils;
import android.net.util.SharedLog;
import android.os.Handler;
import android.os.RemoteException;
import android.os.test.TestLooper;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.networkstack.tethering.BpfCoordinator;
import com.android.networkstack.tethering.BpfCoordinator.Ipv6ForwardingRule;
import com.android.networkstack.tethering.PrivateAddressCoordinator;
import com.android.networkstack.tethering.TetheringConfiguration;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.util.Arrays;
import java.util.List;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class IpServerTest {
    private static final String IFACE_NAME = "testnet1";
    private static final String UPSTREAM_IFACE = "upstream0";
    private static final String UPSTREAM_IFACE2 = "upstream1";
    private static final int UPSTREAM_IFINDEX = 101;
    private static final int UPSTREAM_IFINDEX2 = 102;
    private static final String BLUETOOTH_IFACE_ADDR = "192.168.44.1";
    private static final int BLUETOOTH_DHCP_PREFIX_LENGTH = 24;
    private static final int DHCP_LEASE_TIME_SECS = 3600;
    private static final boolean DEFAULT_USING_BPF_OFFLOAD = true;

    private static final InterfaceParams TEST_IFACE_PARAMS = new InterfaceParams(
            IFACE_NAME, 42 /* index */, MacAddress.ALL_ZEROS_ADDRESS, 1500 /* defaultMtu */);

    private static final int MAKE_DHCPSERVER_TIMEOUT_MS = 1000;

    private final LinkAddress mTestAddress = new LinkAddress("192.168.42.5/24");
    private final IpPrefix mBluetoothPrefix = new IpPrefix("192.168.44.0/24");

    @Mock private INetd mNetd;
    @Mock private IpServer.Callback mCallback;
    @Mock private SharedLog mSharedLog;
    @Mock private IDhcpServer mDhcpServer;
    @Mock private RouterAdvertisementDaemon mRaDaemon;
    @Mock private IpNeighborMonitor mIpNeighborMonitor;
    @Mock private IpServer.Dependencies mDependencies;
    @Mock private PrivateAddressCoordinator mAddressCoordinator;
    @Mock private NetworkStatsManager mStatsManager;
    @Mock private TetheringConfiguration mTetherConfig;

    @Captor private ArgumentCaptor<DhcpServingParamsParcel> mDhcpParamsCaptor;

    private final TestLooper mLooper = new TestLooper();
    private final ArgumentCaptor<LinkProperties> mLinkPropertiesCaptor =
            ArgumentCaptor.forClass(LinkProperties.class);
    private IpServer mIpServer;
    private InterfaceConfigurationParcel mInterfaceConfiguration;
    private NeighborEventConsumer mNeighborEventConsumer;
    private BpfCoordinator mBpfCoordinator;

    private void initStateMachine(int interfaceType) throws Exception {
        initStateMachine(interfaceType, false /* usingLegacyDhcp */, DEFAULT_USING_BPF_OFFLOAD);
    }

    private void initStateMachine(int interfaceType, boolean usingLegacyDhcp,
            boolean usingBpfOffload) throws Exception {
        doAnswer(inv -> {
            final IDhcpServerCallbacks cb = inv.getArgument(2);
            new Thread(() -> {
                try {
                    cb.onDhcpServerCreated(STATUS_SUCCESS, mDhcpServer);
                } catch (RemoteException e) {
                    fail(e.getMessage());
                }
            }).run();
            return null;
        }).when(mDependencies).makeDhcpServer(any(), mDhcpParamsCaptor.capture(), any());
        when(mDependencies.getRouterAdvertisementDaemon(any())).thenReturn(mRaDaemon);
        when(mDependencies.getInterfaceParams(IFACE_NAME)).thenReturn(TEST_IFACE_PARAMS);

        when(mDependencies.getIfindex(eq(UPSTREAM_IFACE))).thenReturn(UPSTREAM_IFINDEX);
        when(mDependencies.getIfindex(eq(UPSTREAM_IFACE2))).thenReturn(UPSTREAM_IFINDEX2);

        mInterfaceConfiguration = new InterfaceConfigurationParcel();
        mInterfaceConfiguration.flags = new String[0];
        if (interfaceType == TETHERING_BLUETOOTH) {
            mInterfaceConfiguration.ipv4Addr = BLUETOOTH_IFACE_ADDR;
            mInterfaceConfiguration.prefixLength = BLUETOOTH_DHCP_PREFIX_LENGTH;
        }

        ArgumentCaptor<NeighborEventConsumer> neighborCaptor =
                ArgumentCaptor.forClass(NeighborEventConsumer.class);
        doReturn(mIpNeighborMonitor).when(mDependencies).getIpNeighborMonitor(any(), any(),
                neighborCaptor.capture());

        mIpServer = new IpServer(
                IFACE_NAME, mLooper.getLooper(), interfaceType, mSharedLog, mNetd, mBpfCoordinator,
                mCallback, usingLegacyDhcp, usingBpfOffload, mAddressCoordinator, mDependencies);
        mIpServer.start();
        mNeighborEventConsumer = neighborCaptor.getValue();

        // Starting the state machine always puts us in a consistent state and notifies
        // the rest of the world that we've changed from an unknown to available state.
        mLooper.dispatchAll();
        reset(mNetd, mCallback);

        when(mRaDaemon.start()).thenReturn(true);
    }

    private void initTetheredStateMachine(int interfaceType, String upstreamIface)
            throws Exception {
        initTetheredStateMachine(interfaceType, upstreamIface, false,
                DEFAULT_USING_BPF_OFFLOAD);
    }

    private void initTetheredStateMachine(int interfaceType, String upstreamIface,
            boolean usingLegacyDhcp, boolean usingBpfOffload) throws Exception {
        initStateMachine(interfaceType, usingLegacyDhcp, usingBpfOffload);
        dispatchCommand(IpServer.CMD_TETHER_REQUESTED, STATE_TETHERED);
        if (upstreamIface != null) {
            LinkProperties lp = new LinkProperties();
            lp.setInterfaceName(upstreamIface);
            dispatchTetherConnectionChanged(upstreamIface, lp, 0);
        }
        reset(mNetd, mCallback, mAddressCoordinator);
        when(mAddressCoordinator.requestDownstreamAddress(any())).thenReturn(mTestAddress);
    }

    @Before public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        when(mSharedLog.forSubComponent(anyString())).thenReturn(mSharedLog);
        when(mAddressCoordinator.requestDownstreamAddress(any())).thenReturn(mTestAddress);
        when(mTetherConfig.isBpfOffloadEnabled()).thenReturn(true /* default value */);

        mBpfCoordinator = spy(new BpfCoordinator(
                new BpfCoordinator.Dependencies() {
                    @NonNull
                    public Handler getHandler() {
                        return new Handler(mLooper.getLooper());
                    }

                    @NonNull
                    public INetd getNetd() {
                        return mNetd;
                    }

                    @NonNull
                    public NetworkStatsManager getNetworkStatsManager() {
                        return mStatsManager;
                    }

                    @NonNull
                    public SharedLog getSharedLog() {
                        return mSharedLog;
                    }

                    @Nullable
                    public TetheringConfiguration getTetherConfig() {
                        return mTetherConfig;
                    }
                }));
    }

    @Test
    public void startsOutAvailable() {
        when(mDependencies.getIpNeighborMonitor(any(), any(), any()))
                .thenReturn(mIpNeighborMonitor);
        mIpServer = new IpServer(IFACE_NAME, mLooper.getLooper(), TETHERING_BLUETOOTH, mSharedLog,
                mNetd, mBpfCoordinator, mCallback, false /* usingLegacyDhcp */,
                DEFAULT_USING_BPF_OFFLOAD, mAddressCoordinator, mDependencies);
        mIpServer.start();
        mLooper.dispatchAll();
        verify(mCallback).updateInterfaceState(
                mIpServer, STATE_AVAILABLE, TETHER_ERROR_NO_ERROR);
        verify(mCallback).updateLinkProperties(eq(mIpServer), any(LinkProperties.class));
        verifyNoMoreInteractions(mCallback, mNetd);
    }

    @Test
    public void shouldDoNothingUntilRequested() throws Exception {
        initStateMachine(TETHERING_BLUETOOTH);
        final int [] noOp_commands = {
            IpServer.CMD_TETHER_UNREQUESTED,
            IpServer.CMD_IP_FORWARDING_ENABLE_ERROR,
            IpServer.CMD_IP_FORWARDING_DISABLE_ERROR,
            IpServer.CMD_START_TETHERING_ERROR,
            IpServer.CMD_STOP_TETHERING_ERROR,
            IpServer.CMD_SET_DNS_FORWARDERS_ERROR,
            IpServer.CMD_TETHER_CONNECTION_CHANGED
        };
        for (int command : noOp_commands) {
            // None of these commands should trigger us to request action from
            // the rest of the system.
            dispatchCommand(command);
            verifyNoMoreInteractions(mNetd, mCallback);
        }
    }

    @Test
    public void handlesImmediateInterfaceDown() throws Exception {
        initStateMachine(TETHERING_BLUETOOTH);

        dispatchCommand(IpServer.CMD_INTERFACE_DOWN);
        verify(mCallback).updateInterfaceState(
                mIpServer, STATE_UNAVAILABLE, TETHER_ERROR_NO_ERROR);
        verify(mCallback).updateLinkProperties(eq(mIpServer), any(LinkProperties.class));
        verifyNoMoreInteractions(mNetd, mCallback);
    }

    @Test
    public void canBeTethered() throws Exception {
        initStateMachine(TETHERING_BLUETOOTH);

        dispatchCommand(IpServer.CMD_TETHER_REQUESTED, STATE_TETHERED);
        InOrder inOrder = inOrder(mCallback, mNetd);
        inOrder.verify(mNetd).tetherInterfaceAdd(IFACE_NAME);
        inOrder.verify(mNetd).networkAddInterface(INetd.LOCAL_NET_ID, IFACE_NAME);
        // One for ipv4 route, one for ipv6 link local route.
        inOrder.verify(mNetd, times(2)).networkAddRoute(eq(INetd.LOCAL_NET_ID), eq(IFACE_NAME),
                any(), any());
        inOrder.verify(mCallback).updateInterfaceState(
                mIpServer, STATE_TETHERED, TETHER_ERROR_NO_ERROR);
        inOrder.verify(mCallback).updateLinkProperties(
                eq(mIpServer), any(LinkProperties.class));
        verifyNoMoreInteractions(mNetd, mCallback);
    }

    @Test
    public void canUnrequestTethering() throws Exception {
        initTetheredStateMachine(TETHERING_BLUETOOTH, null);

        dispatchCommand(IpServer.CMD_TETHER_UNREQUESTED);
        InOrder inOrder = inOrder(mCallback, mNetd, mAddressCoordinator);
        inOrder.verify(mNetd).tetherApplyDnsInterfaces();
        inOrder.verify(mNetd).tetherInterfaceRemove(IFACE_NAME);
        inOrder.verify(mNetd).networkRemoveInterface(INetd.LOCAL_NET_ID, IFACE_NAME);
        inOrder.verify(mNetd).interfaceSetCfg(argThat(cfg -> IFACE_NAME.equals(cfg.ifName)));
        inOrder.verify(mAddressCoordinator).releaseDownstream(any());
        inOrder.verify(mCallback).updateInterfaceState(
                mIpServer, STATE_AVAILABLE, TETHER_ERROR_NO_ERROR);
        inOrder.verify(mCallback).updateLinkProperties(
                eq(mIpServer), any(LinkProperties.class));
        verifyNoMoreInteractions(mNetd, mCallback, mAddressCoordinator);
    }

    @Test
    public void canBeTetheredAsUsb() throws Exception {
        initStateMachine(TETHERING_USB);

        dispatchCommand(IpServer.CMD_TETHER_REQUESTED, STATE_TETHERED);
        InOrder inOrder = inOrder(mCallback, mNetd, mAddressCoordinator);
        inOrder.verify(mAddressCoordinator).requestDownstreamAddress(any());
        inOrder.verify(mNetd).interfaceSetCfg(argThat(cfg ->
                  IFACE_NAME.equals(cfg.ifName) && assertContainsFlag(cfg.flags, IF_STATE_UP)));
        inOrder.verify(mNetd).tetherInterfaceAdd(IFACE_NAME);
        inOrder.verify(mNetd).networkAddInterface(INetd.LOCAL_NET_ID, IFACE_NAME);
        inOrder.verify(mNetd, times(2)).networkAddRoute(eq(INetd.LOCAL_NET_ID), eq(IFACE_NAME),
                any(), any());
        inOrder.verify(mCallback).updateInterfaceState(
                mIpServer, STATE_TETHERED, TETHER_ERROR_NO_ERROR);
        inOrder.verify(mCallback).updateLinkProperties(
                eq(mIpServer), mLinkPropertiesCaptor.capture());
        assertIPv4AddressAndDirectlyConnectedRoute(mLinkPropertiesCaptor.getValue());
        verifyNoMoreInteractions(mNetd, mCallback, mAddressCoordinator);
    }

    @Test
    public void canBeTetheredAsWifiP2p() throws Exception {
        initStateMachine(TETHERING_WIFI_P2P);

        dispatchCommand(IpServer.CMD_TETHER_REQUESTED, STATE_LOCAL_ONLY);
        InOrder inOrder = inOrder(mCallback, mNetd, mAddressCoordinator);
        inOrder.verify(mAddressCoordinator).requestDownstreamAddress(any());
        inOrder.verify(mNetd).interfaceSetCfg(argThat(cfg ->
                  IFACE_NAME.equals(cfg.ifName) && assertNotContainsFlag(cfg.flags, IF_STATE_UP)));
        inOrder.verify(mNetd).tetherInterfaceAdd(IFACE_NAME);
        inOrder.verify(mNetd).networkAddInterface(INetd.LOCAL_NET_ID, IFACE_NAME);
        inOrder.verify(mNetd, times(2)).networkAddRoute(eq(INetd.LOCAL_NET_ID), eq(IFACE_NAME),
                any(), any());
        inOrder.verify(mCallback).updateInterfaceState(
                mIpServer, STATE_LOCAL_ONLY, TETHER_ERROR_NO_ERROR);
        inOrder.verify(mCallback).updateLinkProperties(
                eq(mIpServer), mLinkPropertiesCaptor.capture());
        assertIPv4AddressAndDirectlyConnectedRoute(mLinkPropertiesCaptor.getValue());
        verifyNoMoreInteractions(mNetd, mCallback, mAddressCoordinator);
    }

    @Test
    public void handlesFirstUpstreamChange() throws Exception {
        initTetheredStateMachine(TETHERING_BLUETOOTH, null);

        // Telling the state machine about its upstream interface triggers
        // a little more configuration.
        dispatchTetherConnectionChanged(UPSTREAM_IFACE);
        InOrder inOrder = inOrder(mNetd);
        inOrder.verify(mNetd).tetherAddForward(IFACE_NAME, UPSTREAM_IFACE);
        inOrder.verify(mNetd).ipfwdAddInterfaceForward(IFACE_NAME, UPSTREAM_IFACE);
        verifyNoMoreInteractions(mNetd, mCallback);
    }

    @Test
    public void handlesChangingUpstream() throws Exception {
        initTetheredStateMachine(TETHERING_BLUETOOTH, UPSTREAM_IFACE);

        dispatchTetherConnectionChanged(UPSTREAM_IFACE2);
        InOrder inOrder = inOrder(mNetd);
        inOrder.verify(mNetd).ipfwdRemoveInterfaceForward(IFACE_NAME, UPSTREAM_IFACE);
        inOrder.verify(mNetd).tetherRemoveForward(IFACE_NAME, UPSTREAM_IFACE);
        inOrder.verify(mNetd).tetherAddForward(IFACE_NAME, UPSTREAM_IFACE2);
        inOrder.verify(mNetd).ipfwdAddInterfaceForward(IFACE_NAME, UPSTREAM_IFACE2);
        verifyNoMoreInteractions(mNetd, mCallback);
    }

    @Test
    public void handlesChangingUpstreamNatFailure() throws Exception {
        initTetheredStateMachine(TETHERING_WIFI, UPSTREAM_IFACE);

        doThrow(RemoteException.class).when(mNetd).tetherAddForward(IFACE_NAME, UPSTREAM_IFACE2);

        dispatchTetherConnectionChanged(UPSTREAM_IFACE2);
        InOrder inOrder = inOrder(mNetd);
        inOrder.verify(mNetd).ipfwdRemoveInterfaceForward(IFACE_NAME, UPSTREAM_IFACE);
        inOrder.verify(mNetd).tetherRemoveForward(IFACE_NAME, UPSTREAM_IFACE);
        inOrder.verify(mNetd).tetherAddForward(IFACE_NAME, UPSTREAM_IFACE2);
        inOrder.verify(mNetd).ipfwdRemoveInterfaceForward(IFACE_NAME, UPSTREAM_IFACE2);
        inOrder.verify(mNetd).tetherRemoveForward(IFACE_NAME, UPSTREAM_IFACE2);
    }

    @Test
    public void handlesChangingUpstreamInterfaceForwardingFailure() throws Exception {
        initTetheredStateMachine(TETHERING_WIFI, UPSTREAM_IFACE);

        doThrow(RemoteException.class).when(mNetd).ipfwdAddInterfaceForward(
                IFACE_NAME, UPSTREAM_IFACE2);

        dispatchTetherConnectionChanged(UPSTREAM_IFACE2);
        InOrder inOrder = inOrder(mNetd);
        inOrder.verify(mNetd).ipfwdRemoveInterfaceForward(IFACE_NAME, UPSTREAM_IFACE);
        inOrder.verify(mNetd).tetherRemoveForward(IFACE_NAME, UPSTREAM_IFACE);
        inOrder.verify(mNetd).tetherAddForward(IFACE_NAME, UPSTREAM_IFACE2);
        inOrder.verify(mNetd).ipfwdAddInterfaceForward(IFACE_NAME, UPSTREAM_IFACE2);
        inOrder.verify(mNetd).ipfwdRemoveInterfaceForward(IFACE_NAME, UPSTREAM_IFACE2);
        inOrder.verify(mNetd).tetherRemoveForward(IFACE_NAME, UPSTREAM_IFACE2);
    }

    @Test
    public void canUnrequestTetheringWithUpstream() throws Exception {
        initTetheredStateMachine(TETHERING_BLUETOOTH, UPSTREAM_IFACE);

        dispatchCommand(IpServer.CMD_TETHER_UNREQUESTED);
        InOrder inOrder = inOrder(mNetd, mCallback, mAddressCoordinator);
        inOrder.verify(mNetd).ipfwdRemoveInterfaceForward(IFACE_NAME, UPSTREAM_IFACE);
        inOrder.verify(mNetd).tetherRemoveForward(IFACE_NAME, UPSTREAM_IFACE);
        inOrder.verify(mNetd).tetherApplyDnsInterfaces();
        inOrder.verify(mNetd).tetherInterfaceRemove(IFACE_NAME);
        inOrder.verify(mNetd).networkRemoveInterface(INetd.LOCAL_NET_ID, IFACE_NAME);
        inOrder.verify(mNetd).interfaceSetCfg(argThat(cfg -> IFACE_NAME.equals(cfg.ifName)));
        inOrder.verify(mAddressCoordinator).releaseDownstream(any());
        inOrder.verify(mCallback).updateInterfaceState(
                mIpServer, STATE_AVAILABLE, TETHER_ERROR_NO_ERROR);
        inOrder.verify(mCallback).updateLinkProperties(
                eq(mIpServer), any(LinkProperties.class));
        verifyNoMoreInteractions(mNetd, mCallback, mAddressCoordinator);
    }

    @Test
    public void interfaceDownLeadsToUnavailable() throws Exception {
        for (boolean shouldThrow : new boolean[]{true, false}) {
            initTetheredStateMachine(TETHERING_USB, null);

            if (shouldThrow) {
                doThrow(RemoteException.class).when(mNetd).tetherInterfaceRemove(IFACE_NAME);
            }
            dispatchCommand(IpServer.CMD_INTERFACE_DOWN);
            InOrder usbTeardownOrder = inOrder(mNetd, mCallback);
            // Currently IpServer interfaceSetCfg twice to stop IPv4. One just set interface down
            // Another one is set IPv4 to 0.0.0.0/0 as clearng ipv4 address.
            usbTeardownOrder.verify(mNetd, times(2)).interfaceSetCfg(
                    argThat(cfg -> IFACE_NAME.equals(cfg.ifName)));
            usbTeardownOrder.verify(mCallback).updateInterfaceState(
                    mIpServer, STATE_UNAVAILABLE, TETHER_ERROR_NO_ERROR);
            usbTeardownOrder.verify(mCallback).updateLinkProperties(
                    eq(mIpServer), mLinkPropertiesCaptor.capture());
            assertNoAddressesNorRoutes(mLinkPropertiesCaptor.getValue());
        }
    }

    @Test
    public void usbShouldBeTornDownOnTetherError() throws Exception {
        initStateMachine(TETHERING_USB);

        doThrow(RemoteException.class).when(mNetd).tetherInterfaceAdd(IFACE_NAME);
        dispatchCommand(IpServer.CMD_TETHER_REQUESTED, STATE_TETHERED);
        InOrder usbTeardownOrder = inOrder(mNetd, mCallback);
        usbTeardownOrder.verify(mNetd).interfaceSetCfg(
                argThat(cfg -> IFACE_NAME.equals(cfg.ifName)));
        usbTeardownOrder.verify(mNetd).tetherInterfaceAdd(IFACE_NAME);

        usbTeardownOrder.verify(mNetd, times(2)).interfaceSetCfg(
                argThat(cfg -> IFACE_NAME.equals(cfg.ifName)));
        usbTeardownOrder.verify(mCallback).updateInterfaceState(
                mIpServer, STATE_AVAILABLE, TETHER_ERROR_TETHER_IFACE_ERROR);
        usbTeardownOrder.verify(mCallback).updateLinkProperties(
                eq(mIpServer), mLinkPropertiesCaptor.capture());
        assertNoAddressesNorRoutes(mLinkPropertiesCaptor.getValue());
    }

    @Test
    public void shouldTearDownUsbOnUpstreamError() throws Exception {
        initTetheredStateMachine(TETHERING_USB, null);

        doThrow(RemoteException.class).when(mNetd).tetherAddForward(anyString(), anyString());
        dispatchTetherConnectionChanged(UPSTREAM_IFACE);
        InOrder usbTeardownOrder = inOrder(mNetd, mCallback);
        usbTeardownOrder.verify(mNetd).tetherAddForward(IFACE_NAME, UPSTREAM_IFACE);

        usbTeardownOrder.verify(mNetd, times(2)).interfaceSetCfg(
                argThat(cfg -> IFACE_NAME.equals(cfg.ifName)));
        usbTeardownOrder.verify(mCallback).updateInterfaceState(
                mIpServer, STATE_AVAILABLE, TETHER_ERROR_ENABLE_FORWARDING_ERROR);
        usbTeardownOrder.verify(mCallback).updateLinkProperties(
                eq(mIpServer), mLinkPropertiesCaptor.capture());
        assertNoAddressesNorRoutes(mLinkPropertiesCaptor.getValue());
    }

    @Test
    public void ignoresDuplicateUpstreamNotifications() throws Exception {
        initTetheredStateMachine(TETHERING_WIFI, UPSTREAM_IFACE);

        verifyNoMoreInteractions(mNetd, mCallback);

        for (int i = 0; i < 5; i++) {
            dispatchTetherConnectionChanged(UPSTREAM_IFACE);
            verifyNoMoreInteractions(mNetd, mCallback);
        }
    }

    @Test
    public void startsDhcpServer() throws Exception {
        initTetheredStateMachine(TETHERING_WIFI, UPSTREAM_IFACE);
        dispatchTetherConnectionChanged(UPSTREAM_IFACE);

        assertDhcpStarted(PrefixUtils.asIpPrefix(mTestAddress));
    }

    @Test
    public void startsDhcpServerOnBluetooth() throws Exception {
        initTetheredStateMachine(TETHERING_BLUETOOTH, UPSTREAM_IFACE);
        dispatchTetherConnectionChanged(UPSTREAM_IFACE);

        assertDhcpStarted(mBluetoothPrefix);
    }

    @Test
    public void startsDhcpServerOnWifiP2p() throws Exception {
        initTetheredStateMachine(TETHERING_WIFI_P2P, UPSTREAM_IFACE);
        dispatchTetherConnectionChanged(UPSTREAM_IFACE);

        assertDhcpStarted(PrefixUtils.asIpPrefix(mTestAddress));
    }

    @Test
    public void startsDhcpServerOnNcm() throws Exception {
        initStateMachine(TETHERING_NCM);
        dispatchCommand(IpServer.CMD_TETHER_REQUESTED, STATE_LOCAL_ONLY);
        dispatchTetherConnectionChanged(UPSTREAM_IFACE);

        assertDhcpStarted(new IpPrefix("192.168.42.0/24"));
    }

    @Test
    public void testOnNewPrefixRequest() throws Exception {
        initStateMachine(TETHERING_NCM);
        dispatchCommand(IpServer.CMD_TETHER_REQUESTED, STATE_LOCAL_ONLY);

        final IDhcpEventCallbacks eventCallbacks;
        final ArgumentCaptor<IDhcpEventCallbacks> dhcpEventCbsCaptor =
                 ArgumentCaptor.forClass(IDhcpEventCallbacks.class);
        verify(mDhcpServer, timeout(MAKE_DHCPSERVER_TIMEOUT_MS).times(1)).startWithCallbacks(
                any(), dhcpEventCbsCaptor.capture());
        eventCallbacks = dhcpEventCbsCaptor.getValue();
        assertDhcpStarted(new IpPrefix("192.168.42.0/24"));

        final ArgumentCaptor<LinkProperties> lpCaptor =
                ArgumentCaptor.forClass(LinkProperties.class);
        InOrder inOrder = inOrder(mNetd, mCallback, mAddressCoordinator);
        inOrder.verify(mAddressCoordinator).requestDownstreamAddress(any());
        inOrder.verify(mNetd).networkAddInterface(INetd.LOCAL_NET_ID, IFACE_NAME);
        // One for ipv4 route, one for ipv6 link local route.
        inOrder.verify(mNetd, times(2)).networkAddRoute(eq(INetd.LOCAL_NET_ID), eq(IFACE_NAME),
                any(), any());
        inOrder.verify(mCallback).updateInterfaceState(
                mIpServer, STATE_LOCAL_ONLY, TETHER_ERROR_NO_ERROR);
        inOrder.verify(mCallback).updateLinkProperties(eq(mIpServer), lpCaptor.capture());
        verifyNoMoreInteractions(mCallback, mAddressCoordinator);

        // Simulate the DHCP server receives DHCPDECLINE on MirrorLink and then signals
        // onNewPrefixRequest callback.
        final LinkAddress newAddress = new LinkAddress("192.168.100.125/24");
        when(mAddressCoordinator.requestDownstreamAddress(any())).thenReturn(newAddress);
        eventCallbacks.onNewPrefixRequest(new IpPrefix("192.168.42.0/24"));
        mLooper.dispatchAll();

        inOrder.verify(mAddressCoordinator).requestDownstreamAddress(any());
        inOrder.verify(mNetd).tetherApplyDnsInterfaces();
        inOrder.verify(mCallback).updateLinkProperties(eq(mIpServer), lpCaptor.capture());
        verifyNoMoreInteractions(mCallback);

        final LinkProperties linkProperties = lpCaptor.getValue();
        final List<LinkAddress> linkAddresses = linkProperties.getLinkAddresses();
        assertEquals(1, linkProperties.getLinkAddresses().size());
        assertEquals(1, linkProperties.getRoutes().size());
        final IpPrefix prefix = new IpPrefix(linkAddresses.get(0).getAddress(),
                linkAddresses.get(0).getPrefixLength());
        assertNotEquals(prefix, new IpPrefix("192.168.42.0/24"));

        verify(mDhcpServer).updateParams(mDhcpParamsCaptor.capture(), any());
        assertDhcpServingParams(mDhcpParamsCaptor.getValue(), prefix);
    }

    @Test
    public void doesNotStartDhcpServerIfDisabled() throws Exception {
        initTetheredStateMachine(TETHERING_WIFI, UPSTREAM_IFACE, true /* usingLegacyDhcp */,
                DEFAULT_USING_BPF_OFFLOAD);
        dispatchTetherConnectionChanged(UPSTREAM_IFACE);

        verify(mDependencies, never()).makeDhcpServer(any(), any(), any());
    }

    private InetAddress addr(String addr) throws Exception {
        return InetAddresses.parseNumericAddress(addr);
    }

    private void recvNewNeigh(int ifindex, InetAddress addr, short nudState, MacAddress mac) {
        mNeighborEventConsumer.accept(new NeighborEvent(0, RTM_NEWNEIGH, ifindex, addr,
                nudState, mac));
        mLooper.dispatchAll();
    }

    private void recvDelNeigh(int ifindex, InetAddress addr, short nudState, MacAddress mac) {
        mNeighborEventConsumer.accept(new NeighborEvent(0, RTM_DELNEIGH, ifindex, addr,
                nudState, mac));
        mLooper.dispatchAll();
    }

    /**
     * Custom ArgumentMatcher for TetherOffloadRuleParcel. This is needed because generated stable
     * AIDL classes don't have equals(), so we cannot just use eq(). A custom assert, such as:
     *
     * private void checkFooCalled(StableParcelable p, ...) {
     *     ArgumentCaptor<FooParam> captor = ArgumentCaptor.forClass(FooParam.class);
     *     verify(mMock).foo(captor.capture());
     *     Foo foo = captor.getValue();
     *     assertFooMatchesExpectations(foo);
     * }
     *
     * almost works, but not quite. This is because if the code under test calls foo() twice, the
     * first call to checkFooCalled() matches both the calls, putting both calls into the captor,
     * and then fails with TooManyActualInvocations. It also makes it harder to use other mockito
     * features such as never(), inOrder(), etc.
     *
     * This approach isn't great because if the match fails, the error message is unhelpful
     * (actual: "android.net.TetherOffloadRuleParcel@8c827b0" or some such), but at least it does
     * work.
     *
     * TODO: consider making the error message more readable by adding a method that catching the
     * AssertionFailedError and throwing a new assertion with more details. See
     * NetworkMonitorTest#verifyNetworkTested.
     *
     * See ConnectivityServiceTest#assertRoutesAdded for an alternative approach which solves the
     * TooManyActualInvocations problem described above by forcing the caller of the custom assert
     * method to specify all expected invocations in one call. This is useful when the stable
     * parcelable class being asserted on has a corresponding Java object (eg., RouteInfo and
     * RouteInfoParcelable), and the caller can just pass in a list of them. It not useful here
     * because there is no such object.
     */
    private static class TetherOffloadRuleParcelMatcher implements
            ArgumentMatcher<TetherOffloadRuleParcel> {
        public final int upstreamIfindex;
        public final InetAddress dst;
        public final MacAddress dstMac;

        TetherOffloadRuleParcelMatcher(int upstreamIfindex, InetAddress dst, MacAddress dstMac) {
            this.upstreamIfindex = upstreamIfindex;
            this.dst = dst;
            this.dstMac = dstMac;
        }

        public boolean matches(TetherOffloadRuleParcel parcel) {
            return upstreamIfindex == parcel.inputInterfaceIndex
                    && (TEST_IFACE_PARAMS.index == parcel.outputInterfaceIndex)
                    && Arrays.equals(dst.getAddress(), parcel.destination)
                    && (128 == parcel.prefixLength)
                    && Arrays.equals(TEST_IFACE_PARAMS.macAddr.toByteArray(), parcel.srcL2Address)
                    && Arrays.equals(dstMac.toByteArray(), parcel.dstL2Address);
        }

        public String toString() {
            return String.format("TetherOffloadRuleParcelMatcher(%d, %s, %s",
                    upstreamIfindex, dst.getHostAddress(), dstMac);
        }
    }

    @NonNull
    private static TetherOffloadRuleParcel matches(
            int upstreamIfindex, InetAddress dst, MacAddress dstMac) {
        return argThat(new TetherOffloadRuleParcelMatcher(upstreamIfindex, dst, dstMac));
    }

    @NonNull
    private static Ipv6ForwardingRule makeForwardingRule(
            int upstreamIfindex, @NonNull InetAddress dst, @NonNull MacAddress dstMac) {
        return new Ipv6ForwardingRule(upstreamIfindex, TEST_IFACE_PARAMS.index,
                (Inet6Address) dst, TEST_IFACE_PARAMS.macAddr, dstMac);
    }

    @NonNull
    private static TetherStatsParcel buildEmptyTetherStatsParcel(int ifIndex) {
        TetherStatsParcel parcel = new TetherStatsParcel();
        parcel.ifIndex = ifIndex;
        return parcel;
    }

    private void resetNetdAndBpfCoordinator() throws Exception {
        reset(mNetd, mBpfCoordinator);
        when(mNetd.tetherOffloadGetStats()).thenReturn(new TetherStatsParcel[0]);
        when(mNetd.tetherOffloadGetAndClearStats(UPSTREAM_IFINDEX))
                .thenReturn(buildEmptyTetherStatsParcel(UPSTREAM_IFINDEX));
        when(mNetd.tetherOffloadGetAndClearStats(UPSTREAM_IFINDEX2))
                .thenReturn(buildEmptyTetherStatsParcel(UPSTREAM_IFINDEX2));
    }

    @Test
    public void addRemoveipv6ForwardingRules() throws Exception {
        initTetheredStateMachine(TETHERING_WIFI, UPSTREAM_IFACE, false /* usingLegacyDhcp */,
                DEFAULT_USING_BPF_OFFLOAD);

        final int myIfindex = TEST_IFACE_PARAMS.index;
        final int notMyIfindex = myIfindex - 1;

        final MacAddress myMac = TEST_IFACE_PARAMS.macAddr;
        final InetAddress neighA = InetAddresses.parseNumericAddress("2001:db8::1");
        final InetAddress neighB = InetAddresses.parseNumericAddress("2001:db8::2");
        final InetAddress neighLL = InetAddresses.parseNumericAddress("fe80::1");
        final InetAddress neighMC = InetAddresses.parseNumericAddress("ff02::1234");
        final MacAddress macNull = MacAddress.fromString("00:00:00:00:00:00");
        final MacAddress macA = MacAddress.fromString("00:00:00:00:00:0a");
        final MacAddress macB = MacAddress.fromString("11:22:33:00:00:0b");

        resetNetdAndBpfCoordinator();
        verifyNoMoreInteractions(mBpfCoordinator, mNetd);

        // TODO: Perhaps verify the interaction of tetherOffloadSetInterfaceQuota and
        // tetherOffloadGetAndClearStats in netd while the rules are changed.

        // Events on other interfaces are ignored.
        recvNewNeigh(notMyIfindex, neighA, NUD_REACHABLE, macA);
        verifyNoMoreInteractions(mBpfCoordinator, mNetd);

        // Events on this interface are received and sent to netd.
        recvNewNeigh(myIfindex, neighA, NUD_REACHABLE, macA);
        verify(mBpfCoordinator).tetherOffloadRuleAdd(
                mIpServer, makeForwardingRule(UPSTREAM_IFINDEX, neighA, macA));
        verify(mNetd).tetherOffloadRuleAdd(matches(UPSTREAM_IFINDEX, neighA, macA));
        resetNetdAndBpfCoordinator();

        recvNewNeigh(myIfindex, neighB, NUD_REACHABLE, macB);
        verify(mBpfCoordinator).tetherOffloadRuleAdd(
                mIpServer, makeForwardingRule(UPSTREAM_IFINDEX, neighB, macB));
        verify(mNetd).tetherOffloadRuleAdd(matches(UPSTREAM_IFINDEX, neighB, macB));
        resetNetdAndBpfCoordinator();

        // Link-local and multicast neighbors are ignored.
        recvNewNeigh(myIfindex, neighLL, NUD_REACHABLE, macA);
        verifyNoMoreInteractions(mBpfCoordinator, mNetd);
        recvNewNeigh(myIfindex, neighMC, NUD_REACHABLE, macA);
        verifyNoMoreInteractions(mBpfCoordinator, mNetd);

        // A neighbor that is no longer valid causes the rule to be removed.
        // NUD_FAILED events do not have a MAC address.
        recvNewNeigh(myIfindex, neighA, NUD_FAILED, null);
        verify(mBpfCoordinator).tetherOffloadRuleRemove(
                mIpServer, makeForwardingRule(UPSTREAM_IFINDEX, neighA, macNull));
        verify(mNetd).tetherOffloadRuleRemove(matches(UPSTREAM_IFINDEX, neighA, macNull));
        resetNetdAndBpfCoordinator();

        // A neighbor that is deleted causes the rule to be removed.
        recvDelNeigh(myIfindex, neighB, NUD_STALE, macB);
        verify(mBpfCoordinator).tetherOffloadRuleRemove(
                mIpServer,  makeForwardingRule(UPSTREAM_IFINDEX, neighB, macNull));
        verify(mNetd).tetherOffloadRuleRemove(matches(UPSTREAM_IFINDEX, neighB, macNull));
        resetNetdAndBpfCoordinator();

        // Upstream changes result in updating the rules.
        recvNewNeigh(myIfindex, neighA, NUD_REACHABLE, macA);
        recvNewNeigh(myIfindex, neighB, NUD_REACHABLE, macB);
        resetNetdAndBpfCoordinator();

        InOrder inOrder = inOrder(mNetd);
        LinkProperties lp = new LinkProperties();
        lp.setInterfaceName(UPSTREAM_IFACE2);
        dispatchTetherConnectionChanged(UPSTREAM_IFACE2, lp, -1);
        verify(mBpfCoordinator).tetherOffloadRuleUpdate(mIpServer, UPSTREAM_IFINDEX2);
        inOrder.verify(mNetd).tetherOffloadRuleRemove(matches(UPSTREAM_IFINDEX, neighA, macA));
        inOrder.verify(mNetd).tetherOffloadRuleAdd(matches(UPSTREAM_IFINDEX2, neighA, macA));
        inOrder.verify(mNetd).tetherOffloadRuleRemove(matches(UPSTREAM_IFINDEX, neighB, macB));
        inOrder.verify(mNetd).tetherOffloadRuleAdd(matches(UPSTREAM_IFINDEX2, neighB, macB));
        resetNetdAndBpfCoordinator();

        // When the upstream is lost, rules are removed.
        dispatchTetherConnectionChanged(null, null, 0);
        // Clear function is called two times by:
        // - processMessage CMD_TETHER_CONNECTION_CHANGED for the upstream is lost.
        // - processMessage CMD_IPV6_TETHER_UPDATE for the IPv6 upstream is lost.
        // See dispatchTetherConnectionChanged.
        verify(mBpfCoordinator, times(2)).tetherOffloadRuleClear(mIpServer);
        verify(mNetd).tetherOffloadRuleRemove(matches(UPSTREAM_IFINDEX2, neighA, macA));
        verify(mNetd).tetherOffloadRuleRemove(matches(UPSTREAM_IFINDEX2, neighB, macB));
        resetNetdAndBpfCoordinator();

        // If the upstream is IPv4-only, no rules are added.
        dispatchTetherConnectionChanged(UPSTREAM_IFACE);
        resetNetdAndBpfCoordinator();
        recvNewNeigh(myIfindex, neighA, NUD_REACHABLE, macA);
        // Clear function is called by #updateIpv6ForwardingRules for the IPv6 upstream is lost.
        verify(mBpfCoordinator).tetherOffloadRuleClear(mIpServer);
        verifyNoMoreInteractions(mBpfCoordinator, mNetd);

        // Rules can be added again once upstream IPv6 connectivity is available.
        lp.setInterfaceName(UPSTREAM_IFACE);
        dispatchTetherConnectionChanged(UPSTREAM_IFACE, lp, -1);
        recvNewNeigh(myIfindex, neighB, NUD_REACHABLE, macB);
        verify(mBpfCoordinator).tetherOffloadRuleAdd(
                mIpServer, makeForwardingRule(UPSTREAM_IFINDEX, neighB, macB));
        verify(mNetd).tetherOffloadRuleAdd(matches(UPSTREAM_IFINDEX, neighB, macB));
        verify(mBpfCoordinator, never()).tetherOffloadRuleAdd(
                mIpServer, makeForwardingRule(UPSTREAM_IFINDEX, neighA, macA));
        verify(mNetd, never()).tetherOffloadRuleAdd(matches(UPSTREAM_IFINDEX, neighA, macA));

        // If upstream IPv6 connectivity is lost, rules are removed.
        resetNetdAndBpfCoordinator();
        dispatchTetherConnectionChanged(UPSTREAM_IFACE, null, 0);
        verify(mBpfCoordinator).tetherOffloadRuleClear(mIpServer);
        verify(mNetd).tetherOffloadRuleRemove(matches(UPSTREAM_IFINDEX, neighB, macB));

        // When the interface goes down, rules are removed.
        lp.setInterfaceName(UPSTREAM_IFACE);
        dispatchTetherConnectionChanged(UPSTREAM_IFACE, lp, -1);
        recvNewNeigh(myIfindex, neighA, NUD_REACHABLE, macA);
        recvNewNeigh(myIfindex, neighB, NUD_REACHABLE, macB);
        verify(mBpfCoordinator).tetherOffloadRuleAdd(
                mIpServer, makeForwardingRule(UPSTREAM_IFINDEX, neighA, macA));
        verify(mNetd).tetherOffloadRuleAdd(matches(UPSTREAM_IFINDEX, neighA, macA));
        verify(mBpfCoordinator).tetherOffloadRuleAdd(
                mIpServer, makeForwardingRule(UPSTREAM_IFINDEX, neighB, macB));
        verify(mNetd).tetherOffloadRuleAdd(matches(UPSTREAM_IFINDEX, neighB, macB));
        resetNetdAndBpfCoordinator();

        mIpServer.stop();
        mLooper.dispatchAll();
        verify(mBpfCoordinator).tetherOffloadRuleClear(mIpServer);
        verify(mNetd).tetherOffloadRuleRemove(matches(UPSTREAM_IFINDEX, neighA, macA));
        verify(mNetd).tetherOffloadRuleRemove(matches(UPSTREAM_IFINDEX, neighB, macB));
        verify(mIpNeighborMonitor).stop();
        resetNetdAndBpfCoordinator();
    }

    @Test
    public void enableDisableUsingBpfOffload() throws Exception {
        final int myIfindex = TEST_IFACE_PARAMS.index;
        final InetAddress neigh = InetAddresses.parseNumericAddress("2001:db8::1");
        final MacAddress macA = MacAddress.fromString("00:00:00:00:00:0a");
        final MacAddress macNull = MacAddress.fromString("00:00:00:00:00:00");

        // Expect that rules can be only added/removed when the BPF offload config is enabled.
        // Note that the BPF offload disabled case is not a realistic test case. Because IP
        // neighbor monitor doesn't start if BPF offload is disabled, there should have no
        // neighbor event listening. This is used for testing the protection check just in case.
        // TODO: Perhaps remove the BPF offload disabled case test once this check isn't needed
        // anymore.

        // [1] Enable BPF offload.
        // A neighbor that is added or deleted causes the rule to be added or removed.
        initTetheredStateMachine(TETHERING_WIFI, UPSTREAM_IFACE, false /* usingLegacyDhcp */,
                true /* usingBpfOffload */);
        resetNetdAndBpfCoordinator();

        recvNewNeigh(myIfindex, neigh, NUD_REACHABLE, macA);
        verify(mBpfCoordinator).tetherOffloadRuleAdd(
                mIpServer, makeForwardingRule(UPSTREAM_IFINDEX, neigh, macA));
        verify(mNetd).tetherOffloadRuleAdd(matches(UPSTREAM_IFINDEX, neigh, macA));
        resetNetdAndBpfCoordinator();

        recvDelNeigh(myIfindex, neigh, NUD_STALE, macA);
        verify(mBpfCoordinator).tetherOffloadRuleRemove(
                mIpServer, makeForwardingRule(UPSTREAM_IFINDEX, neigh, macNull));
        verify(mNetd).tetherOffloadRuleRemove(matches(UPSTREAM_IFINDEX, neigh, macNull));
        resetNetdAndBpfCoordinator();

        // [2] Disable BPF offload.
        // A neighbor that is added or deleted doesn’t cause the rule to be added or removed.
        initTetheredStateMachine(TETHERING_WIFI, UPSTREAM_IFACE, false /* usingLegacyDhcp */,
                false /* usingBpfOffload */);
        resetNetdAndBpfCoordinator();

        recvNewNeigh(myIfindex, neigh, NUD_REACHABLE, macA);
        verify(mBpfCoordinator, never()).tetherOffloadRuleAdd(any(), any());
        verify(mNetd, never()).tetherOffloadRuleAdd(any());
        resetNetdAndBpfCoordinator();

        recvDelNeigh(myIfindex, neigh, NUD_STALE, macA);
        verify(mBpfCoordinator, never()).tetherOffloadRuleRemove(any(), any());
        verify(mNetd, never()).tetherOffloadRuleRemove(any());
        resetNetdAndBpfCoordinator();
    }

    @Test
    public void doesNotStartIpNeighborMonitorIfBpfOffloadDisabled() throws Exception {
        initTetheredStateMachine(TETHERING_WIFI, UPSTREAM_IFACE, false /* usingLegacyDhcp */,
                false /* usingBpfOffload */);

        // IP neighbor monitor doesn't start if BPF offload is disabled.
        verify(mIpNeighborMonitor, never()).start();
    }

    private LinkProperties buildIpv6OnlyLinkProperties(final String iface) {
        final LinkProperties linkProp = new LinkProperties();
        linkProp.setInterfaceName(iface);
        linkProp.addLinkAddress(new LinkAddress("2001:db8::1/64"));
        linkProp.addRoute(new RouteInfo(new IpPrefix("::/0"), null, iface, RTN_UNICAST));
        final InetAddress dns = InetAddresses.parseNumericAddress("2001:4860:4860::8888");
        linkProp.addDnsServer(dns);

        return linkProp;
    }

    @Test
    public void testAdjustTtlValue() throws Exception {
        final ArgumentCaptor<RaParams> raParamsCaptor =
                ArgumentCaptor.forClass(RaParams.class);
        initTetheredStateMachine(TETHERING_WIFI, UPSTREAM_IFACE);
        verify(mRaDaemon).buildNewRa(any(), raParamsCaptor.capture());
        final RaParams noV6Params = raParamsCaptor.getValue();
        assertEquals(65, noV6Params.hopLimit);
        reset(mRaDaemon);

        when(mNetd.getProcSysNet(
                INetd.IPV6, INetd.CONF, UPSTREAM_IFACE, "hop_limit")).thenReturn("64");
        final LinkProperties lp = buildIpv6OnlyLinkProperties(UPSTREAM_IFACE);
        dispatchTetherConnectionChanged(UPSTREAM_IFACE, lp, 1);
        verify(mRaDaemon).buildNewRa(any(), raParamsCaptor.capture());
        final RaParams nonCellularParams = raParamsCaptor.getValue();
        assertEquals(65, nonCellularParams.hopLimit);
        reset(mRaDaemon);

        dispatchTetherConnectionChanged(UPSTREAM_IFACE, null, 0);
        verify(mRaDaemon).buildNewRa(any(), raParamsCaptor.capture());
        final RaParams noUpstream = raParamsCaptor.getValue();
        assertEquals(65, nonCellularParams.hopLimit);
        reset(mRaDaemon);

        dispatchTetherConnectionChanged(UPSTREAM_IFACE, lp, -1);
        verify(mRaDaemon).buildNewRa(any(), raParamsCaptor.capture());
        final RaParams cellularParams = raParamsCaptor.getValue();
        assertEquals(63, cellularParams.hopLimit);
        reset(mRaDaemon);
    }

    private void assertDhcpServingParams(final DhcpServingParamsParcel params,
            final IpPrefix prefix) {
        // Last address byte is random
        assertTrue(prefix.contains(intToInet4AddressHTH(params.serverAddr)));
        assertEquals(prefix.getPrefixLength(), params.serverAddrPrefixLength);
        assertEquals(1, params.defaultRouters.length);
        assertEquals(params.serverAddr, params.defaultRouters[0]);
        assertEquals(1, params.dnsServers.length);
        assertEquals(params.serverAddr, params.dnsServers[0]);
        assertEquals(DHCP_LEASE_TIME_SECS, params.dhcpLeaseTimeSecs);
        if (mIpServer.interfaceType() == TETHERING_NCM) {
            assertTrue(params.changePrefixOnDecline);
        }
    }

    private void assertDhcpStarted(IpPrefix expectedPrefix) throws Exception {
        verify(mDependencies, times(1)).makeDhcpServer(eq(IFACE_NAME), any(), any());
        verify(mDhcpServer, timeout(MAKE_DHCPSERVER_TIMEOUT_MS).times(1)).startWithCallbacks(
                any(), any());
        assertDhcpServingParams(mDhcpParamsCaptor.getValue(), expectedPrefix);
    }

    /**
     * Send a command to the state machine under test, and run the event loop to idle.
     *
     * @param command One of the IpServer.CMD_* constants.
     * @param arg1 An additional argument to pass.
     */
    private void dispatchCommand(int command, int arg1) {
        mIpServer.sendMessage(command, arg1);
        mLooper.dispatchAll();
    }

    /**
     * Send a command to the state machine under test, and run the event loop to idle.
     *
     * @param command One of the IpServer.CMD_* constants.
     */
    private void dispatchCommand(int command) {
        mIpServer.sendMessage(command);
        mLooper.dispatchAll();
    }

    /**
     * Special override to tell the state machine that the upstream interface has changed.
     *
     * @see #dispatchCommand(int)
     * @param upstreamIface String name of upstream interface (or null)
     * @param v6lp IPv6 LinkProperties of the upstream interface, or null for an IPv4-only upstream.
     */
    private void dispatchTetherConnectionChanged(String upstreamIface, LinkProperties v6lp,
            int ttlAdjustment) {
        dispatchTetherConnectionChanged(upstreamIface);
        mIpServer.sendMessage(IpServer.CMD_IPV6_TETHER_UPDATE, ttlAdjustment, 0, v6lp);
        mLooper.dispatchAll();
    }

    private void dispatchTetherConnectionChanged(String upstreamIface) {
        final InterfaceSet ifs = (upstreamIface != null) ? new InterfaceSet(upstreamIface) : null;
        mIpServer.sendMessage(IpServer.CMD_TETHER_CONNECTION_CHANGED, ifs);
        mLooper.dispatchAll();
    }

    private void assertIPv4AddressAndDirectlyConnectedRoute(LinkProperties lp) {
        // Find the first IPv4 LinkAddress.
        LinkAddress addr4 = null;
        for (LinkAddress addr : lp.getLinkAddresses()) {
            if (!(addr.getAddress() instanceof Inet4Address)) continue;
            addr4 = addr;
            break;
        }
        assertNotNull("missing IPv4 address", addr4);

        final IpPrefix destination = new IpPrefix(addr4.getAddress(), addr4.getPrefixLength());
        // Assert the presence of the associated directly connected route.
        final RouteInfo directlyConnected = new RouteInfo(destination, null, lp.getInterfaceName(),
                RouteInfo.RTN_UNICAST);
        assertTrue("missing directly connected route: '" + directlyConnected.toString() + "'",
                   lp.getRoutes().contains(directlyConnected));
    }

    private void assertNoAddressesNorRoutes(LinkProperties lp) {
        assertTrue(lp.getLinkAddresses().isEmpty());
        assertTrue(lp.getRoutes().isEmpty());
        // We also check that interface name is non-empty, because we should
        // never see an empty interface name in any LinkProperties update.
        assertFalse(TextUtils.isEmpty(lp.getInterfaceName()));
    }

    private boolean assertContainsFlag(String[] flags, String match) {
        for (String flag : flags) {
            if (flag.equals(match)) return true;
        }
        fail("Missing flag: " + match);
        return false;
    }

    private boolean assertNotContainsFlag(String[] flags, String match) {
        for (String flag : flags) {
            if (flag.equals(match)) {
                fail("Unexpected flag: " + match);
                return false;
            }
        }
        return true;
    }
}
