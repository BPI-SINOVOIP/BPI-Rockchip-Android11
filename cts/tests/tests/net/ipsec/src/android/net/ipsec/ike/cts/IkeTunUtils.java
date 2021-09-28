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
package android.net.ipsec.ike.cts;

import static android.net.ipsec.ike.cts.PacketUtils.BytePayload;
import static android.net.ipsec.ike.cts.PacketUtils.IP4_HDRLEN;
import static android.net.ipsec.ike.cts.PacketUtils.IP6_HDRLEN;
import static android.net.ipsec.ike.cts.PacketUtils.Ip4Header;
import static android.net.ipsec.ike.cts.PacketUtils.Ip6Header;
import static android.net.ipsec.ike.cts.PacketUtils.IpHeader;
import static android.net.ipsec.ike.cts.PacketUtils.Payload;
import static android.net.ipsec.ike.cts.PacketUtils.UDP_HDRLEN;
import static android.net.ipsec.ike.cts.PacketUtils.UdpHeader;
import static android.system.OsConstants.IPPROTO_UDP;

import static com.android.internal.util.HexDump.hexStringToByteArray;

import static org.junit.Assert.fail;

import android.os.ParcelFileDescriptor;

import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.Predicate;

public class IkeTunUtils extends TunUtils {
    private static final int PORT_LEN = 2;

    private static final int NON_ESP_MARKER_LEN = 4;
    private static final byte[] NON_ESP_MARKER = new byte[NON_ESP_MARKER_LEN];

    private static final int IKE_INIT_SPI_OFFSET = 0;
    private static final int IKE_FIRST_PAYLOAD_OFFSET = 16;
    private static final int IKE_IS_RESP_BYTE_OFFSET = 19;
    private static final int IKE_MSG_ID_OFFSET = 20;
    private static final int IKE_HEADER_LEN = 28;
    private static final int IKE_FRAG_NUM_OFFSET = 32;
    private static final int IKE_PAYLOAD_TYPE_SKF = 53;

    private static final int RSP_FLAG_MASK = 0x20;

    public IkeTunUtils(ParcelFileDescriptor tunFd) {
        super(tunFd);
    }

    /**
     * Await the expected IKE request inject an IKE response (or a list of response fragments)
     *
     * @param ikeRespDataFragmentsHex IKE response hex (or a list of response fragments) without
     *     IP/UDP headers or NON ESP MARKER.
     */
    public byte[] awaitReqAndInjectResp(
            long expectedInitIkeSpi,
            int expectedMsgId,
            boolean expectedUseEncap,
            String... ikeRespDataFragmentsHex)
            throws Exception {
        return awaitReqAndInjectResp(
                        expectedInitIkeSpi,
                        expectedMsgId,
                        expectedUseEncap,
                        1 /* expectedReqPktCnt */,
                        ikeRespDataFragmentsHex)
                .get(0);
    }

    /**
     * Await the expected IKE request (or the list of IKE request fragments) and inject an IKE
     * response (or a list of response fragments)
     *
     * @param ikeRespDataFragmentsHex IKE response hex (or a list of response fragments) without
     *     IP/UDP headers or NON ESP MARKER.
     */
    public List<byte[]> awaitReqAndInjectResp(
            long expectedInitIkeSpi,
            int expectedMsgId,
            boolean expectedUseEncap,
            int expectedReqPktCnt,
            String... ikeRespDataFragmentsHex)
            throws Exception {
        List<byte[]> reqList = new ArrayList<>(expectedReqPktCnt);
        if (expectedReqPktCnt == 1) {
            // Expecting one complete IKE packet
            byte[] req =
                    awaitIkePacket(
                            (pkt) -> {
                                return isExpectedIkePkt(
                                        pkt,
                                        expectedInitIkeSpi,
                                        expectedMsgId,
                                        false /* expectedResp */,
                                        expectedUseEncap);
                            });
            reqList.add(req);
        } else {
            // Expecting "expectedReqPktCnt" number of request fragments
            for (int i = 0; i < expectedReqPktCnt; i++) {
                // IKE Fragment number always starts from 1
                int expectedFragNum = i + 1;
                byte[] req =
                        awaitIkePacket(
                                (pkt) -> {
                                    return isExpectedIkeFragPkt(
                                            pkt,
                                            expectedInitIkeSpi,
                                            expectedMsgId,
                                            false /* expectedResp */,
                                            expectedUseEncap,
                                            expectedFragNum);
                                });
                reqList.add(req);
            }
        }

        // All request fragments have the same addresses and ports
        byte[] request = reqList.get(0);

        // Build response header by flipping address and port
        InetAddress srcAddr = getAddress(request, false /* shouldGetSource */);
        InetAddress dstAddr = getAddress(request, true /* shouldGetSource */);
        int srcPort = getPort(request, false /* shouldGetSource */);
        int dstPort = getPort(request, true /* shouldGetSource */);
        for (String resp : ikeRespDataFragmentsHex) {
            byte[] response =
                    buildIkePacket(
                            srcAddr,
                            dstAddr,
                            srcPort,
                            dstPort,
                            expectedUseEncap,
                            hexStringToByteArray(resp));
            injectPacket(response);
        }

        return reqList;
    }

