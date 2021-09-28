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

package android.net.cts;

import static android.system.OsConstants.IPPROTO_IPV6;
import static android.system.OsConstants.IPPROTO_UDP;

import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.ShortBuffer;
import java.security.GeneralSecurityException;
import java.security.SecureRandom;
import java.util.Arrays;

import javax.crypto.Cipher;
import javax.crypto.Mac;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

public class PacketUtils {
    private static final String TAG = PacketUtils.class.getSimpleName();

    private static final int DATA_BUFFER_LEN = 4096;

    static final int IP4_HDRLEN = 20;
    static final int IP6_HDRLEN = 40;
    static final int UDP_HDRLEN = 8;
    static final int TCP_HDRLEN = 20;
    static final int TCP_HDRLEN_WITH_TIMESTAMP_OPT = TCP_HDRLEN + 12;

    // Not defined in OsConstants
    static final int IPPROTO_IPV4 = 4;
    static final int IPPROTO_ESP = 50;

    // Encryption parameters
    static final int AES_GCM_IV_LEN = 8;
    static final int AES_CBC_IV_LEN = 16;
    static final int AES_GCM_BLK_SIZE = 4;
    static final int AES_CBC_BLK_SIZE = 16;

    // Encryption algorithms
    static final String AES = "AES";
    static final String AES_CBC = "AES/CBC/NoPadding";
    static final String HMAC_SHA_256 = "HmacSHA256";

    public interface Payload {
        byte[] getPacketBytes(IpHeader header) throws Exception;

        void addPacketBytes(IpHeader header, ByteBuffer resultBuffer) throws Exception;

        short length();

        int getProtocolId();
    }

    public abstract static class IpHeader {

        public final byte proto;
        public final InetAddress srcAddr;
        public final InetAddress dstAddr;
        public final Payload payload;

        public IpHeader(int proto, InetAddress src, InetAddress dst, Payload payload) {
            this.proto = (byte) proto;
            this.srcAddr = src;
            this.dstAddr = dst;
            this.payload = payload;
        }

        public abstract byte[] getPacketBytes() throws Exception;

        public abstract int getProtocolId();
    }

    public static class Ip4Header extends IpHeader {
        private short checksum;

        public Ip4Header(int proto, Inet4Address src, Inet4Address dst, Payload payload) {
            super(proto, src, dst, payload);
        }

        public byte[] getPacketBytes() throws Exception {
            ByteBuffer resultBuffer = buildHeader();
            payload.addPacketBytes(this, resultBuffer);

            return getByteArrayFromBuffer(resultBuffer);
        }

        public ByteBuffer buildHeader() {
            ByteBuffer bb = ByteBuffer.allocate(DATA_BUFFER_LEN);

            // Version, IHL
            bb.put((byte) (0x45));

            // DCSP, ECN
            bb.put((byte) 0);

            // Total Length
            bb.putShort((short) (IP4_HDRLEN + payload.length()));

            // Empty for Identification, Flags and Fragment Offset
            bb.putShort((short) 0);
            bb.put((byte) 0x40);
            bb.put((byte) 0x00);

            // TTL
            bb.put((byte) 64);

            // Protocol
            bb.put(proto);

            // Header Checksum
            final int ipChecksumOffset = bb.position();
            bb.putShort((short) 0);

            // Src/Dst addresses
            bb.put(srcAddr.getAddress());
            bb.put(dstAddr.getAddress());

            bb.putShort(ipChecksumOffset, calculateChecksum(bb));

            return bb;
        }

        private short calculateChecksum(ByteBuffer bb) {
            int checksum = 0;

            // Calculate sum of 16-bit values, excluding checksum. IPv4 headers are always 32-bit
            // aligned, so no special cases needed for unaligned values.
            ShortBuffer shortBuffer = ByteBuffer.wrap(getByteArrayFromBuffer(bb)).asShortBuffer();
            while (shortBuffer.hasRemaining()) {
                short val = shortBuffer.get();

                // Wrap as needed
                checksum = addAndWrapForChecksum(checksum, val);
            }

            return onesComplement(checksum);
        }

        public int getProtocolId() {
            return IPPROTO_IPV4;
        }
    }

    public static class Ip6Header extends IpHeader {
        public Ip6Header(int nextHeader, Inet6Address src, Inet6Address dst, Payload payload) {
            super(nextHeader, src, dst, payload);
        }

        public byte[] getPacketBytes() throws Exception {
            ByteBuffer bb = ByteBuffer.allocate(DATA_BUFFER_LEN);

            // Version | Traffic Class (First 4 bits)
            bb.put((byte) 0x60);

            // Traffic class (Last 4 bits), Flow Label
            bb.put((byte) 0);
            bb.put((byte) 0);
            bb.put((byte) 0);

            // Payload Length
            bb.putShort((short) payload.length());

            // Next Header
            bb.put(proto);

            // Hop Limit
            bb.put((byte) 64);

            // Src/Dst addresses
            bb.put(srcAddr.getAddress());
            bb.put(dstAddr.getAddress());

            // Payload
            payload.addPacketBytes(this, bb);

            return getByteArrayFromBuffer(bb);
        }

