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

package android.net.dhcp;

import static android.net.InetAddresses.parseNumericAddress;
import static android.net.dhcp.DhcpPacket.DHCP_HOST_NAME;
import static android.net.dhcp.DhcpPacket.ENCAP_BOOTP;
import static android.net.dhcp.DhcpPacket.INADDR_ANY;
import static android.net.dhcp.DhcpPacket.INADDR_BROADCAST;
import static android.net.dhcp.DhcpServer.CMD_RECEIVE_PACKET;
import static android.net.dhcp.IDhcpServer.STATUS_SUCCESS;
import static android.net.util.NetworkStackUtils.DHCP_RAPID_COMMIT_VERSION;

import static com.android.net.module.util.Inet4AddressUtils.inet4AddressToIntHTH;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertTrue;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.Context;
import android.net.INetworkStackStatusCallback;
import android.net.IpPrefix;
import android.net.LinkAddress;
import android.net.MacAddress;
import android.net.dhcp.DhcpLeaseRepository.InvalidAddressException;
import android.net.dhcp.DhcpLeaseRepository.OutOfAddressesException;
import android.net.dhcp.DhcpServer.Clock;
import android.net.dhcp.DhcpServer.Dependencies;
import android.net.util.SharedLog;
import android.os.ConditionVariable;
import android.testing.AndroidTestingRunner;

import androidx.test.filters.SmallTest;

import com.android.net.module.util.Inet4AddressUtils;
import com.android.testutils.HandlerUtilsKt;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.net.Inet4Address;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

@RunWith(AndroidTestingRunner.class)
@SmallTest
public class DhcpServerTest {
    private static final String TEST_IFACE = "testiface";

    private static final Inet4Address TEST_SERVER_ADDR = parseAddr("192.168.0.2");
    private static final int TEST_PREFIX_LENGTH = 20;
    private static final LinkAddress TEST_SERVER_LINKADDR = new LinkAddress(
            TEST_SERVER_ADDR, TEST_PREFIX_LENGTH);
    private static final Set<Inet4Address> TEST_DEFAULT_ROUTERS = new HashSet<>(
            Arrays.asList(parseAddr("192.168.0.123"), parseAddr("192.168.0.124")));
    private static final Set<Inet4Address> TEST_DNS_SERVERS = new HashSet<>(
            Arrays.asList(parseAddr("192.168.0.126"), parseAddr("192.168.0.127")));
    private static final Set<Inet4Address> TEST_EXCLUDED_ADDRS = new HashSet<>(
            Arrays.asList(parseAddr("192.168.0.200"), parseAddr("192.168.0.201")));
    private static final long TEST_LEASE_TIME_SECS = 3600L;
    private static final int TEST_MTU = 1500;
    private static final String TEST_HOSTNAME = "testhostname";

    private static final int TEST_TRANSACTION_ID = 123;
    private static final byte[] TEST_CLIENT_MAC_BYTES = new byte [] { 1, 2, 3, 4, 5, 6 };
    private static final MacAddress TEST_CLIENT_MAC = MacAddress.fromBytes(TEST_CLIENT_MAC_BYTES);
    private static final Inet4Address TEST_CLIENT_ADDR = parseAddr("192.168.0.42");

    private static final long TEST_CLOCK_TIME = 1234L;
    private static final int TEST_LEASE_EXPTIME_SECS = 3600;
    private static final DhcpLease TEST_LEASE = new DhcpLease(null, TEST_CLIENT_MAC,
            TEST_CLIENT_ADDR, TEST_PREFIX_LENGTH, TEST_LEASE_EXPTIME_SECS * 1000L + TEST_CLOCK_TIME,
            null /* hostname */);
    private static final DhcpLease TEST_LEASE_WITH_HOSTNAME = new DhcpLease(null, TEST_CLIENT_MAC,
            TEST_CLIENT_ADDR, TEST_PREFIX_LENGTH, TEST_LEASE_EXPTIME_SECS * 1000L + TEST_CLOCK_TIME,
            TEST_HOSTNAME);
    private static final int TEST_TIMEOUT_MS = 10000;

