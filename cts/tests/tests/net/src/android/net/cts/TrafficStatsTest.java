/*
 * Copyright (C) 2010 The Android Open Source Project
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

import android.net.NetworkStats;
import android.net.TrafficStats;
import android.os.Process;
import android.platform.test.annotations.AppModeFull;
import android.test.AndroidTestCase;
import android.util.Log;
import android.util.Range;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class TrafficStatsTest extends AndroidTestCase {
    private static final String LOG_TAG = "TrafficStatsTest";

    /** Verify the given value is in range [lower, upper] */
    private void assertInRange(String tag, long value, long lower, long upper) {
        final Range range = new Range(lower, upper);
        assertTrue(tag + ": " + value + " is not within range [" + lower + ", " + upper + "]",
                range.contains(value));
    }

    public void testValidMobileStats() {
        // We can't assume a mobile network is even present in this test, so
        // we simply assert that a valid value is returned.

        assertTrue(TrafficStats.getMobileTxPackets() >= 0);
        assertTrue(TrafficStats.getMobileRxPackets() >= 0);
        assertTrue(TrafficStats.getMobileTxBytes() >= 0);
        assertTrue(TrafficStats.getMobileRxBytes() >= 0);
    }

    public void testValidTotalStats() {
        assertTrue(TrafficStats.getTotalTxPackets() >= 0);
        assertTrue(TrafficStats.getTotalRxPackets() >= 0);
        assertTrue(TrafficStats.getTotalTxBytes() >= 0);
        assertTrue(TrafficStats.getTotalRxBytes() >= 0);
    }

    public void testValidPacketStats() {
        assertTrue(TrafficStats.getTxPackets("lo") >= 0);
        assertTrue(TrafficStats.getRxPackets("lo") >= 0);
    }

    public void testThreadStatsTag() throws Exception {
        TrafficStats.setThreadStatsTag(0xf00d);
        assertTrue("Tag didn't stick", TrafficStats.getThreadStatsTag() == 0xf00d);

        final CountDownLatch latch = new CountDownLatch(1);

        new Thread("TrafficStatsTest.testThreadStatsTag") {
            @Override
            public void run() {
                assertTrue("Tag leaked", TrafficStats.getThreadStatsTag() != 0xf00d);
                TrafficStats.setThreadStatsTag(0xcafe);
                assertTrue("Tag didn't stick", TrafficStats.getThreadStatsTag() == 0xcafe);
                latch.countDown();
            }
        }.start();

        latch.await(5, TimeUnit.SECONDS);
        assertTrue("Tag lost", TrafficStats.getThreadStatsTag() == 0xf00d);

        TrafficStats.clearThreadStatsTag();
        assertTrue("Tag not cleared", TrafficStats.getThreadStatsTag() != 0xf00d);
    }

    long tcpPacketToIpBytes(long packetCount, long bytes) {
        // ip header + tcp header + data.
        // Tcp header is mostly 32. Syn has different tcp options -> 40. Don't care.
        return packetCount * (20 + 32 + bytes);
    }

    @AppModeFull(reason = "Socket cannot bind in instant app mode")
    public void testTrafficStatsForLocalhost() throws IOException {
        final long mobileTxPacketsBefore = TrafficStats.getMobileTxPackets();
        final long mobileRxPacketsBefore = TrafficStats.getMobileRxPackets();
        final long mobileTxBytesBefore = TrafficStats.getMobileTxBytes();
        final long mobileRxBytesBefore = TrafficStats.getMobileRxBytes();
        final long totalTxPacketsBefore = TrafficStats.getTotalTxPackets();
        final long totalRxPacketsBefore = TrafficStats.getTotalRxPackets();
        final long totalTxBytesBefore = TrafficStats.getTotalTxBytes();
        final long totalRxBytesBefore = TrafficStats.getTotalRxBytes();
        final long uidTxBytesBefore = TrafficStats.getUidTxBytes(Process.myUid());
        final long uidRxBytesBefore = TrafficStats.getUidRxBytes(Process.myUid());
        final long uidTxPacketsBefore = TrafficStats.getUidTxPackets(Process.myUid());
        final long uidRxPacketsBefore = TrafficStats.getUidRxPackets(Process.myUid());
        final long ifaceTxPacketsBefore = TrafficStats.getTxPackets("lo");
        final long ifaceRxPacketsBefore = TrafficStats.getRxPackets("lo");

        // Transfer 1MB of data across an explicitly localhost socket.
        final int byteCount = 1024;
        final int packetCount = 1024;

        TrafficStats.startDataProfiling(null);
        final ServerSocket server = new ServerSocket(0);
        new Thread("TrafficStatsTest.testTrafficStatsForLocalhost") {
            @Override
            public void run() {
                try {
                    final Socket socket = new Socket("localhost", server.getLocalPort());
                    // Make sure that each write()+flush() turns into a packet:
                    // disable Nagle.
                    socket.setTcpNoDelay(true);
                    final OutputStream out = socket.getOutputStream();
                    final byte[] buf = new byte[byteCount];
                    TrafficStats.setThreadStatsTag(0x42);
                    TrafficStats.tagSocket(socket);
                    for (int i = 0; i < packetCount; i++) {
                        out.write(buf);
                        out.flush();
                        try {
                            // Bug: 10668088, Even with Nagle disabled, and flushing the 1024 bytes
                            // the kernel still regroups data into a larger packet.
                            Thread.sleep(5);
                        } catch (InterruptedException e) {
                        }
                    }
                    out.close();
                    socket.close();
                } catch (IOException e) {
                    Log.i(LOG_TAG, "Badness during writes to socket: " + e);
                }
            }
        }.start();

        int read = 0;
        try {
            final Socket socket = server.accept();
            socket.setTcpNoDelay(true);
            TrafficStats.setThreadStatsTag(0x43);
            TrafficStats.tagSocket(socket);
            final InputStream in = socket.getInputStream();
            final byte[] buf = new byte[byteCount];
            while (read < byteCount * packetCount) {
                int n = in.read(buf);
                assertTrue("Unexpected EOF", n > 0);
                read += n;
            }
        } finally {
            server.close();
        }
        assertTrue("Not all data read back", read >= byteCount * packetCount);

        // It's too fast to call getUidTxBytes function.
        try {
            Thread.sleep(1000);
        } catch (InterruptedException e) {
        }
        final NetworkStats testStats = TrafficStats.stopDataProfiling(null);

        final long mobileTxPacketsAfter = TrafficStats.getMobileTxPackets();
        final long mobileRxPacketsAfter = TrafficStats.getMobileRxPackets();
        final long mobileTxBytesAfter = TrafficStats.getMobileTxBytes();
        final long mobileRxBytesAfter = TrafficStats.getMobileRxBytes();
        final long totalTxPacketsAfter = TrafficStats.getTotalTxPackets();
        final long totalRxPacketsAfter = TrafficStats.getTotalRxPackets();
        final long totalTxBytesAfter = TrafficStats.getTotalTxBytes();
        final long totalRxBytesAfter = TrafficStats.getTotalRxBytes();
        final long uidTxBytesAfter = TrafficStats.getUidTxBytes(Process.myUid());
        final long uidRxBytesAfter = TrafficStats.getUidRxBytes(Process.myUid());
        final long uidTxPacketsAfter = TrafficStats.getUidTxPackets(Process.myUid());
        final long uidRxPacketsAfter = TrafficStats.getUidRxPackets(Process.myUid());
        final long uidTxDeltaBytes = uidTxBytesAfter - uidTxBytesBefore;
        final long uidTxDeltaPackets = uidTxPacketsAfter - uidTxPacketsBefore;
        final long uidRxDeltaBytes = uidRxBytesAfter - uidRxBytesBefore;
        final long uidRxDeltaPackets = uidRxPacketsAfter - uidRxPacketsBefore;
        final long ifaceTxPacketsAfter = TrafficStats.getTxPackets("lo");
        final long ifaceRxPacketsAfter = TrafficStats.getRxPackets("lo");
        final long ifaceTxDeltaPackets = ifaceTxPacketsAfter - ifaceTxPacketsBefore;
        final long ifaceRxDeltaPackets = ifaceRxPacketsAfter - ifaceRxPacketsBefore;

        // Localhost traffic *does* count against per-UID stats.
        /*
         * Calculations:
         *  - bytes
         *   bytes is approx: packets * data + packets * acks;
         *   but sometimes there are less acks than packets, so we set a lower
         *   limit of 1 ack.
         *  - setup/teardown
         *   + 7 approx.: syn, syn-ack, ack, fin-ack, ack, fin-ack, ack;
         *   but sometimes the last find-acks just vanish, so we set a lower limit of +5.
         */
        final int maxExpectedExtraPackets = 7;
        final int minExpectedExtraPackets = 5;

        // Some other tests don't cleanup connections correctly.
        // They have the same UID, so we discount their lingering traffic
        // which happens only on non-localhost, such as TCP FIN retranmission packets
        final long deltaTxOtherPackets = (totalTxPacketsAfter - totalTxPacketsBefore)
                - uidTxDeltaPackets;
        final long deltaRxOtherPackets = (totalRxPacketsAfter - totalRxPacketsBefore)
                - uidRxDeltaPackets;
        if (deltaTxOtherPackets > 0 || deltaRxOtherPackets > 0) {
            Log.i(LOG_TAG, "lingering traffic data: " + deltaTxOtherPackets + "/"
                    + deltaRxOtherPackets);
        }

        // Check that the per-uid stats obtained from data profiling contain the expected values.
        // The data profiling snapshot is generated from the readNetworkStatsDetail() method in
        // networkStatsService, so it's possible to verify that the detailed stats for a given
        // uid are correct.
        final NetworkStats.Entry entry = testStats.getTotal(null, Process.myUid());
        final long pktBytes = tcpPacketToIpBytes(packetCount, byteCount);
        final long pktWithNoDataBytes = tcpPacketToIpBytes(packetCount, 0);
        final long minExpExtraPktBytes = tcpPacketToIpBytes(minExpectedExtraPackets, 0);
        final long maxExpExtraPktBytes = tcpPacketToIpBytes(maxExpectedExtraPackets, 0);
        final long deltaTxOtherPktBytes = tcpPacketToIpBytes(deltaTxOtherPackets, 0);
        final long deltaRxOtherPktBytes  = tcpPacketToIpBytes(deltaRxOtherPackets, 0);
        assertInRange("txPackets detail", entry.txPackets, packetCount + minExpectedExtraPackets,
                uidTxDeltaPackets);
        assertInRange("rxPackets detail", entry.rxPackets, packetCount + minExpectedExtraPackets,
                uidRxDeltaPackets);
        assertInRange("txBytes detail", entry.txBytes, pktBytes + minExpExtraPktBytes,
                uidTxDeltaBytes);
        assertInRange("rxBytes detail", entry.rxBytes, pktBytes + minExpExtraPktBytes,
                uidRxDeltaBytes);
        assertInRange("uidtxp", uidTxDeltaPackets, packetCount + minExpectedExtraPackets,
                packetCount + packetCount + maxExpectedExtraPackets + deltaTxOtherPackets);
        assertInRange("uidrxp", uidRxDeltaPackets, packetCount + minExpectedExtraPackets,
                packetCount + packetCount + maxExpectedExtraPackets + deltaRxOtherPackets);
        assertInRange("uidtxb", uidTxDeltaBytes, pktBytes + minExpExtraPktBytes,
                pktBytes + pktWithNoDataBytes + maxExpExtraPktBytes + deltaTxOtherPktBytes);
        assertInRange("uidrxb", uidRxDeltaBytes, pktBytes + minExpExtraPktBytes,
                pktBytes + pktWithNoDataBytes + maxExpExtraPktBytes + deltaRxOtherPktBytes);
        assertInRange("iftxp", ifaceTxDeltaPackets, packetCount + minExpectedExtraPackets,
                packetCount + packetCount + maxExpectedExtraPackets + deltaTxOtherPackets);
        assertInRange("ifrxp", ifaceRxDeltaPackets, packetCount + minExpectedExtraPackets,
                packetCount + packetCount + maxExpectedExtraPackets + deltaRxOtherPackets);

        // Localhost traffic *does* count against total stats.
        // Check the total stats increased after test data transfer over localhost has been made.
        assertTrue("ttxp: " + totalTxPacketsBefore + " -> " + totalTxPacketsAfter,
                totalTxPacketsAfter >= totalTxPacketsBefore + uidTxDeltaPackets);
        assertTrue("trxp: " + totalRxPacketsBefore + " -> " + totalRxPacketsAfter,
                totalRxPacketsAfter >= totalRxPacketsBefore + uidRxDeltaPackets);
        assertTrue("ttxb: " + totalTxBytesBefore + " -> " + totalTxBytesAfter,
                totalTxBytesAfter >= totalTxBytesBefore + uidTxDeltaBytes);
        assertTrue("trxb: " + totalRxBytesBefore + " -> " + totalRxBytesAfter,
                totalRxBytesAfter >= totalRxBytesBefore + uidRxDeltaBytes);
        assertTrue("iftxp: " + ifaceTxPacketsBefore + " -> " + ifaceTxPacketsAfter,
                totalTxPacketsAfter >= totalTxPacketsBefore + ifaceTxDeltaPackets);
        assertTrue("ifrxp: " + ifaceRxPacketsBefore + " -> " + ifaceRxPacketsAfter,
                totalRxPacketsAfter >= totalRxPacketsBefore + ifaceRxDeltaPackets);

        // Localhost traffic should *not* count against mobile stats,
        // There might be some other traffic, but nowhere near 1MB.
        assertInRange("mtxp", mobileTxPacketsAfter, mobileTxPacketsBefore,
                mobileTxPacketsBefore + 500);
        assertInRange("mrxp", mobileRxPacketsAfter, mobileRxPacketsBefore,
                mobileRxPacketsBefore + 500);
        assertInRange("mtxb", mobileTxBytesAfter, mobileTxBytesBefore,
                mobileTxBytesBefore + 200000);
        assertInRange("mrxb", mobileRxBytesAfter, mobileRxBytesBefore,
                mobileRxBytesBefore + 200000);
    }
}
