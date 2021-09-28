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
package android.net.cts;

import static android.net.cts.PacketUtils.BytePayload;
import static android.net.cts.PacketUtils.IP4_HDRLEN;
import static android.net.cts.PacketUtils.IP6_HDRLEN;
import static android.net.cts.PacketUtils.IpHeader;
import static android.net.cts.PacketUtils.UDP_HDRLEN;
import static android.net.cts.PacketUtils.UdpHeader;
import static android.net.cts.PacketUtils.getIpHeader;
import static android.system.OsConstants.IPPROTO_UDP;

import android.os.ParcelFileDescriptor;

import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.util.Arrays;

// TODO: Merge this with the version in the IPsec module (IKEv2 library) CTS tests.
/** An extension of the TunUtils class with IKE-specific packet handling. */
public class IkeTunUtils extends TunUtils {
    private static final int PORT_LEN = 2;

    private static final byte[] NON_ESP_MARKER = new byte[] {0, 0, 0, 0};

    private static final int IKE_HEADER_LEN = 28;
    private static final int IKE_SPI_LEN = 8;
    private static final int IKE_IS_RESP_BYTE_OFFSET = 19;
    private static final int IKE_MSG_ID_OFFSET = 20;
    private static final int IKE_MSG_ID_LEN = 4;

    public IkeTunUtils(ParcelFileDescriptor tunFd) {
        super(tunFd);
    }

    /**
     * Await an expected IKE request and inject an IKE response.
     *
     * @param respIkePkt IKE response packet without IP/UDP headers or NON ESP MARKER.
     */
    public byte[] awaitReqAndInjectResp(long expectedInitIkeSpi, int expectedMsgId,
            boolean encapExpected, byte[] respIkePkt) throws Exception {
        final byte[] request = awaitIkePacket(expectedInitIkeSpi, expectedMsgId, encapExpected);

        // Build response header by flipping address and port
        final InetAddress srcAddr = getDstAddress(request);
        final InetAddress dstAddr = getSrcAddress(request);
        final int srcPort = getDstPort(request);
        final int dstPort = getSrcPort(request);

        final byte[] response =
                buildIkePacket(srcAddr, dstAddr, srcPort, dstPort, encapExpected, respIkePkt);
        injectPacket(response);
        return request;
    }

    private byte[] awaitIkePacket(long expectedInitIkeSpi, int expectedMsgId, boolean expectEncap)
            throws Exception {
        return super.awaitPacket(pkt -> isIke(pkt, expectedInitIkeSpi, expectedMsgId, expectEncap));
    }

    private static boolean isIke(
            byte[] pkt, long expectedInitIkeSpi, int expectedMsgId, boolean encapExpected) {
        final int ipProtocolOffset;
        final int ikeOffset;

        if (isIpv6(pkt)) {
            ipProtocolOffset = IP6_PROTO_OFFSET;
            ikeOffset = IP6_HDRLEN + UDP_HDRLEN;
        } else {
            if (encapExpected && !hasNonEspMarkerv4(pkt)) {
                return false;
            }

            // Use default IPv4 header length (assuming no options)
            final int encapMarkerLen = encapExpected ? NON_ESP_MARKER.length : 0;
            ipProtocolOffset = IP4_PROTO_OFFSET;
            ikeOffset = IP4_HDRLEN + UDP_HDRLEN + encapMarkerLen;
        }

        return pkt[ipProtocolOffset] == IPPROTO_UDP
                && areSpiAndMsgIdEqual(pkt, ikeOffset, expectedInitIkeSpi, expectedMsgId);
    }

    /** Checks if the provided IPv4 packet has a UDP-encapsulation NON-ESP marker */
    private static boolean hasNonEspMarkerv4(byte[] ipv4Pkt) {
        final int nonEspMarkerOffset = IP4_HDRLEN + UDP_HDRLEN;
        if (ipv4Pkt.length < nonEspMarkerOffset + NON_ESP_MARKER.length) {
            return false;
        }

        final byte[] nonEspMarker = Arrays.copyOfRange(
                ipv4Pkt, nonEspMarkerOffset, nonEspMarkerOffset + NON_ESP_MARKER.length);
        return Arrays.equals(NON_ESP_MARKER, nonEspMarker);
    }

    private static boolean areSpiAndMsgIdEqual(
            byte[] pkt, int ikeOffset, long expectedIkeInitSpi, int expectedMsgId) {
        if (pkt.length <= ikeOffset + IKE_HEADER_LEN) {
            return false;
        }

        final ByteBuffer buffer = ByteBuffer.wrap(pkt);
        final long spi = buffer.getLong(ikeOffset);
        final int msgId = buffer.getInt(ikeOffset + IKE_MSG_ID_OFFSET);

        return expectedIkeInitSpi == spi && expectedMsgId == msgId;
    }

    private static InetAddress getSrcAddress(byte[] pkt) throws Exception {
        return getAddress(pkt, true);
    }

    private static InetAddress getDstAddress(byte[] pkt) throws Exception {
        return getAddress(pkt, false);
    }

    private static InetAddress getAddress(byte[] pkt, boolean getSrcAddr) throws Exception {
        final int ipLen = isIpv6(pkt) ? IP6_ADDR_LEN : IP4_ADDR_LEN;
        final int srcIpOffset = isIpv6(pkt) ? IP6_ADDR_OFFSET : IP4_ADDR_OFFSET;
        final int ipOffset = getSrcAddr ? srcIpOffset : srcIpOffset + ipLen;

        if (pkt.length < ipOffset + ipLen) {
            // Should be impossible; getAddress() is only called with a full IKE request including
            // the IP and UDP headers.
            throw new IllegalArgumentException("Packet was too short to contain IP address");
        }

        return InetAddress.getByAddress(Arrays.copyOfRange(pkt, ipOffset, ipOffset + ipLen));
    }

    private static int getSrcPort(byte[] pkt) throws Exception {
        return getPort(pkt, true);
    }

    private static int getDstPort(byte[] pkt) throws Exception {
        return getPort(pkt, false);
    }

    private static int getPort(byte[] pkt, boolean getSrcPort) {
        final int srcPortOffset = isIpv6(pkt) ? IP6_HDRLEN : IP4_HDRLEN;
        final int portOffset = getSrcPort ? srcPortOffset : srcPortOffset + PORT_LEN;

        if (pkt.length < portOffset + PORT_LEN) {
            // Should be impossible; getPort() is only called with a full IKE request including the
            // IP and UDP headers.
            throw new IllegalArgumentException("Packet was too short to contain port");
        }

        final ByteBuffer buffer = ByteBuffer.wrap(pkt);
        return Short.toUnsignedInt(buffer.getShort(portOffset));
    }

    private static byte[] buildIkePacket(
            InetAddress srcAddr,
            InetAddress dstAddr,
            int srcPort,
            int dstPort,
            boolean useEncap,
            byte[] payload)
            throws Exception {
        // Append non-ESP marker if encap is enabled
        if (useEncap) {
            final ByteBuffer buffer = ByteBuffer.allocate(NON_ESP_MARKER.length + payload.length);
            buffer.put(NON_ESP_MARKER);
            buffer.put(payload);
            payload = buffer.array();
        }

        final UdpHeader udpPkt = new UdpHeader(srcPort, dstPort, new BytePayload(payload));
        final IpHeader ipPkt = getIpHeader(udpPkt.getProtocolId(), srcAddr, dstAddr, udpPkt);
        return ipPkt.getPacketBytes();
    }
}