    @NonNull @Mock
    private Context mContext;
    @NonNull @Mock
    private Dependencies mDeps;
    @NonNull @Mock
    private DhcpLeaseRepository mRepository;
    @NonNull @Mock
    private Clock mClock;
    @NonNull @Mock
    private DhcpPacketListener mPacketListener;
    @NonNull @Mock
    private IDhcpEventCallbacks mEventCallbacks;

    @NonNull @Captor
    private ArgumentCaptor<ByteBuffer> mSentPacketCaptor;
    @NonNull @Captor
    private ArgumentCaptor<Inet4Address> mResponseDstAddrCaptor;

    @NonNull
    private MyDhcpServer mServer;

    @Nullable
    private String mPrevShareClassloaderProp;

    private class MyDhcpServer extends DhcpServer {
        private final ConditionVariable mCv = new ConditionVariable(false);

        MyDhcpServer(Context context, String ifName, DhcpServingParams params, SharedLog log,
                Dependencies deps) {
            super(context, ifName, params, log, deps);
        }

        @Override
        protected void onQuitting() {
            super.onQuitting();
            mCv.open();
        }

        public void waitForShutdown() {
            assertTrue(mCv.block(TEST_TIMEOUT_MS));
        }
    }

    private final INetworkStackStatusCallback mAssertSuccessCallback =
            new INetworkStackStatusCallback.Stub() {
        @Override
        public void onStatusAvailable(int statusCode) {
            assertEquals(STATUS_SUCCESS, statusCode);
        }

        @Override
        public int getInterfaceVersion() {
            return this.VERSION;
        }

        @Override
        public String getInterfaceHash() {
            return this.HASH;
        }
    };

    private DhcpServingParams makeServingParams() throws Exception {
        return new DhcpServingParams.Builder()
                .setDefaultRouters(TEST_DEFAULT_ROUTERS)
                .setDhcpLeaseTimeSecs(TEST_LEASE_TIME_SECS)
                .setDnsServers(TEST_DNS_SERVERS)
                .setServerAddr(TEST_SERVER_LINKADDR)
                .setLinkMtu(TEST_MTU)
                .setExcludedAddrs(TEST_EXCLUDED_ADDRS)
                .setChangePrefixOnDecline(false)
                .build();
    }

    private void startServer() throws Exception {
        mServer.start(mAssertSuccessCallback);
        HandlerUtilsKt.waitForIdle(mServer.getHandler(), TEST_TIMEOUT_MS);
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        when(mDeps.makeLeaseRepository(any(), any(), any())).thenReturn(mRepository);
        when(mDeps.makeClock()).thenReturn(mClock);
        when(mDeps.makePacketListener(any())).thenReturn(mPacketListener);
        when(mDeps.isFeatureEnabled(eq(mContext), eq(DHCP_RAPID_COMMIT_VERSION))).thenReturn(true);
        doNothing().when(mDeps)
                .sendPacket(any(), mSentPacketCaptor.capture(), mResponseDstAddrCaptor.capture());
        when(mClock.elapsedRealtime()).thenReturn(TEST_CLOCK_TIME);
        when(mPacketListener.start()).thenReturn(true);

        mServer = new MyDhcpServer(mContext, TEST_IFACE, makeServingParams(),
                new SharedLog(DhcpServerTest.class.getSimpleName()), mDeps);
    }

    @After
    public void tearDown() throws Exception {
        verify(mRepository, never()).addLeaseCallbacks(eq(null));
        mServer.stop(mAssertSuccessCallback);
        mServer.waitForShutdown();
        verify(mPacketListener, times(1)).stop();
    }

    @Test
    public void testStart() throws Exception {
        startServer();

        verify(mPacketListener, times(1)).start();
    }

