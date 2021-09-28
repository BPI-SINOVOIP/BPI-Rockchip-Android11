/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.networkstack.netlink;

import static android.net.netlink.NetlinkConstants.SOCKDIAG_MSG_HEADER_SIZE;
import static android.net.util.DataStallUtils.CONFIG_TCP_PACKETS_FAIL_PERCENTAGE;
import static android.net.util.DataStallUtils.DEFAULT_TCP_PACKETS_FAIL_PERCENTAGE;
import static android.provider.DeviceConfig.NAMESPACE_CONNECTIVITY;
import static android.system.OsConstants.AF_INET;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.doReturn;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.mockitoSession;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

import static org.junit.Assume.assumeTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.net.INetd;
import android.net.MarkMaskParcel;
import android.net.Network;
import android.net.netlink.StructNlMsgHdr;
import android.os.Build;
import android.util.Log;
import android.util.Log.TerribleFailureHandler;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.networkstack.apishim.ConstantsShim;
import com.android.networkstack.apishim.NetworkShimImpl;
import com.android.testutils.DevSdkIgnoreRule;
import com.android.testutils.DevSdkIgnoreRule.IgnoreAfter;
import com.android.testutils.DevSdkIgnoreRule.IgnoreUpTo;

import libcore.util.HexEncoding;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.MockitoSession;
import org.mockito.quality.Strictness;

import java.io.FileDescriptor;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

// TODO: Add more tests for missing coverage.
@RunWith(AndroidJUnit4.class)
@SmallTest
public class TcpSocketTrackerTest {
    private static final int TEST_BUFFER_SIZE = 1024;
    private static final String DIAG_MSG_HEX =
            // struct nlmsghdr.
            "58000000" +      // length = 88
            "1400" +         // type = SOCK_DIAG_BY_FAMILY
            "0301" +         // flags = NLM_F_REQUEST | NLM_F_DUMP
            "00000000" +     // seqno
            "00000000" +     // pid (0 == kernel)
            // struct inet_diag_req_v2
            "02" +           // family = AF_INET
            "06" +           // state
            "00" +           // timer
            "00" +           // retrans
            // inet_diag_sockid
            "DEA5" +         // idiag_sport = 42462
            "71B9" +         // idiag_dport = 47473
            "0a006402000000000000000000000000" + // idiag_src = 10.0.100.2
            "08080808000000000000000000000000" + // idiag_dst = 8.8.8.8
            "00000000" +    // idiag_if
            "34ED000076270000" + // idiag_cookie = 43387759684916
            "00000000" +    // idiag_expires
            "00000000" +    // idiag_rqueue
            "00000000" +    // idiag_wqueue
            "00000000" +    // idiag_uid
            "00000000";    // idiag_inode
    private static final byte[] SOCK_DIAG_MSG_BYTES =
            HexEncoding.decode(DIAG_MSG_HEX.toCharArray(), false);
    // Hexadecimal representation of a SOCK_DIAG response with tcp info.
    private static final String SOCK_DIAG_TCP_INET_HEX =
            // struct nlmsghdr.
            "14010000" +        // length = 276
            "1400" +            // type = SOCK_DIAG_BY_FAMILY
            "0301" +            // flags = NLM_F_REQUEST | NLM_F_DUMP
            "00000000" +        // seqno
            "00000000" +        // pid (0 == kernel)
            // struct inet_diag_req_v2
            "02" +              // family = AF_INET
            "06" +              // state
            "00" +              // timer
            "00" +              // retrans
            // inet_diag_sockid
            "DEA5" +            // idiag_sport = 42462
            "71B9" +            // idiag_dport = 47473
            "0a006402000000000000000000000000" + // idiag_src = 10.0.100.2
            "08080808000000000000000000000000" + // idiag_dst = 8.8.8.8
            "00000000" +            // idiag_if
            "34ED000076270000" +    // idiag_cookie = 43387759684916
            "00000000" +            // idiag_expires
            "00000000" +            // idiag_rqueue
            "00000000" +            // idiag_wqueue
            "00000000" +            // idiag_uid
            "00000000" +            // idiag_inode
            // rtattr
            "0500" +            // len = 5
            "0800" +            // type = 8
            "00000000" +        // data
            "0800" +            // len = 8
            "0F00" +            // type = 15(INET_DIAG_MARK)
            "850A0C00" +        // data, socket mark=789125
            "AC00" +            // len = 172
            "0200" +            // type = 2(INET_DIAG_INFO)
            // tcp_info
            "01" +              // state = TCP_ESTABLISHED
            "00" +              // ca_state = TCP_CA_OPEN
            "05" +              // retransmits = 5
            "00" +              // probes = 0
            "00" +              // backoff = 0
            "07" +              // option = TCPI_OPT_WSCALE|TCPI_OPT_SACK|TCPI_OPT_TIMESTAMPS
            "88" +              // wscale = 8
            "00" +              // delivery_rate_app_limited = 0
            "4A911B00" +        // rto = 1806666
            "00000000" +        // ato = 0
            "2E050000" +        // sndMss = 1326
            "18020000" +        // rcvMss = 536
            "00000000" +        // unsacked = 0
            "00000000" +        // acked = 0
            "00000000" +        // lost = 0
            "00000000" +        // retrans = 0
            "00000000" +        // fackets = 0
            "BB000000" +        // lastDataSent = 187
            "00000000" +        // lastAckSent = 0
            "BB000000" +        // lastDataRecv = 187
            "BB000000" +        // lastDataAckRecv = 187
            "DC050000" +        // pmtu = 1500
            "30560100" +        // rcvSsthresh = 87600
            "3E2C0900" +        // rttt = 601150
            "1F960400" +        // rttvar = 300575
            "78050000" +        // sndSsthresh = 1400
            "0A000000" +        // sndCwnd = 10
            "A8050000" +        // advmss = 1448
            "03000000" +        // reordering = 3
            "00000000" +        // rcvrtt = 0
            "30560100" +        // rcvspace = 87600
            "00000000" +        // totalRetrans = 0
            "53AC000000000000" +    // pacingRate = 44115
            "FFFFFFFFFFFFFFFF" +    // maxPacingRate = 18446744073709551615
            "0100000000000000" +    // bytesAcked = 1
            "0000000000000000" +    // bytesReceived = 0
            "0A000000" +        // SegsOut = 10
            "00000000" +        // SegsIn = 0
            "00000000" +        // NotSentBytes = 0
            "3E2C0900" +        // minRtt = 601150
            "00000000" +        // DataSegsIn = 0
            "00000000" +        // DataSegsOut = 0
            "0000000000000000"; // deliverRate = 0
    private static final byte[] SOCK_DIAG_TCP_INET_BYTES =
            HexEncoding.decode(SOCK_DIAG_TCP_INET_HEX.toCharArray(), false);
    private static final TcpInfo TEST_TCPINFO =
            new TcpInfo(5 /* retransmits */, 0 /* lost */, 10 /* segsOut */, 0 /* segsIn */);