    /** Await the expected IKE response */
    public byte[] awaitResp(long expectedInitIkeSpi, int expectedMsgId, boolean expectedUseEncap)
            throws Exception {
        return awaitIkePacket(
                (pkt) -> {
                    return isExpectedIkePkt(
                            pkt,
                            expectedInitIkeSpi,
                            expectedMsgId,
                            true /* expectedResp*/,
                            expectedUseEncap);
                });
    }

    private byte[] awaitIkePacket(Predicate<byte[]> pktVerifier) throws Exception {
        long endTime = System.currentTimeMillis() + TIMEOUT;
        int startIndex = 0;
        synchronized (mPackets) {
            while (System.currentTimeMillis() < endTime) {
                byte[] ikePkt = getFirstMatchingPacket(pktVerifier, startIndex);
                if (ikePkt != null) {
                    return ikePkt; // We've found the packet we're looking for.
                }

                startIndex = mPackets.size();

                // Try to prevent waiting too long. If waitTimeout <= 0, we've already hit timeout
                long waitTimeout = endTime - System.currentTimeMillis();
                if (waitTimeout > 0) {
                    mPackets.wait(waitTimeout);
                }
            }

            fail("No matching packet found");
        }

        throw new IllegalStateException(
                "Hit an impossible case where fail() didn't throw an exception");
    }

    private static boolean isExpectedIkePkt(
            byte[] pkt,
            long expectedInitIkeSpi,
            int expectedMsgId,
            boolean expectedResp,
            boolean expectedUseEncap) {
        int ipProtocolOffset = isIpv6(pkt) ? IP6_PROTO_OFFSET : IP4_PROTO_OFFSET;
        int ikeOffset = getIkeOffset(pkt, expectedUseEncap);

        return pkt[ipProtocolOffset] == IPPROTO_UDP
                && expectedUseEncap == hasNonEspMarker(pkt)
                && isExpectedSpiAndMsgId(
                        pkt, ikeOffset, expectedInitIkeSpi, expectedMsgId, expectedResp);
    }

    private static boolean isExpectedIkeFragPkt(
            byte[] pkt,
            long expectedInitIkeSpi,
            int expectedMsgId,
            boolean expectedResp,
            boolean expectedUseEncap,
            int expectedFragNum) {
        return isExpectedIkePkt(
                        pkt, expectedInitIkeSpi, expectedMsgId, expectedResp, expectedUseEncap)
                && isExpectedFragNum(pkt, getIkeOffset(pkt, expectedUseEncap), expectedFragNum);
    }

    private static int getIkeOffset(byte[] pkt, boolean useEncap) {
        if (isIpv6(pkt)) {
            // IPv6 UDP expectedUseEncap not supported by kernels; assume non-expectedUseEncap.
            return IP6_HDRLEN + UDP_HDRLEN;
        } else {
            // Use default IPv4 header length (assuming no options)
            int ikeOffset = IP4_HDRLEN + UDP_HDRLEN;
            return useEncap ? ikeOffset + NON_ESP_MARKER_LEN : ikeOffset;
        }
    }

    private static boolean hasNonEspMarker(byte[] pkt) {
        ByteBuffer buffer = ByteBuffer.wrap(pkt);
        int ikeOffset = IP4_HDRLEN + UDP_HDRLEN;
        if (buffer.remaining() < ikeOffset) return false;

        buffer.get(new byte[ikeOffset]); // Skip IP and UDP header
        byte[] nonEspMarker = new byte[NON_ESP_MARKER_LEN];
        if (buffer.remaining() < NON_ESP_MARKER_LEN) return false;

        buffer.get(nonEspMarker);
        return Arrays.equals(NON_ESP_MARKER, nonEspMarker);
    }