    @Test
    public void testStartWithCallbacks() throws Exception {
        mServer.start(mAssertSuccessCallback, mEventCallbacks);
        HandlerUtilsKt.waitForIdle(mServer.getHandler(), TEST_TIMEOUT_MS);
        verify(mRepository).addLeaseCallbacks(eq(mEventCallbacks));
    }

    @Test
    public void testDiscover() throws Exception {
        startServer();

        // TODO: refactor packet construction to eliminate unnecessary/confusing/duplicate fields
        when(mRepository.getOffer(isNull() /* clientId */, eq(TEST_CLIENT_MAC),
                eq(INADDR_ANY) /* relayAddr */, isNull() /* reqAddr */, isNull() /* hostname */))
                .thenReturn(TEST_LEASE);

        final DhcpDiscoverPacket discover = new DhcpDiscoverPacket(TEST_TRANSACTION_ID,
                (short) 0 /* secs */, INADDR_ANY /* relayIp */, TEST_CLIENT_MAC_BYTES,
                false /* broadcast */, INADDR_ANY /* srcIp */, false /* rapidCommit */);
        mServer.sendMessage(CMD_RECEIVE_PACKET, discover);
        HandlerUtilsKt.waitForIdle(mServer.getHandler(), TEST_TIMEOUT_MS);

        assertResponseSentTo(TEST_CLIENT_ADDR);
        final DhcpOfferPacket packet = assertOffer(getPacket());
        assertMatchesTestLease(packet);
    }

    @Test
    public void testDiscover_RapidCommit() throws Exception {
        startServer();

        when(mDeps.isFeatureEnabled(eq(mContext), eq(DHCP_RAPID_COMMIT_VERSION))).thenReturn(true);
        when(mRepository.getCommittedLease(isNull() /* clientId */, eq(TEST_CLIENT_MAC),
                eq(INADDR_ANY) /* relayAddr */, isNull() /* hostname */)).thenReturn(TEST_LEASE);

        final DhcpDiscoverPacket discover = new DhcpDiscoverPacket(TEST_TRANSACTION_ID,
                (short) 0 /* secs */, INADDR_ANY /* relayIp */, TEST_CLIENT_MAC_BYTES,
                false /* broadcast */, INADDR_ANY /* srcIp */, true /* rapidCommit */);
        mServer.sendMessage(CMD_RECEIVE_PACKET, discover);
        HandlerUtilsKt.waitForIdle(mServer.getHandler(), TEST_TIMEOUT_MS);

        assertResponseSentTo(TEST_CLIENT_ADDR);
        final DhcpAckPacket packet = assertAck(getPacket());
        assertMatchesTestLease(packet);
    }

    @Test
    public void testDiscover_OutOfAddresses() throws Exception {
        startServer();

        when(mRepository.getOffer(isNull() /* clientId */, eq(TEST_CLIENT_MAC),
                eq(INADDR_ANY) /* relayAddr */, isNull() /* reqAddr */, isNull() /* hostname */))
                .thenThrow(new OutOfAddressesException("Test exception"));

        final DhcpDiscoverPacket discover = new DhcpDiscoverPacket(TEST_TRANSACTION_ID,
                (short) 0 /* secs */, INADDR_ANY /* relayIp */, TEST_CLIENT_MAC_BYTES,
                false /* broadcast */, INADDR_ANY /* srcIp */, false /* rapidCommit */);
        mServer.sendMessage(CMD_RECEIVE_PACKET, discover);
        HandlerUtilsKt.waitForIdle(mServer.getHandler(), TEST_TIMEOUT_MS);

        assertResponseSentTo(INADDR_BROADCAST);
        final DhcpNakPacket packet = assertNak(getPacket());
        assertMatchesClient(packet);
    }