    private static final String TEST_RESPONSE_HEX = SOCK_DIAG_TCP_INET_HEX
            // struct nlmsghdr
            + "14000000"     // length = 20
            + "0300"         // type = NLMSG_DONE
            + "0301"         // flags = NLM_F_REQUEST | NLM_F_DUMP
            + "00000000"     // seqno
            + "00000000"     // pid (0 == kernel)
            // struct inet_diag_req_v2
            + "02"           // family = AF_INET
            + "06"           // state
            + "00"           // timer
            + "00";          // retrans
    private static final byte[] TEST_RESPONSE_BYTES =
            HexEncoding.decode(TEST_RESPONSE_HEX.toCharArray(), false);
    private static final int TEST_NETID1 = 0xA85;
    private static final int TEST_NETID2 = 0x1A85;
    private static final int TEST_NETID1_FWMARK = 0x0A85;
    private static final int TEST_NETID2_FWMARK = 0x1A85;
    private static final int NETID_MASK = 0xffff;
    @Mock private TcpSocketTracker.Dependencies mDependencies;
    @Mock private FileDescriptor mMockFd;
    @Mock private INetd mNetd;
    private final Network mNetwork = new Network(TEST_NETID1);
    private final Network mOtherNetwork = new Network(TEST_NETID2);
    private MockitoSession mSession;
    private TerribleFailureHandler mOldWtfHandler;

    @Rule
    public final DevSdkIgnoreRule mIgnoreRule = new DevSdkIgnoreRule();

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        // Override the default TerribleFailureHandler, as that handler might terminate the process
        // (if we're on an eng build).
        mOldWtfHandler =
                Log.setWtfHandler((tag, what, system) -> Log.e(tag, what.getMessage(), what));
        when(mDependencies.getNetd()).thenReturn(mNetd);
        when(mDependencies.isTcpInfoParsingSupported()).thenReturn(true);
        when(mDependencies.connectToKernel()).thenReturn(mMockFd);
        when(mDependencies.getDeviceConfigPropertyInt(
                eq(NAMESPACE_CONNECTIVITY),
                eq(CONFIG_TCP_PACKETS_FAIL_PERCENTAGE),
                anyInt())).thenReturn(DEFAULT_TCP_PACKETS_FAIL_PERCENTAGE);
        mSession = mockitoSession()
                .spyStatic(NetworkShimImpl.class)
                .strictness(Strictness.WARN)
                .startMocking();