        public int getProtocolId() {
            return IPPROTO_IPV6;
        }
    }

    public static class BytePayload implements Payload {
        public final byte[] payload;

        public BytePayload(byte[] payload) {
            this.payload = payload;
        }

        public int getProtocolId() {
            return -1;
        }

        public byte[] getPacketBytes(IpHeader header) {
            ByteBuffer bb = ByteBuffer.allocate(DATA_BUFFER_LEN);

            addPacketBytes(header, bb);
            return getByteArrayFromBuffer(bb);
        }

        public void addPacketBytes(IpHeader header, ByteBuffer resultBuffer) {
            resultBuffer.put(payload);
        }

        public short length() {
            return (short) payload.length;
        }
    }

    public static class UdpHeader implements Payload {

        public final short srcPort;
        public final short dstPort;
        public final Payload payload;

        public UdpHeader(int srcPort, int dstPort, Payload payload) {
            this.srcPort = (short) srcPort;
            this.dstPort = (short) dstPort;
            this.payload = payload;
        }

        public int getProtocolId() {
            return IPPROTO_UDP;
        }

        public short length() {
            return (short) (payload.length() + 8);
        }

        public byte[] getPacketBytes(IpHeader header) throws Exception {
            ByteBuffer bb = ByteBuffer.allocate(DATA_BUFFER_LEN);

            addPacketBytes(header, bb);
            return getByteArrayFromBuffer(bb);
        }

        public void addPacketBytes(IpHeader header, ByteBuffer resultBuffer) throws Exception {
            // Source, Destination port
            resultBuffer.putShort(srcPort);
            resultBuffer.putShort(dstPort);

            // Payload Length
            resultBuffer.putShort(length());

            // Get payload bytes for checksum + payload
            ByteBuffer payloadBuffer = ByteBuffer.allocate(DATA_BUFFER_LEN);
            payload.addPacketBytes(header, payloadBuffer);
            byte[] payloadBytes = getByteArrayFromBuffer(payloadBuffer);

            // Checksum
            resultBuffer.putShort(calculateChecksum(header, payloadBytes));

            // Payload
            resultBuffer.put(payloadBytes);
        }

        private short calculateChecksum(IpHeader header, byte[] payloadBytes) throws Exception {
            int newChecksum = 0;
            ShortBuffer srcBuffer = ByteBuffer.wrap(header.srcAddr.getAddress()).asShortBuffer();
            ShortBuffer dstBuffer = ByteBuffer.wrap(header.dstAddr.getAddress()).asShortBuffer();

            while (srcBuffer.hasRemaining() || dstBuffer.hasRemaining()) {
                short val = srcBuffer.hasRemaining() ? srcBuffer.get() : dstBuffer.get();

                // Wrap as needed
                newChecksum = addAndWrapForChecksum(newChecksum, val);
            }

            // Add pseudo-header values. Proto is 0-padded, so just use the byte.
            newChecksum = addAndWrapForChecksum(newChecksum, header.proto);
            newChecksum = addAndWrapForChecksum(newChecksum, length());
            newChecksum = addAndWrapForChecksum(newChecksum, srcPort);
            newChecksum = addAndWrapForChecksum(newChecksum, dstPort);
            newChecksum = addAndWrapForChecksum(newChecksum, length());

            ShortBuffer payloadShortBuffer = ByteBuffer.wrap(payloadBytes).asShortBuffer();
            while (payloadShortBuffer.hasRemaining()) {
                newChecksum = addAndWrapForChecksum(newChecksum, payloadShortBuffer.get());
            }
            if (payload.length() % 2 != 0) {
                newChecksum =
                        addAndWrapForChecksum(
                                newChecksum, (payloadBytes[payloadBytes.length - 1] << 8));
            }

            return onesComplement(newChecksum);
        }
    }

    public static class EspHeader implements Payload {
        public final int nextHeader;
        public final int spi;
        public final int seqNum;
        public final byte[] key;
        public final byte[] payload;

        /**
         * Generic constructor for ESP headers.
         *
         * <p>For Tunnel mode, payload will be a full IP header + attached payloads
         *
         * <p>For Transport mode, payload will be only the attached payloads, but with the checksum
         * calculated using the pre-encryption IP header
         */
        public EspHeader(int nextHeader, int spi, int seqNum, byte[] key, byte[] payload) {
            this.nextHeader = nextHeader;
            this.spi = spi;
            this.seqNum = seqNum;
            this.key = key;
            this.payload = payload;
        }

        public int getProtocolId() {
            return IPPROTO_ESP;
        }

        public short length() {
            // ALWAYS uses AES-CBC, HMAC-SHA256 (128b trunc len)
            return (short)
                    calculateEspPacketSize(payload.length, AES_CBC_IV_LEN, AES_CBC_BLK_SIZE, 128);
        }