    private DhcpRequestPacket makeRequestSelectingPacket() {
        final DhcpRequestPacket request = new DhcpRequestPacket(TEST_TRANSACTION_ID,
                (short) 0 /* secs */, INADDR_ANY /* clientIp */, INADDR_ANY /* relayIp */,
                TEST_CLIENT_MAC_BYTES, false /* broadcast */);
        request.mServerIdentifier = TEST_SERVER_ADDR;
        request.mRequestedIp = TEST_CLIENT_ADDR;
        return request;
    }

    @Test
    public void testRequest_Selecting_Ack() throws Exception {
        startServer();

        when(mRepository.requestLease(isNull() /* clientId */, eq(TEST_CLIENT_MAC),
                eq(INADDR_ANY) /* clientAddr */, eq(INADDR_ANY) /* relayAddr */,
                eq(TEST_CLIENT_ADDR) /* reqAddr */, eq(true) /* sidSet */, eq(TEST_HOSTNAME)))
                .thenReturn(TEST_LEASE_WITH_HOSTNAME);

        final DhcpRequestPacket request = makeRequestSelectingPacket();
        request.mHostName = TEST_HOSTNAME;
        request.mRequestedParams = new byte[] { DHCP_HOST_NAME };
        mServer.sendMessage(CMD_RECEIVE_PACKET, request);
        HandlerUtilsKt.waitForIdle(mServer.getHandler(), TEST_TIMEOUT_MS);

        assertResponseSentTo(TEST_CLIENT_ADDR);
        final DhcpAckPacket packet = assertAck(getPacket());
        assertMatchesTestLease(packet, TEST_HOSTNAME);
    }

    @Test
    public void testRequest_Selecting_Nak() throws Exception {
        startServer();

        when(mRepository.requestLease(isNull(), eq(TEST_CLIENT_MAC),
                eq(INADDR_ANY) /* clientAddr */, eq(INADDR_ANY) /* relayAddr */,
                eq(TEST_CLIENT_ADDR) /* reqAddr */, eq(true) /* sidSet */, isNull() /* hostname */))
                .thenThrow(new InvalidAddressException("Test error"));

        final DhcpRequestPacket request = makeRequestSelectingPacket();
        mServer.sendMessage(CMD_RECEIVE_PACKET, request);
        HandlerUtilsKt.waitForIdle(mServer.getHandler(), TEST_TIMEOUT_MS);

        assertResponseSentTo(INADDR_BROADCAST);
        final DhcpNakPacket packet = assertNak(getPacket());
        assertMatchesClient(packet);
    }

    @Test
    public void testRelease() throws Exception {
        startServer();

        final DhcpReleasePacket release = new DhcpReleasePacket(TEST_TRANSACTION_ID,
                TEST_SERVER_ADDR, TEST_CLIENT_ADDR,
                INADDR_ANY /* relayIp */, TEST_CLIENT_MAC_BYTES);
        mServer.sendMessage(CMD_RECEIVE_PACKET, release);
        HandlerUtilsKt.waitForIdle(mServer.getHandler(), TEST_TIMEOUT_MS);

        verify(mRepository, times(1))
                .releaseLease(isNull(), eq(TEST_CLIENT_MAC), eq(TEST_CLIENT_ADDR));
    }

    @Test
    public void testDecline_LeaseDoesNotExist() throws Exception {
        when(mRepository.markAndReleaseDeclinedLease(isNull(), eq(TEST_CLIENT_MAC),
                eq(TEST_CLIENT_ADDR))).thenReturn(false);

        startServer();
        runOnReceivedDeclinePacket();
        verify(mEventCallbacks, never()).onNewPrefixRequest(any());
    }