    private static boolean isExpectedSpiAndMsgId(
            byte[] pkt,
            int ikeOffset,
            long expectedInitIkeSpi,
            int expectedMsgId,
            boolean expectedResp) {
        if (pkt.length <= ikeOffset + IKE_HEADER_LEN) return false;

        ByteBuffer buffer = ByteBuffer.wrap(pkt);
        buffer.get(new byte[ikeOffset]); // Skip IP, UDP header (and NON_ESP_MARKER)
        buffer.mark(); // Mark this position so that later we can reset back here

        // Check SPI
        buffer.get(new byte[IKE_INIT_SPI_OFFSET]);
        long initSpi = buffer.getLong();
        if (expectedInitIkeSpi != initSpi) {
            return false;
        }

        // Check direction
        buffer.reset();
        buffer.get(new byte[IKE_IS_RESP_BYTE_OFFSET]);
        byte flagsByte = buffer.get();
        boolean isResp = ((flagsByte & RSP_FLAG_MASK) != 0);
        if (expectedResp != isResp) {
            return false;
        }

        // Check message ID
        buffer.reset();
        buffer.get(new byte[IKE_MSG_ID_OFFSET]);

        // Both the expected message ID and the packet's msgId are signed integers, so directly
        // compare them.
        int msgId = buffer.getInt();
        if (expectedMsgId != msgId) {
            return false;
        }

        return true;
    }

    private static boolean isExpectedFragNum(byte[] pkt, int ikeOffset, int expectedFragNum) {
        ByteBuffer buffer = ByteBuffer.wrap(pkt);
        buffer.get(new byte[ikeOffset]);
        buffer.mark(); // Mark this position so that later we can reset back here

        // Check if it is a fragment packet
        buffer.get(new byte[IKE_FIRST_PAYLOAD_OFFSET]);
        int firstPayload = Byte.toUnsignedInt(buffer.get());
        if (firstPayload != IKE_PAYLOAD_TYPE_SKF) {
            return false;
        }

        // Check fragment number
        buffer.reset();
        buffer.get(new byte[IKE_FRAG_NUM_OFFSET]);
        int fragNum = Short.toUnsignedInt(buffer.getShort());
        return expectedFragNum == fragNum;
    }

    public static class PortPair {
        public final int srcPort;
        public final int dstPort;

        public PortPair(int sourcePort, int destinationPort) {
            srcPort = sourcePort;
            dstPort = destinationPort;
        }
    }

    public static PortPair getSrcDestPortPair(byte[] outboundIkePkt) throws Exception {
        return new PortPair(
                getPort(outboundIkePkt, true /* shouldGetSource */),
                getPort(outboundIkePkt, false /* shouldGetSource */));
    }

    private static InetAddress getAddress(byte[] pkt, boolean shouldGetSource) throws Exception {
        int ipLen = isIpv6(pkt) ? IP6_ADDR_LEN : IP4_ADDR_LEN;
        int srcIpOffset = isIpv6(pkt) ? IP6_ADDR_OFFSET : IP4_ADDR_OFFSET;
        int ipOffset = shouldGetSource ? srcIpOffset : srcIpOffset + ipLen;

        ByteBuffer buffer = ByteBuffer.wrap(pkt);
        buffer.get(new byte[ipOffset]);
        byte[] ipAddrBytes = new byte[ipLen];
        buffer.get(ipAddrBytes);
        return InetAddress.getByAddress(ipAddrBytes);
    }

    private static int getPort(byte[] pkt, boolean shouldGetSource) {
        ByteBuffer buffer = ByteBuffer.wrap(pkt);
        int srcPortOffset = isIpv6(pkt) ? IP6_HDRLEN : IP4_HDRLEN;
        int portOffset = shouldGetSource ? srcPortOffset : srcPortOffset + PORT_LEN;

        buffer.get(new byte[portOffset]);
        return Short.toUnsignedInt(buffer.getShort());
    }

    public static byte[] buildIkePacket(
            InetAddress srcAddr,
            InetAddress dstAddr,
            int srcPort,
            int dstPort,
            boolean useEncap,
            byte[] ikePacket)
            throws Exception {
        if (useEncap) {
            ByteBuffer buffer = ByteBuffer.allocate(NON_ESP_MARKER_LEN + ikePacket.length);
            buffer.put(NON_ESP_MARKER);
            buffer.put(ikePacket);
            ikePacket = buffer.array();
        }

        UdpHeader udpPkt = new UdpHeader(srcPort, dstPort, new BytePayload(ikePacket));
        IpHeader ipPkt = getIpHeader(udpPkt.getProtocolId(), srcAddr, dstAddr, udpPkt);
        return ipPkt.getPacketBytes();
    }

    private static IpHeader getIpHeader(
            int protocol, InetAddress src, InetAddress dst, Payload payload) {
        if ((src instanceof Inet6Address) != (dst instanceof Inet6Address)) {
            throw new IllegalArgumentException("Invalid src/dst address combination");
        }

        if (src instanceof Inet6Address) {
            return new Ip6Header(protocol, (Inet6Address) src, (Inet6Address) dst, payload);
        } else {
            return new Ip4Header(protocol, (Inet4Address) src, (Inet4Address) dst, payload);
        }
    }
}
