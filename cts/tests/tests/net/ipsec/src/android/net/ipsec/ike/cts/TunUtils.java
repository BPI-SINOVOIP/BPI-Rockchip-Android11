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

import static android.net.ipsec.ike.cts.PacketUtils.IP4_HDRLEN;
import static android.net.ipsec.ike.cts.PacketUtils.IP6_HDRLEN;
import static android.net.ipsec.ike.cts.PacketUtils.IPPROTO_ESP;
import static android.net.ipsec.ike.cts.PacketUtils.UDP_HDRLEN;
import static android.system.OsConstants.IPPROTO_UDP;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.fail;

import android.os.ParcelFileDescriptor;

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.function.Predicate;

/**
 * This code is a exact copy of {@link TunUtils} in
 * cts/tests/tests/net/src/android/net/cts/TunUtils.java, except the import path of PacketUtils is
 * the path to the copy of PacktUtils.
 *
 * <p>TODO(b/148689509): Statically include the TunUtils source file instead of copying it.
 */
public class TunUtils {
    private static final String TAG = TunUtils.class.getSimpleName();

    private static final int DATA_BUFFER_LEN = 4096;
    static final int TIMEOUT = 500;

    static final int IP4_PROTO_OFFSET = 9;
    static final int IP6_PROTO_OFFSET = 6;

    static final int IP4_ADDR_OFFSET = 12;
    static final int IP4_ADDR_LEN = 4;
    static final int IP6_ADDR_OFFSET = 8;
    static final int IP6_ADDR_LEN = 16;

    final List<byte[]> mPackets = new ArrayList<>();
    private final ParcelFileDescriptor mTunFd;
    private final Thread mReaderThread;

    public TunUtils(ParcelFileDescriptor tunFd) {
        mTunFd = tunFd;

        // Start background reader thread
        mReaderThread =
                new Thread(
                        () -> {
                            try {
                                // Loop will exit and thread will quit when tunFd is closed.
                                // Receiving either EOF or an exception will exit this reader loop.
                                // FileInputStream in uninterruptable, so there's no good way to
                                // ensure that this thread shuts down except upon FD closure.
                                while (true) {
                                    byte[] intercepted = receiveFromTun();
                                    if (intercepted == null) {
                                        // Exit once we've hit EOF
                                        return;
                                    } else if (intercepted.length > 0) {
                                        // Only save packet if we've received any bytes.
                                        synchronized (mPackets) {
                                            mPackets.add(intercepted);
                                            mPackets.notifyAll();
                                        }
                                    }
                                }
                            } catch (IOException ignored) {
                                // Simply exit this reader thread
                                return;
                            }
                        });
        mReaderThread.start();
    }

    private byte[] receiveFromTun() throws IOException {
        FileInputStream in = new FileInputStream(mTunFd.getFileDescriptor());
        byte[] inBytes = new byte[DATA_BUFFER_LEN];
        int bytesRead = in.read(inBytes);

        if (bytesRead < 0) {
            return null; // return null for EOF
        } else if (bytesRead >= DATA_BUFFER_LEN) {
            throw new IllegalStateException("Too big packet. Fragmentation unsupported");
        }
        return Arrays.copyOf(inBytes, bytesRead);
    }

    byte[] getFirstMatchingPacket(Predicate<byte[]> verifier, int startIndex) {
        synchronized (mPackets) {
            for (int i = startIndex; i < mPackets.size(); i++) {
                byte[] pkt = mPackets.get(i);
                if (verifier.test(pkt)) {
                    return pkt;
                }
            }
        }
        return null;
    }

    /**
     * Checks if the specified bytes were ever sent in plaintext.
     *
     * <p>Only checks for known plaintext bytes to prevent triggering on ICMP/RA packets or the like
     *
     * @param plaintext the plaintext bytes to check for
     * @param startIndex the index in the list to check for
     */
    public boolean hasPlaintextPacket(byte[] plaintext, int startIndex) {
        Predicate<byte[]> verifier =
                (pkt) -> {
                    return Collections.indexOfSubList(Arrays.asList(pkt), Arrays.asList(plaintext))
                            != -1;
                };
        return getFirstMatchingPacket(verifier, startIndex) != null;
    }

    public byte[] getEspPacket(int spi, boolean encap, int startIndex) {
        return getFirstMatchingPacket(
                (pkt) -> {
                    return isEsp(pkt, spi, encap);
                },
                startIndex);
    }