    private void runOnReceivedDeclinePacket() throws Exception {
        when(mRepository.getCommittedLeases()).thenReturn(
                Arrays.asList(new DhcpLease(null, TEST_CLIENT_MAC, TEST_CLIENT_ADDR,
                        TEST_PREFIX_LENGTH, TEST_LEASE_EXPTIME_SECS * 1000L + TEST_CLOCK_TIME,
                        TEST_HOSTNAME)));
        final DhcpDeclinePacket decline = new DhcpDeclinePacket(TEST_TRANSACTION_ID,
                (short) 0 /* secs */, INADDR_ANY /* clientIp */, INADDR_ANY /* yourIp */,
                INADDR_ANY /* nextIp */, INADDR_ANY /* relayIp */, TEST_CLIENT_MAC_BYTES,
                TEST_CLIENT_ADDR /* requestedIp */, TEST_SERVER_ADDR /* serverIdentifier */);
        mServer.sendMessage(CMD_RECEIVE_PACKET, decline);
        HandlerUtilsKt.waitForIdle(mServer.getHandler(), TEST_TIMEOUT_MS);

        verify(mRepository).markAndReleaseDeclinedLease(isNull(), eq(TEST_CLIENT_MAC),
                eq(TEST_CLIENT_ADDR));
    }

    private int[] toIntArray(@NonNull Collection<Inet4Address> addrs) {
        return addrs.stream().mapToInt(Inet4AddressUtils::inet4AddressToIntHTH).toArray();
    }

    private void updateServingParams(Set<Inet4Address> defaultRouters,
            Set<Inet4Address> dnsServers, Set<Inet4Address> excludedAddrs, LinkAddress serverAddr,
            boolean changePrefixOnDecline) throws Exception {
        final DhcpServingParamsParcel params = new DhcpServingParamsParcel();
        params.serverAddr = inet4AddressToIntHTH((Inet4Address) serverAddr.getAddress());
        params.serverAddrPrefixLength = serverAddr.getPrefixLength();
        params.defaultRouters = toIntArray(defaultRouters);
        params.dnsServers = toIntArray(dnsServers);
        params.excludedAddrs = toIntArray(excludedAddrs);
        params.dhcpLeaseTimeSecs = TEST_LEASE_EXPTIME_SECS;
        params.linkMtu = TEST_MTU;
        params.metered = true;
        params.changePrefixOnDecline = changePrefixOnDecline;

        mServer.updateParams(params, mAssertSuccessCallback);
        HandlerUtilsKt.waitForIdle(mServer.getHandler(), TEST_TIMEOUT_MS);
    }

    @Test
    public void testChangePrefixOnDecline() throws Exception {
        when(mRepository.markAndReleaseDeclinedLease(isNull(), eq(TEST_CLIENT_MAC),
                eq(TEST_CLIENT_ADDR))).thenReturn(true);

        mServer.start(mAssertSuccessCallback, mEventCallbacks);
        HandlerUtilsKt.waitForIdle(mServer.getHandler(), TEST_TIMEOUT_MS);
        verify(mRepository).addLeaseCallbacks(eq(mEventCallbacks));

        // Enable changePrefixOnDecline
        updateServingParams(TEST_DEFAULT_ROUTERS, TEST_DNS_SERVERS, TEST_EXCLUDED_ADDRS,
                TEST_SERVER_LINKADDR, true /* changePrefixOnDecline */);

        runOnReceivedDeclinePacket();
        final IpPrefix servingPrefix = DhcpServingParams.makeIpPrefix(TEST_SERVER_LINKADDR);
        verify(mEventCallbacks).onNewPrefixRequest(eq(servingPrefix));

        final Inet4Address serverAddr = parseAddr("192.168.51.129");
        final LinkAddress srvLinkAddr = new LinkAddress(serverAddr, 24);
        final Set<Inet4Address> srvAddr = new HashSet<>(Collections.singletonList(serverAddr));
        final Set<Inet4Address> excludedAddrs = new HashSet<>(
                Arrays.asList(parseAddr("192.168.51.200"), parseAddr("192.168.51.201")));

        // Simulate IpServer updates the serving params with a new prefix.
        updateServingParams(srvAddr, srvAddr, excludedAddrs, srvLinkAddr,
                true /* changePrefixOnDecline */);

        final Inet4Address clientAddr = parseAddr("192.168.51.42");
        final DhcpLease lease = new DhcpLease(null, TEST_CLIENT_MAC,
                clientAddr, 24 /*prefixLen*/, TEST_LEASE_EXPTIME_SECS * 1000L + TEST_CLOCK_TIME,
                null /* hostname */);
        when(mRepository.getOffer(isNull() /* clientId */, eq(TEST_CLIENT_MAC),
                eq(INADDR_ANY) /* relayAddr */, isNull() /* reqAddr */, isNull() /* hostname */))
                .thenReturn(lease);

        // Test discover packet
        final DhcpDiscoverPacket discover = new DhcpDiscoverPacket(TEST_TRANSACTION_ID,
                (short) 0 /* secs */, INADDR_ANY /* relayIp */, TEST_CLIENT_MAC_BYTES,
                false /* broadcast */, INADDR_ANY /* srcIp */, false /* rapidCommit */);
        mServer.sendMessage(CMD_RECEIVE_PACKET, discover);
        HandlerUtilsKt.waitForIdle(mServer.getHandler(), TEST_TIMEOUT_MS);
        assertResponseSentTo(clientAddr);
        final DhcpOfferPacket packet = assertOffer(getPacket());
        assertMatchesLease(packet, serverAddr, clientAddr, null);
    }

