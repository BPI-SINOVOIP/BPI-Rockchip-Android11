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

package android.net;

import static com.android.testutils.ParcelUtilsKt.assertParcelingIsLossless;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.net.InetAddress;
import java.nio.ByteBuffer;

@RunWith(JUnit4.class)
public final class TcpKeepalivePacketDataTest {
    private static final byte[] IPV4_KEEPALIVE_SRC_ADDR = {10, 0, 0, 1};
    private static final byte[] IPV4_KEEPALIVE_DST_ADDR = {10, 0, 0, 5};

    @Before
    public void setUp() {}

    @Test
    public void testV4TcpKeepalivePacket() throws Exception {
        final int srcPort = 1234;
        final int dstPort = 4321;
        final int seq = 0x11111111;
        final int ack = 0x22222222;
        final int wnd = 8000;
        final int wndScale = 2;
        final int tos = 4;
        final int ttl = 64;
        TcpKeepalivePacketData resultData = null;
        final TcpKeepalivePacketDataParcelable testInfo = new TcpKeepalivePacketDataParcelable();
        testInfo.srcAddress = IPV4_KEEPALIVE_SRC_ADDR;
        testInfo.srcPort = srcPort;
        testInfo.dstAddress = IPV4_KEEPALIVE_DST_ADDR;
        testInfo.dstPort = dstPort;
        testInfo.seq = seq;
        testInfo.ack = ack;
        testInfo.rcvWnd = wnd;
        testInfo.rcvWndScale = wndScale;
        testInfo.tos = tos;
        testInfo.ttl = ttl;
        try {
            resultData = TcpKeepalivePacketData.tcpKeepalivePacket(testInfo);
        } catch (InvalidPacketException e) {
            fail("InvalidPacketException: " + e);
        }

        assertEquals(InetAddress.getByAddress(testInfo.srcAddress), resultData.getSrcAddress());
        assertEquals(InetAddress.getByAddress(testInfo.dstAddress), resultData.getDstAddress());
        assertEquals(testInfo.srcPort, resultData.getSrcPort());
        assertEquals(testInfo.dstPort, resultData.getDstPort());
        assertEquals(testInfo.seq, resultData.tcpSeq);
        assertEquals(testInfo.ack, resultData.tcpAck);
        assertEquals(testInfo.rcvWnd, resultData.tcpWnd);
        assertEquals(testInfo.rcvWndScale, resultData.tcpWndScale);
        assertEquals(testInfo.tos, resultData.ipTos);
        assertEquals(testInfo.ttl, resultData.ipTtl);

        assertParcelingIsLossless(resultData);

        final byte[] packet = resultData.getPacket();
        // IP version and IHL
        assertEquals(packet[0], 0x45);
        // TOS
        assertEquals(packet[1], tos);
        // TTL
        assertEquals(packet[8], ttl);
        // Source IP address.
        byte[] ip = new byte[4];
        ByteBuffer buf = ByteBuffer.wrap(packet, 12, 4);
        buf.get(ip);
        assertArrayEquals(ip, IPV4_KEEPALIVE_SRC_ADDR);
        // Destination IP address.
        buf = ByteBuffer.wrap(packet, 16, 4);
        buf.get(ip);
        assertArrayEquals(ip, IPV4_KEEPALIVE_DST_ADDR);

        buf = ByteBuffer.wrap(packet, 20, 12);
        // Source port.
        assertEquals(buf.getShort(), srcPort);
        // Destination port.
        assertEquals(buf.getShort(), dstPort);
        // Sequence number.
        assertEquals(buf.getInt(), seq);
        // Ack.
        assertEquals(buf.getInt(), ack);
        // Window size.
        buf = ByteBuffer.wrap(packet, 34, 2);
        assertEquals(buf.getShort(), wnd >> wndScale);
    }

    //TODO: add ipv6 test when ipv6 supported

    @Test
    public void testParcel() throws Exception {
        final int srcPort = 1234;
        final int dstPort = 4321;
        final int sequence = 0x11111111;
        final int ack = 0x22222222;
        final int wnd = 48_000;
        final int wndScale = 2;
        final int tos = 4;
        final int ttl = 64;
        final TcpKeepalivePacketDataParcelable testInfo = new TcpKeepalivePacketDataParcelable();
        testInfo.srcAddress = IPV4_KEEPALIVE_SRC_ADDR;
        testInfo.srcPort = srcPort;
        testInfo.dstAddress = IPV4_KEEPALIVE_DST_ADDR;
        testInfo.dstPort = dstPort;
        testInfo.seq = sequence;
        testInfo.ack = ack;
        testInfo.rcvWnd = wnd;
        testInfo.rcvWndScale = wndScale;
        testInfo.tos = tos;
        testInfo.ttl = ttl;
        TcpKeepalivePacketData testData = null;
        TcpKeepalivePacketDataParcelable resultData = null;
        testData = TcpKeepalivePacketData.tcpKeepalivePacket(testInfo);
        resultData = testData.toStableParcelable();
        assertArrayEquals(resultData.srcAddress, IPV4_KEEPALIVE_SRC_ADDR);
        assertArrayEquals(resultData.dstAddress, IPV4_KEEPALIVE_DST_ADDR);
        assertEquals(resultData.srcPort, srcPort);
        assertEquals(resultData.dstPort, dstPort);
        assertEquals(resultData.seq, sequence);
        assertEquals(resultData.ack, ack);
        assertEquals(resultData.rcvWnd, wnd);
        assertEquals(resultData.rcvWndScale, wndScale);
        assertEquals(resultData.tos, tos);
        assertEquals(resultData.ttl, ttl);
    }
}