        public byte[] getPacketBytes(IpHeader header) throws Exception {
            ByteBuffer bb = ByteBuffer.allocate(DATA_BUFFER_LEN);

            addPacketBytes(header, bb);
            return getByteArrayFromBuffer(bb);
        }

        public void addPacketBytes(IpHeader header, ByteBuffer resultBuffer) throws Exception {
            ByteBuffer espPayloadBuffer = ByteBuffer.allocate(DATA_BUFFER_LEN);
            espPayloadBuffer.putInt(spi);
            espPayloadBuffer.putInt(seqNum);
            espPayloadBuffer.put(getCiphertext(key));

            espPayloadBuffer.put(getIcv(getByteArrayFromBuffer(espPayloadBuffer)), 0, 16);
            resultBuffer.put(getByteArrayFromBuffer(espPayloadBuffer));
        }

        private byte[] getIcv(byte[] authenticatedSection) throws GeneralSecurityException {
            Mac sha256HMAC = Mac.getInstance(HMAC_SHA_256);
            SecretKeySpec authKey = new SecretKeySpec(key, HMAC_SHA_256);
            sha256HMAC.init(authKey);

            return sha256HMAC.doFinal(authenticatedSection);
        }

        /**
         * Encrypts and builds ciphertext block. Includes the IV, Padding and Next-Header blocks
         *
         * <p>The ciphertext does NOT include the SPI/Sequence numbers, or the ICV.
         */
        private byte[] getCiphertext(byte[] key) throws GeneralSecurityException {
            int paddedLen = calculateEspEncryptedLength(payload.length, AES_CBC_BLK_SIZE);
            ByteBuffer paddedPayload = ByteBuffer.allocate(paddedLen);
            paddedPayload.put(payload);

            // Add padding - consecutive integers from 0x01
            int pad = 1;
            while (paddedPayload.position() < paddedPayload.limit()) {
                paddedPayload.put((byte) pad++);
            }

            paddedPayload.position(paddedPayload.limit() - 2);
            paddedPayload.put((byte) (paddedLen - 2 - payload.length)); // Pad length
            paddedPayload.put((byte) nextHeader);

            // Generate Initialization Vector
            byte[] iv = new byte[AES_CBC_IV_LEN];
            new SecureRandom().nextBytes(iv);
            IvParameterSpec ivParameterSpec = new IvParameterSpec(iv);
            SecretKeySpec secretKeySpec = new SecretKeySpec(key, AES);

            // Encrypt payload
            Cipher cipher = Cipher.getInstance(AES_CBC);
            cipher.init(Cipher.ENCRYPT_MODE, secretKeySpec, ivParameterSpec);
            byte[] encrypted = cipher.doFinal(getByteArrayFromBuffer(paddedPayload));

            // Build ciphertext
            ByteBuffer cipherText = ByteBuffer.allocate(AES_CBC_IV_LEN + encrypted.length);
            cipherText.put(iv);
            cipherText.put(encrypted);

            return getByteArrayFromBuffer(cipherText);
        }
    }

    private static int addAndWrapForChecksum(int currentChecksum, int value) {
        currentChecksum += value & 0x0000ffff;

        // Wrap anything beyond the first 16 bits, and add to lower order bits
        return (currentChecksum >>> 16) + (currentChecksum & 0x0000ffff);
    }

    private static short onesComplement(int val) {
        val = (val >>> 16) + (val & 0xffff);

        if (val == 0) return 0;
        return (short) ((~val) & 0xffff);
    }

    public static int calculateEspPacketSize(
            int payloadLen, int cryptIvLength, int cryptBlockSize, int authTruncLen) {
        final int ESP_HDRLEN = 4 + 4; // SPI + Seq#
        final int ICV_LEN = authTruncLen / 8; // Auth trailer; based on truncation length
        payloadLen += cryptIvLength; // Initialization Vector

        // Align to block size of encryption algorithm
        payloadLen = calculateEspEncryptedLength(payloadLen, cryptBlockSize);
        return payloadLen + ESP_HDRLEN + ICV_LEN;
    }

    private static int calculateEspEncryptedLength(int payloadLen, int cryptBlockSize) {
        payloadLen += 2; // ESP trailer

        // Align to block size of encryption algorithm
        return payloadLen + calculateEspPadLen(payloadLen, cryptBlockSize);
    }

    private static int calculateEspPadLen(int payloadLen, int cryptBlockSize) {
        return (cryptBlockSize - (payloadLen % cryptBlockSize)) % cryptBlockSize;
    }

    private static byte[] getByteArrayFromBuffer(ByteBuffer buffer) {
        return Arrays.copyOfRange(buffer.array(), 0, buffer.position());
    }

    public static IpHeader getIpHeader(
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

    /*
     * Debug printing
     */
    private static final char[] hexArray = "0123456789ABCDEF".toCharArray();

    public static String bytesToHex(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) {
            sb.append(hexArray[b >>> 4]);
            sb.append(hexArray[b & 0x0F]);
            sb.append(' ');
        }
        return sb.toString();
    }
}
