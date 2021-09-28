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

import static android.system.OsConstants.ETH_P_ARP;
import static android.system.OsConstants.ETH_P_IP;

import static com.android.server.util.NetworkStackConstants.ARP_ETHER_IPV4_LEN;
import static com.android.server.util.NetworkStackConstants.ARP_HWTYPE_ETHER;
import static com.android.server.util.NetworkStackConstants.ARP_REPLY;
import static com.android.server.util.NetworkStackConstants.ARP_REQUEST;
import static com.android.server.util.NetworkStackConstants.ETHER_ADDR_LEN;
import static com.android.server.util.NetworkStackConstants.IPV4_ADDR_LEN;

import android.net.MacAddress;

import com.android.internal.annotations.VisibleForTesting;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;

/**
 * Defines basic data and operations needed to build and parse packets for the
 * ARP protocol.
 *
 * @hide
 */
public class ArpPacket {
    private static final String TAG = "ArpPacket";

    public final short opCode;
    public final Inet4Address senderIp;
    public final Inet4Address targetIp;
    public final MacAddress senderHwAddress;
    public final MacAddress targetHwAddress;

    ArpPacket(short opCode, MacAddress senderHwAddress, Inet4Address senderIp,
            MacAddress targetHwAddress, Inet4Address targetIp) {
        this.opCode = opCode;
        this.senderHwAddress = senderHwAddress;
        this.senderIp = senderIp;
        this.targetHwAddress = targetHwAddress;
        this.targetIp = targetIp;
    }

    /**
     * Build an ARP packet from the required specified parameters.
     */
    @VisibleForTesting
    public static ByteBuffer buildArpPacket(final byte[] dstMac, final byte[] srcMac,
            final byte[] targetIp, final byte[] targetHwAddress, byte[] senderIp,
            final short opCode) {
        final ByteBuffer buf = ByteBuffer.allocate(ARP_ETHER_IPV4_LEN);

        // Ether header
        buf.put(dstMac);
        buf.put(srcMac);
        buf.putShort((short) ETH_P_ARP);

        // ARP header
        buf.putShort((short) ARP_HWTYPE_ETHER);  // hrd
        buf.putShort((short) ETH_P_IP);          // pro
        buf.put((byte) ETHER_ADDR_LEN);          // hln
        buf.put((byte) IPV4_ADDR_LEN);           // pln
        buf.putShort(opCode);                    // op
        buf.put(srcMac);                         // sha
        buf.put(senderIp);                       // spa
        buf.put(targetHwAddress);                // tha
        buf.put(targetIp);                       // tpa
        buf.flip();
        return buf;
    }

    /**
     * Parse an ARP packet from an ByteBuffer object.
     */
    @VisibleForTesting
    public static ArpPacket parseArpPacket(final byte[] recvbuf, final int length)
            throws ParseException {
        try {
            if (length < ARP_ETHER_IPV4_LEN || recvbuf.length < length) {
                throw new ParseException("Invalid packet length: " + length);
            }

            final ByteBuffer buffer = ByteBuffer.wrap(recvbuf, 0, length);
            byte[] l2dst = new byte[ETHER_ADDR_LEN];
            byte[] l2src = new byte[ETHER_ADDR_LEN];
            buffer.get(l2dst);
            buffer.get(l2src);

            final short etherType = buffer.getShort();
            if (etherType != ETH_P_ARP) {
                throw new ParseException("Incorrect Ether Type: " + etherType);
            }

            final short hwType = buffer.getShort();
            if (hwType != ARP_HWTYPE_ETHER) {
                throw new ParseException("Incorrect HW Type: " + hwType);
            }

            final short protoType = buffer.getShort();
            if (protoType != ETH_P_IP) {
                throw new ParseException("Incorrect Protocol Type: " + protoType);
            }

            final byte hwAddrLength = buffer.get();
            if (hwAddrLength != ETHER_ADDR_LEN) {
                throw new ParseException("Incorrect HW address length: " + hwAddrLength);
            }

            final byte ipAddrLength = buffer.get();
            if (ipAddrLength != IPV4_ADDR_LEN) {
                throw new ParseException("Incorrect Protocol address length: " + ipAddrLength);
            }

            final short opCode = buffer.getShort();
            if (opCode != ARP_REQUEST && opCode != ARP_REPLY) {
                throw new ParseException("Incorrect opCode: " + opCode);
            }

            byte[] senderHwAddress = new byte[ETHER_ADDR_LEN];
            byte[] senderIp = new byte[IPV4_ADDR_LEN];
            buffer.get(senderHwAddress);
            buffer.get(senderIp);

            byte[] targetHwAddress = new byte[ETHER_ADDR_LEN];
            byte[] targetIp = new byte[IPV4_ADDR_LEN];
            buffer.get(targetHwAddress);
            buffer.get(targetIp);

            return new ArpPacket(opCode, MacAddress.fromBytes(senderHwAddress),
                    (Inet4Address) InetAddress.getByAddress(senderIp),
                    MacAddress.fromBytes(targetHwAddress),
                    (Inet4Address) InetAddress.getByAddress(targetIp));
        } catch (IndexOutOfBoundsException e) {
            throw new ParseException("Invalid index when wrapping a byte array into a buffer");
        } catch (BufferUnderflowException e) {
            throw new ParseException("Invalid buffer position");
        } catch (IllegalArgumentException e) {
            throw new ParseException("Invalid MAC address representation");
        } catch (UnknownHostException e) {
            throw new ParseException("Invalid IP address of Host");
        }
    }

    /**
     * Thrown when parsing ARP packet failed.
     */
    public static class ParseException extends Exception {
        ParseException(String message) {
            super(message);
        }
    }
}
