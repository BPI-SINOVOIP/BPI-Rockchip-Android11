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

package android.net.netlink;

import static android.net.netlink.StructNlMsgHdr.NLM_F_DUMP;
import static android.net.netlink.StructNlMsgHdr.NLM_F_REQUEST;
import static android.system.OsConstants.AF_INET;
import static android.system.OsConstants.AF_INET6;
import static android.system.OsConstants.IPPROTO_TCP;
import static android.system.OsConstants.IPPROTO_UDP;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import libcore.util.HexEncoding;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class InetDiagSocketTest {
    // Hexadecimal representation of InetDiagReqV2 request.
    private static final String INET_DIAG_REQ_V2_UDP_INET4_HEX =
            // struct nlmsghdr
            "48000000" +     // length = 72
            "1400" +         // type = SOCK_DIAG_BY_FAMILY
            "0103" +         // flags = NLM_F_REQUEST | NLM_F_DUMP
            "00000000" +     // seqno
            "00000000" +     // pid (0 == kernel)
            // struct inet_diag_req_v2
            "02" +           // family = AF_INET
            "11" +           // protcol = IPPROTO_UDP
            "00" +           // idiag_ext
            "00" +           // pad
            "ffffffff" +     // idiag_states
            // inet_diag_sockid
            "a5de" +         // idiag_sport = 42462
            "b971" +         // idiag_dport = 47473
            "0a006402000000000000000000000000" + // idiag_src = 10.0.100.2
            "08080808000000000000000000000000" + // idiag_dst = 8.8.8.8
            "00000000" +     // idiag_if
            "ffffffffffffffff"; // idiag_cookie = INET_DIAG_NOCOOKIE
    private static final byte[] INET_DIAG_REQ_V2_UDP_INET4_BYTES =
            HexEncoding.decode(INET_DIAG_REQ_V2_UDP_INET4_HEX.toCharArray(), false);

    @Test
    public void testInetDiagReqV2UdpInet4() throws Exception {
        InetSocketAddress local = new InetSocketAddress(InetAddress.getByName("10.0.100.2"),
                42462);
        InetSocketAddress remote = new InetSocketAddress(InetAddress.getByName("8.8.8.8"),
                47473);
        final byte[] msg = InetDiagMessage.InetDiagReqV2(IPPROTO_UDP, local, remote, AF_INET,
                (short) (NLM_F_REQUEST | NLM_F_DUMP));
        assertArrayEquals(INET_DIAG_REQ_V2_UDP_INET4_BYTES, msg);
    }

    // Hexadecimal representation of InetDiagReqV2 request.
    private static final String INET_DIAG_REQ_V2_TCP_INET6_HEX =
            // struct nlmsghdr
            "48000000" +     // length = 72
            "1400" +         // type = SOCK_DIAG_BY_FAMILY
            "0100" +         // flags = NLM_F_REQUEST
            "00000000" +     // seqno
            "00000000" +     // pid (0 == kernel)
            // struct inet_diag_req_v2
            "0a" +           // family = AF_INET6
            "06" +           // protcol = IPPROTO_TCP
            "00" +           // idiag_ext
            "00" +           // pad
            "ffffffff" +     // idiag_states
                // inet_diag_sockid
                "a5de" +         // idiag_sport = 42462
                "b971" +         // idiag_dport = 47473
                "fe8000000000000086c9b2fffe6aed4b" + // idiag_src = fe80::86c9:b2ff:fe6a:ed4b
                "08080808000000000000000000000000" + // idiag_dst = 8.8.8.8
                "00000000" +     // idiag_if
                "ffffffffffffffff"; // idiag_cookie = INET_DIAG_NOCOOKIE
    private static final byte[] INET_DIAG_REQ_V2_TCP_INET6_BYTES =
            HexEncoding.decode(INET_DIAG_REQ_V2_TCP_INET6_HEX.toCharArray(), false);

    @Test
    public void testInetDiagReqV2TcpInet6() throws Exception {
        InetSocketAddress local = new InetSocketAddress(
                InetAddress.getByName("fe80::86c9:b2ff:fe6a:ed4b"), 42462);
        InetSocketAddress remote = new InetSocketAddress(InetAddress.getByName("8.8.8.8"),
                47473);
        byte[] msg = InetDiagMessage.InetDiagReqV2(IPPROTO_TCP, local, remote, AF_INET6,
                NLM_F_REQUEST);

        assertArrayEquals(INET_DIAG_REQ_V2_TCP_INET6_BYTES, msg);
    }

    // Hexadecimal representation of InetDiagReqV2 request with extension, INET_DIAG_INFO.
    private static final String INET_DIAG_REQ_V2_TCP_INET_INET_DIAG_HEX =
            // struct nlmsghdr
            "48000000" +     // length = 72
            "1400" +         // type = SOCK_DIAG_BY_FAMILY
            "0100" +         // flags = NLM_F_REQUEST
            "00000000" +     // seqno
            "00000000" +     // pid (0 == kernel)
            // struct inet_diag_req_v2
            "02" +           // family = AF_INET
            "06" +           // protcol = IPPROTO_TCP
            "02" +           // idiag_ext = INET_DIAG_INFO
            "00" +           // pad
            "ffffffff" +   // idiag_states
            // inet_diag_sockid
            "3039" +         // idiag_sport = 12345
            "d431" +         // idiag_dport = 54321
            "01020304000000000000000000000000" + // idiag_src = 1.2.3.4
            "08080404000000000000000000000000" + // idiag_dst = 8.8.4.4
            "00000000" +     // idiag_if
            "ffffffffffffffff"; // idiag_cookie = INET_DIAG_NOCOOKIE

    private static final byte[] INET_DIAG_REQ_V2_TCP_INET_INET_DIAG_BYTES =
            HexEncoding.decode(INET_DIAG_REQ_V2_TCP_INET_INET_DIAG_HEX.toCharArray(), false);
    private static final int TCP_ALL_STATES = 0xffffffff;
    @Test
    public void testInetDiagReqV2TcpInetWithExt() throws Exception {
        InetSocketAddress local = new InetSocketAddress(
                InetAddress.getByName("1.2.3.4"), 12345);
        InetSocketAddress remote = new InetSocketAddress(InetAddress.getByName("8.8.4.4"),
                54321);
        byte[] msg = InetDiagMessage.InetDiagReqV2(IPPROTO_TCP, local, remote, AF_INET,
                NLM_F_REQUEST, 0 /* pad */, 2 /* idiagExt */, TCP_ALL_STATES);

        assertArrayEquals(INET_DIAG_REQ_V2_TCP_INET_INET_DIAG_BYTES, msg);

        local = new InetSocketAddress(
                InetAddress.getByName("fe80::86c9:b2ff:fe6a:ed4b"), 42462);
        remote = new InetSocketAddress(InetAddress.getByName("8.8.8.8"),
                47473);
        msg = InetDiagMessage.InetDiagReqV2(IPPROTO_TCP, local, remote, AF_INET6,
                NLM_F_REQUEST, 0 /* pad */, 0 /* idiagExt */, TCP_ALL_STATES);

        assertArrayEquals(INET_DIAG_REQ_V2_TCP_INET6_BYTES, msg);
    }

    // Hexadecimal representation of InetDiagReqV2 request with no socket specified.
    private static final String INET_DIAG_REQ_V2_TCP_INET6_NO_ID_SPECIFIED_HEX =
            // struct nlmsghdr
            "48000000" +     // length = 72
            "1400" +         // type = SOCK_DIAG_BY_FAMILY
            "0100" +         // flags = NLM_F_REQUEST
            "00000000" +     // seqno
            "00000000" +     // pid (0 == kernel)
            // struct inet_diag_req_v2
            "0a" +           // family = AF_INET6
            "06" +           // protcol = IPPROTO_TCP
            "00" +           // idiag_ext
            "00" +           // pad
            "ffffffff" +     // idiag_states
            // inet_diag_sockid
            "0000" +         // idiag_sport
            "0000" +         // idiag_dport
            "00000000000000000000000000000000" + // idiag_src
            "00000000000000000000000000000000" + // idiag_dst
            "00000000" +     // idiag_if
            "0000000000000000"; // idiag_cookie

    private static final byte[] INET_DIAG_REQ_V2_TCP_INET6_NO_ID_SPECIFED_BYTES =
            HexEncoding.decode(INET_DIAG_REQ_V2_TCP_INET6_NO_ID_SPECIFIED_HEX.toCharArray(), false);

    @Test
    public void testInetDiagReqV2TcpInet6NoIdSpecified() throws Exception {
        InetSocketAddress local = new InetSocketAddress(
                InetAddress.getByName("fe80::fe6a:ed4b"), 12345);
        InetSocketAddress remote = new InetSocketAddress(InetAddress.getByName("8.8.4.4"),
                54321);
        // Verify no socket specified if either local or remote socket address is null.
        byte[] msgExt = InetDiagMessage.InetDiagReqV2(IPPROTO_TCP, null, null, AF_INET6,
                NLM_F_REQUEST, 0 /* pad */, 0 /* idiagExt */, TCP_ALL_STATES);
        byte[] msg;
        try {
            msg = InetDiagMessage.InetDiagReqV2(IPPROTO_TCP, null, remote, AF_INET6,
                    NLM_F_REQUEST);
            fail("Both remote and local should be null, expected UnknownHostException");
        } catch (NullPointerException e) {
        }

        try {
            msg = InetDiagMessage.InetDiagReqV2(IPPROTO_TCP, local, null, AF_INET6,
                    NLM_F_REQUEST, 0 /* pad */, 0 /* idiagExt */, TCP_ALL_STATES);
            fail("Both remote and local should be null, expected UnknownHostException");
        } catch (NullPointerException e) {
        }

        msg = InetDiagMessage.InetDiagReqV2(IPPROTO_TCP, null, null, AF_INET6,
                NLM_F_REQUEST, 0 /* pad */, 0 /* idiagExt */, TCP_ALL_STATES);
        assertArrayEquals(INET_DIAG_REQ_V2_TCP_INET6_NO_ID_SPECIFED_BYTES, msg);
        assertArrayEquals(INET_DIAG_REQ_V2_TCP_INET6_NO_ID_SPECIFED_BYTES, msgExt);
    }

    // Hexadecimal representation of InetDiagReqV2 request.
    private static final String INET_DIAG_MSG_HEX =
            // struct nlmsghdr
            "58000000" +     // length = 88
            "1400" +         // type = SOCK_DIAG_BY_FAMILY
            "0200" +         // flags = NLM_F_MULTI
            "00000000" +     // seqno
            "f5220000" +     // pid (0 == kernel)
            // struct inet_diag_msg
            "0a" +           // family = AF_INET6
            "01" +           // idiag_state
            "00" +           // idiag_timer
            "00" +           // idiag_retrans
                // inet_diag_sockid
                "a817" +     // idiag_sport = 43031
                "960f" +     // idiag_dport = 38415
                "fe8000000000000086c9b2fffe6aed4b" + // idiag_src = fe80::86c9:b2ff:fe6a:ed4b
                "00000000000000000000ffff08080808" + // idiag_dst = 8.8.8.8
                "00000000" + // idiag_if
                "ffffffffffffffff" + // idiag_cookie = INET_DIAG_NOCOOKIE
            "00000000" +     // idiag_expires
            "00000000" +     // idiag_rqueue
            "00000000" +     // idiag_wqueue
            "a3270000" +     // idiag_uid
            "A57E1900";      // idiag_inode
    private static final byte[] INET_DIAG_MSG_BYTES =
            HexEncoding.decode(INET_DIAG_MSG_HEX.toCharArray(), false);

    @Test
    public void testParseInetDiagResponse() throws Exception {
        final ByteBuffer byteBuffer = ByteBuffer.wrap(INET_DIAG_MSG_BYTES);
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        final NetlinkMessage msg = NetlinkMessage.parse(byteBuffer);
        assertNotNull(msg);

        assertTrue(msg instanceof InetDiagMessage);
        final InetDiagMessage inetDiagMsg = (InetDiagMessage) msg;
        assertEquals(10147, inetDiagMsg.mStructInetDiagMsg.idiag_uid);

        final StructNlMsgHdr hdr = inetDiagMsg.getHeader();
        assertNotNull(hdr);
        assertEquals(NetlinkConstants.SOCK_DIAG_BY_FAMILY, hdr.nlmsg_type);
        assertEquals(StructNlMsgHdr.NLM_F_MULTI, hdr.nlmsg_flags);
        assertEquals(0, hdr.nlmsg_seq);
        assertEquals(8949, hdr.nlmsg_pid);
    }
}
