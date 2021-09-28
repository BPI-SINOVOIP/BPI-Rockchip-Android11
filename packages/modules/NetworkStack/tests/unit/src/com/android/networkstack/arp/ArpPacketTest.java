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

package com.android.networkstack.arp;

import static com.android.server.util.NetworkStackConstants.ARP_REQUEST;
import static com.android.server.util.NetworkStackConstants.ETHER_ADDR_LEN;
import static com.android.testutils.MiscAssertsKt.assertThrows;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import android.net.InetAddresses;
import android.net.MacAddress;
import android.net.dhcp.DhcpPacket;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.net.Inet4Address;
import java.nio.ByteBuffer;

@RunWith(AndroidJUnit4.class)
@SmallTest
public final class ArpPacketTest {

    private static final Inet4Address TEST_IPV4_ADDR =
            (Inet4Address) InetAddresses.parseNumericAddress("192.168.1.2");
    private static final Inet4Address INADDR_ANY =
            (Inet4Address) InetAddresses.parseNumericAddress("0.0.0.0");
    private static final byte[] TEST_SENDER_MAC_ADDR = new byte[] {
            0x00, 0x1a, 0x11, 0x22, 0x33, 0x33 };
    private static final byte[] TEST_TARGET_MAC_ADDR = new byte[] {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    private static final byte[] TEST_ARP_PROBE = new byte[] {
        // dst mac address
        (byte) 0xff, (byte) 0xff, (byte) 0xff, (byte) 0xff, (byte) 0xff, (byte) 0xff,
        // src mac address
        (byte) 0x00, (byte) 0x1a, (byte) 0x11, (byte) 0x22, (byte) 0x33, (byte) 0x33,
        // ether type
        (byte) 0x08, (byte) 0x06,
        // hardware type
        (byte) 0x00, (byte) 0x01,
        // protocol type
        (byte) 0x08, (byte) 0x00,
        // hardware address size
        (byte) 0x06,
        // protocol address size
        (byte) 0x04,
        // opcode
        (byte) 0x00, (byte) 0x01,
        // sender mac address
        (byte) 0x00, (byte) 0x1a, (byte) 0x11, (byte) 0x22, (byte) 0x33, (byte) 0x33,
        // sender IP address
        (byte) 0x00, (byte) 0x00, (byte) 0x00, (byte) 0x00,
        // target mac address
        (byte) 0x00, (byte) 0x00, (byte) 0x00, (byte) 0x00, (byte) 0x00, (byte) 0x00,
        // target IP address
        (byte) 0xc0, (byte) 0xa8, (byte) 0x01, (byte) 0x02,
    };

    private static final byte[] TEST_ARP_ANNOUNCE = new byte[] {
        // dst mac address
        (byte) 0xff, (byte) 0xff, (byte) 0xff, (byte) 0xff, (byte) 0xff, (byte) 0xff,
        // src mac address
        (byte) 0x00, (byte) 0x1a, (byte) 0x11, (byte) 0x22, (byte) 0x33, (byte) 0x33,
        // ether type
        (byte) 0x08, (byte) 0x06,
        // hardware type
        (byte) 0x00, (byte) 0x01,
        // protocol type
        (byte) 0x08, (byte) 0x00,
        // hardware address size
        (byte) 0x06,
        // protocol address size
        (byte) 0x04,
        // opcode
        (byte) 0x00, (byte) 0x01,
        // sender mac address
        (byte) 0x00, (byte) 0x1a, (byte) 0x11, (byte) 0x22, (byte) 0x33, (byte) 0x33,
        // sender IP address
        (byte) 0xc0, (byte) 0xa8, (byte) 0x01, (byte) 0x02,
        // target mac address
        (byte) 0x00, (byte) 0x00, (byte) 0x00, (byte) 0x00, (byte) 0x00, (byte) 0x00,
        // target IP address
        (byte) 0xc0, (byte) 0xa8, (byte) 0x01, (byte) 0x02,
    };

    private static final byte[] TEST_ARP_PROBE_TRUNCATED = new byte[] {
        // dst mac address
        (byte) 0xff, (byte) 0xff, (byte) 0xff, (byte) 0xff, (byte) 0xff, (byte) 0xff,
        // src mac address
        (byte) 0x00, (byte) 0x1a, (byte) 0x11, (byte) 0x22, (byte) 0x33, (byte) 0x33,
        // ether type
        (byte) 0x08, (byte) 0x06,
        // hardware type
        (byte) 0x00, (byte) 0x01,
        // protocol type
        (byte) 0x08, (byte) 0x00,
        // hardware address size
        (byte) 0x06,
        // protocol address size
        (byte) 0x04,
        // opcode
        (byte) 0x00,
    };

    private static final byte[] TEST_ARP_PROBE_TRUNCATED_MAC = new byte[] {
         // dst mac address
        (byte) 0xff, (byte) 0xff, (byte) 0xff, (byte) 0xff, (byte) 0xff, (byte) 0xff,
        // src mac address
        (byte) 0x00, (byte) 0x1a, (byte) 0x11, (byte) 0x22, (byte) 0x33, (byte) 0x33,
        // ether type
        (byte) 0x08, (byte) 0x06,
        // hardware type
        (byte) 0x00, (byte) 0x01,
        // protocol type
        (byte) 0x08, (byte) 0x00,
        // hardware address size
        (byte) 0x06,
        // protocol address size
        (byte) 0x04,
        // opcode
        (byte) 0x00, (byte) 0x01,
        // sender mac address
        (byte) 0x00, (byte) 0x1a, (byte) 0x11, (byte) 0x22, (byte) 0x33,
    };

    @Test
    public void testBuildArpProbePacket() throws Exception {
        final ByteBuffer arpProbe = ArpPacket.buildArpPacket(DhcpPacket.ETHER_BROADCAST,
                TEST_SENDER_MAC_ADDR, TEST_IPV4_ADDR.getAddress(), new byte[ETHER_ADDR_LEN],
                INADDR_ANY.getAddress(), (short) ARP_REQUEST);
        assertArrayEquals(arpProbe.array(), TEST_ARP_PROBE);
    }

    @Test
    public void testBuildArpAnnouncePacket() throws Exception {
        final ByteBuffer arpAnnounce = ArpPacket.buildArpPacket(DhcpPacket.ETHER_BROADCAST,
                TEST_SENDER_MAC_ADDR, TEST_IPV4_ADDR.getAddress(), new byte[ETHER_ADDR_LEN],
                TEST_IPV4_ADDR.getAddress(), (short) ARP_REQUEST);
        assertArrayEquals(arpAnnounce.array(), TEST_ARP_ANNOUNCE);
    }

    @Test
    public void testParseArpProbePacket() throws Exception {
        final ArpPacket packet = ArpPacket.parseArpPacket(TEST_ARP_PROBE, TEST_ARP_PROBE.length);
        assertEquals(packet.opCode, ARP_REQUEST);
        assertEquals(packet.senderHwAddress, MacAddress.fromBytes(TEST_SENDER_MAC_ADDR));
        assertEquals(packet.targetHwAddress, MacAddress.fromBytes(TEST_TARGET_MAC_ADDR));
        assertEquals(packet.senderIp, INADDR_ANY);
        assertEquals(packet.targetIp, TEST_IPV4_ADDR);
    }

    @Test
    public void testParseArpAnnouncePacket() throws Exception {
        final ArpPacket packet = ArpPacket.parseArpPacket(TEST_ARP_ANNOUNCE,
                TEST_ARP_ANNOUNCE.length);
        assertEquals(packet.opCode, ARP_REQUEST);
        assertEquals(packet.senderHwAddress, MacAddress.fromBytes(TEST_SENDER_MAC_ADDR));
        assertEquals(packet.targetHwAddress, MacAddress.fromBytes(TEST_TARGET_MAC_ADDR));
        assertEquals(packet.senderIp, TEST_IPV4_ADDR);
        assertEquals(packet.targetIp, TEST_IPV4_ADDR);
    }

    @Test
    public void testParseArpPacket_invalidByteBufferParameters() throws Exception {
        assertThrows(ArpPacket.ParseException.class, () -> ArpPacket.parseArpPacket(
                TEST_ARP_PROBE, 0));
    }

    @Test
    public void testParseArpPacket_truncatedPacket() throws Exception {
        assertThrows(ArpPacket.ParseException.class, () -> ArpPacket.parseArpPacket(
                TEST_ARP_PROBE_TRUNCATED, TEST_ARP_PROBE_TRUNCATED.length));
    }

    @Test
    public void testParseArpPacket_truncatedMacAddress() throws Exception {
        assertThrows(ArpPacket.ParseException.class, () -> ArpPacket.parseArpPacket(
                TEST_ARP_PROBE_TRUNCATED_MAC, TEST_ARP_PROBE_TRUNCATED.length));
    }
}