    public byte[] awaitEspPacketNoPlaintext(
            int spi, byte[] plaintext, boolean useEncap, int expectedPacketSize) throws Exception {
        long endTime = System.currentTimeMillis() + TIMEOUT;
        int startIndex = 0;

        synchronized (mPackets) {
            while (System.currentTimeMillis() < endTime) {
                byte[] espPkt = getEspPacket(spi, useEncap, startIndex);
                if (espPkt != null) {
                    // Validate packet size
                    assertEquals(expectedPacketSize, espPkt.length);

                    // Always check plaintext from start
                    assertFalse(hasPlaintextPacket(plaintext, 0));
                    return espPkt; // We've found the packet we're looking for.
                }

                startIndex = mPackets.size();

                // Try to prevent waiting too long. If waitTimeout <= 0, we've already hit timeout
                long waitTimeout = endTime - System.currentTimeMillis();
                if (waitTimeout > 0) {
                    mPackets.wait(waitTimeout);
                }
            }

            fail("No such ESP packet found with SPI " + spi);
        }
        return null;
    }

    private static boolean isSpiEqual(byte[] pkt, int espOffset, int spi) {
        // Check SPI byte by byte.
        return pkt[espOffset] == (byte) ((spi >>> 24) & 0xff)
                && pkt[espOffset + 1] == (byte) ((spi >>> 16) & 0xff)
                && pkt[espOffset + 2] == (byte) ((spi >>> 8) & 0xff)
                && pkt[espOffset + 3] == (byte) (spi & 0xff);
    }

    private static boolean isEsp(byte[] pkt, int spi, boolean encap) {
        if (isIpv6(pkt)) {
            // IPv6 UDP encap not supported by kernels; assume non-encap.
            return pkt[IP6_PROTO_OFFSET] == IPPROTO_ESP && isSpiEqual(pkt, IP6_HDRLEN, spi);
        } else {
            // Use default IPv4 header length (assuming no options)
            if (encap) {
                return pkt[IP4_PROTO_OFFSET] == IPPROTO_UDP
                        && isSpiEqual(pkt, IP4_HDRLEN + UDP_HDRLEN, spi);
            } else {
                return pkt[IP4_PROTO_OFFSET] == IPPROTO_ESP && isSpiEqual(pkt, IP4_HDRLEN, spi);
            }
        }
    }

    static boolean isIpv6(byte[] pkt) {
        // First nibble shows IP version. 0x60 for IPv6
        return (pkt[0] & (byte) 0xF0) == (byte) 0x60;
    }

    private static byte[] getReflectedPacket(byte[] pkt) {
        byte[] reflected = Arrays.copyOf(pkt, pkt.length);

        if (isIpv6(pkt)) {
            // Set reflected packet's dst to that of the original's src
            System.arraycopy(
                    pkt, // src
                    IP6_ADDR_OFFSET + IP6_ADDR_LEN, // src offset
                    reflected, // dst
                    IP6_ADDR_OFFSET, // dst offset
                    IP6_ADDR_LEN); // len
            // Set reflected packet's src IP to that of the original's dst IP
            System.arraycopy(
                    pkt, // src
                    IP6_ADDR_OFFSET, // src offset
                    reflected, // dst
                    IP6_ADDR_OFFSET + IP6_ADDR_LEN, // dst offset
                    IP6_ADDR_LEN); // len
        } else {
            // Set reflected packet's dst to that of the original's src
            System.arraycopy(
                    pkt, // src
                    IP4_ADDR_OFFSET + IP4_ADDR_LEN, // src offset
                    reflected, // dst
                    IP4_ADDR_OFFSET, // dst offset
                    IP4_ADDR_LEN); // len
            // Set reflected packet's src IP to that of the original's dst IP
            System.arraycopy(
                    pkt, // src
                    IP4_ADDR_OFFSET, // src offset
                    reflected, // dst
                    IP4_ADDR_OFFSET + IP4_ADDR_LEN, // dst offset
                    IP4_ADDR_LEN); // len
        }
        return reflected;
    }

    /** Takes all captured packets, flips the src/dst, and re-injects them. */
    public void reflectPackets() throws IOException {
        synchronized (mPackets) {
            for (byte[] pkt : mPackets) {
                injectPacket(getReflectedPacket(pkt));
            }
        }
    }

    public void injectPacket(byte[] pkt) throws IOException {
        FileOutputStream out = new FileOutputStream(mTunFd.getFileDescriptor());
        out.write(pkt);
        out.flush();
    }

    /** Resets the intercepted packets. */
    public void reset() throws IOException {
        synchronized (mPackets) {
            mPackets.clear();
        }
    }
}