    /* TODO: add more tests once packet construction is refactored, including:
     *  - usage of giaddr
     *  - usage of broadcast bit
     *  - other request states (init-reboot/renewing/rebinding)
     */

    private void assertMatchesLease(@NonNull DhcpPacket packet, @NonNull Inet4Address srvAddr,
            @NonNull Inet4Address clientAddr, @Nullable String hostname) {
        assertMatchesClient(packet);
        assertFalse(packet.hasExplicitClientId());
        assertEquals(srvAddr, packet.mServerIdentifier);
        assertEquals(clientAddr, packet.mYourIp);
        assertNotNull(packet.mLeaseTime);
        assertEquals(TEST_LEASE_EXPTIME_SECS, (int) packet.mLeaseTime);
        assertEquals(hostname, packet.mHostName);
    }

    private void assertMatchesTestLease(@NonNull DhcpPacket packet, @Nullable String hostname) {
        assertMatchesLease(packet, TEST_SERVER_ADDR, TEST_CLIENT_ADDR, hostname);
    }

    private void assertMatchesTestLease(@NonNull DhcpPacket packet) {
        assertMatchesTestLease(packet, null);
    }

    private void assertMatchesClient(@NonNull DhcpPacket packet) {
        assertEquals(TEST_TRANSACTION_ID, packet.mTransId);
        assertEquals(TEST_CLIENT_MAC, MacAddress.fromBytes(packet.mClientMac));
    }

    private void assertResponseSentTo(@NonNull Inet4Address addr) {
        assertEquals(addr, mResponseDstAddrCaptor.getValue());
    }

    private static DhcpNakPacket assertNak(@Nullable DhcpPacket packet) {
        assertTrue(packet instanceof DhcpNakPacket);
        return (DhcpNakPacket) packet;
    }

    private static DhcpAckPacket assertAck(@Nullable DhcpPacket packet) {
        assertTrue(packet instanceof DhcpAckPacket);
        return (DhcpAckPacket) packet;
    }

    private static DhcpOfferPacket assertOffer(@Nullable DhcpPacket packet) {
        assertTrue(packet instanceof DhcpOfferPacket);
        return (DhcpOfferPacket) packet;
    }

    private DhcpPacket getPacket() throws Exception {
        verify(mDeps, times(1)).sendPacket(any(), any(), any());
        return DhcpPacket.decodeFullPacket(mSentPacketCaptor.getValue(), ENCAP_BOOTP,
                new byte[0] /* optionsToSkip */);
    }

    private static Inet4Address parseAddr(@Nullable String inet4Addr) {
        return (Inet4Address) parseNumericAddress(inet4Addr);
    }
}