        when(mNetd.getFwmarkForNetwork(eq(TEST_NETID1)))
                .thenReturn(makeMarkMaskParcel(NETID_MASK, TEST_NETID1_FWMARK));
    }

    @After
    public void tearDown() {
        mSession.finishMocking();
        Log.setWtfHandler(mOldWtfHandler);
    }

    private MarkMaskParcel makeMarkMaskParcel(final int mask, final int mark) {
        final MarkMaskParcel parcel = new MarkMaskParcel();
        parcel.mask = mask;
        parcel.mark = mark;
        return parcel;
    }

    private ByteBuffer getByteBuffer(final byte[] bytes) {
        final ByteBuffer buffer = ByteBuffer.wrap(bytes);
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        return buffer;
    }

    @Test
    public void testParseSockInfo() {
        final ByteBuffer buffer = getByteBuffer(SOCK_DIAG_TCP_INET_BYTES);
        final TcpSocketTracker tst = new TcpSocketTracker(mDependencies, mNetwork);
        buffer.position(SOCKDIAG_MSG_HEADER_SIZE);
        final TcpSocketTracker.SocketInfo parsed =
                tst.parseSockInfo(buffer, AF_INET, 276, 100L);

        assertEquals(parsed.tcpInfo, TEST_TCPINFO);
        assertEquals(parsed.fwmark, 789125);
        assertEquals(parsed.updateTime, 100);
        assertEquals(parsed.ipFamily, AF_INET);
    }

    @Test
    public void testEnoughBytesRemainForValidNlMsg() {
        final ByteBuffer buffer = ByteBuffer.allocate(TEST_BUFFER_SIZE);

        buffer.position(TEST_BUFFER_SIZE - StructNlMsgHdr.STRUCT_SIZE);
        assertTrue(TcpSocketTracker.enoughBytesRemainForValidNlMsg(buffer));
        // Remaining buffer size is less than a valid StructNlMsgHdr size.
        buffer.position(TEST_BUFFER_SIZE - StructNlMsgHdr.STRUCT_SIZE + 1);
        assertFalse(TcpSocketTracker.enoughBytesRemainForValidNlMsg(buffer));

        buffer.position(TEST_BUFFER_SIZE);
        assertFalse(TcpSocketTracker.enoughBytesRemainForValidNlMsg(buffer));
    }

    @Test @IgnoreUpTo(Build.VERSION_CODES.Q) // TCP info parsing is not supported on Q
    public void testPollSocketsInfo() throws Exception {
        // This test requires shims that provide API 30 access
        assumeTrue(ConstantsShim.VERSION >= 30);
        when(mDependencies.isTcpInfoParsingSupported()).thenReturn(false);
        final TcpSocketTracker tst = new TcpSocketTracker(mDependencies, mNetwork);
        assertFalse(tst.pollSocketsInfo());

        when(mDependencies.isTcpInfoParsingSupported()).thenReturn(true);
        // No enough bytes remain for a valid NlMsg.
        final ByteBuffer invalidBuffer = ByteBuffer.allocate(1);
        invalidBuffer.order(ByteOrder.LITTLE_ENDIAN);
        when(mDependencies.recvMessage(any())).thenReturn(invalidBuffer);
        assertTrue(tst.pollSocketsInfo());
        assertEquals(-1, tst.getLatestPacketFailPercentage());
        assertEquals(0, tst.getSentSinceLastRecv());

        // Header only.
        final ByteBuffer headerBuffer = getByteBuffer(SOCK_DIAG_MSG_BYTES);
        when(mDependencies.recvMessage(any())).thenReturn(headerBuffer);
        assertTrue(tst.pollSocketsInfo());
        assertEquals(-1, tst.getLatestPacketFailPercentage());
        assertEquals(0, tst.getSentSinceLastRecv());

        final ByteBuffer tcpBuffer = getByteBuffer(TEST_RESPONSE_BYTES);
        when(mDependencies.recvMessage(any())).thenReturn(tcpBuffer);
        assertTrue(tst.pollSocketsInfo());

        assertEquals(10, tst.getSentSinceLastRecv());
        assertEquals(50, tst.getLatestPacketFailPercentage());
        assertFalse(tst.isDataStallSuspected());
        // Lower the threshold.
        when(mDependencies.getDeviceConfigPropertyInt(any(), eq(CONFIG_TCP_PACKETS_FAIL_PERCENTAGE),
                anyInt())).thenReturn(40);
        // No device config change. Using cache value.
        assertFalse(tst.isDataStallSuspected());
        // Trigger a config update
        tst.mConfigListener.onPropertiesChanged(null /* properties */);
        assertTrue(tst.isDataStallSuspected());
    }

    @Test
    public void testTcpInfoParsingUnsupported() {
        doReturn(false).when(mDependencies).isTcpInfoParsingSupported();
        final TcpSocketTracker tst = new TcpSocketTracker(mDependencies, mNetwork);
        verify(mDependencies).getNetd();

        assertFalse(tst.pollSocketsInfo());
        assertEquals(-1, tst.getLatestPacketFailPercentage());
        assertEquals(-1, tst.getLatestReceivedCount());
        assertEquals(-1, tst.getSentSinceLastRecv());
        assertFalse(tst.isDataStallSuspected());

        verify(mDependencies, atLeastOnce()).isTcpInfoParsingSupported();
        verifyNoMoreInteractions(mDependencies);
    }

    @Test @IgnoreAfter(Build.VERSION_CODES.Q)
    public void testTcpInfoParsingNotSupportedOnQ() {
        assertFalse(new TcpSocketTracker.Dependencies(getInstrumentation().getContext())
                .isTcpInfoParsingSupported());
    }

    @Test @IgnoreUpTo(Build.VERSION_CODES.Q)
    public void testTcpInfoParsingSupportedFromR() {
        assertTrue(new TcpSocketTracker.Dependencies(getInstrumentation().getContext())
                .isTcpInfoParsingSupported());
    }

    private static final String BAD_DIAG_MSG_HEX =
        // struct nlmsghdr.
            "00000058" +      // length = 1476395008
            "1400" +         // type = SOCK_DIAG_BY_FAMILY
            "0301" +         // flags = NLM_F_REQUEST | NLM_F_DUMP
            "00000000" +     // seqno
            "00000000" +     // pid (0 == kernel)
            // struct inet_diag_req_v2
            "02" +           // family = AF_INET
            "06" +           // state
            "00" +           // timer
            "00" +           // retrans
            // inet_diag_sockid
            "DEA5" +         // idiag_sport = 42462
            "71B9" +         // idiag_dport = 47473
            "0a006402000000000000000000000000" + // idiag_src = 10.0.100.2
            "08080808000000000000000000000000" + // idiag_dst = 8.8.8.8
            "00000000" +    // idiag_if
            "34ED000076270000" + // idiag_cookie = 43387759684916
            "00000000" +    // idiag_expires
            "00000000" +    // idiag_rqueue
            "00000000" +    // idiag_wqueue
            "00000000" +    // idiag_uid
            "00000000";    // idiag_inode
    private static final byte[] BAD_SOCK_DIAG_MSG_BYTES =
        HexEncoding.decode(BAD_DIAG_MSG_HEX.toCharArray(), false);

    @Test @IgnoreUpTo(Build.VERSION_CODES.Q) // TCP info parsing is not supported on Q
    public void testPollSocketsInfo_BadFormat() throws Exception {
        // This test requires shims that provide API 30 access
        assumeTrue(ConstantsShim.VERSION >= 30);
        final TcpSocketTracker tst = new TcpSocketTracker(mDependencies, mNetwork);
        ByteBuffer tcpBuffer = getByteBuffer(TEST_RESPONSE_BYTES);

        when(mDependencies.recvMessage(any())).thenReturn(tcpBuffer);
        assertTrue(tst.pollSocketsInfo());
        assertEquals(10, tst.getSentSinceLastRecv());
        assertEquals(50, tst.getLatestPacketFailPercentage());

        tcpBuffer = getByteBuffer(BAD_SOCK_DIAG_MSG_BYTES);
        when(mDependencies.recvMessage(any())).thenReturn(tcpBuffer);
        assertTrue(tst.pollSocketsInfo());
        // Expect no additional packets, so still 10.
        assertEquals(10, tst.getSentSinceLastRecv());
        // Expect to reset to 0.
        assertEquals(0, tst.getLatestPacketFailPercentage());
    }

    @Test
    public void testUnMatchNetwork() throws Exception {
        when(mNetd.getFwmarkForNetwork(eq(TEST_NETID2)))
                .thenReturn(makeMarkMaskParcel(NETID_MASK, TEST_NETID2_FWMARK));
        final TcpSocketTracker tst = new TcpSocketTracker(mDependencies, mOtherNetwork);
        final ByteBuffer tcpBuffer = getByteBuffer(TEST_RESPONSE_BYTES);
        when(mDependencies.recvMessage(any())).thenReturn(tcpBuffer);
        assertTrue(tst.pollSocketsInfo());

        assertEquals(0, tst.getSentSinceLastRecv());
        assertEquals(-1, tst.getLatestPacketFailPercentage());
        assertFalse(tst.isDataStallSuspected());
    }
}
