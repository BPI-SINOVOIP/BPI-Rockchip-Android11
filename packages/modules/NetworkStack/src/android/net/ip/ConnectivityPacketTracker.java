/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.net.ip;

import static android.net.util.SocketUtils.makePacketSocketAddress;
import static android.system.OsConstants.AF_PACKET;
import static android.system.OsConstants.ARPHRD_ETHER;
import static android.system.OsConstants.ETH_P_ALL;
import static android.system.OsConstants.SOCK_NONBLOCK;
import static android.system.OsConstants.SOCK_RAW;

import android.net.util.ConnectivityPacketSummary;
import android.net.util.InterfaceParams;
import android.net.util.NetworkStackUtils;
import android.net.util.PacketReader;
import android.os.Handler;
import android.os.SystemClock;
import android.system.ErrnoException;
import android.system.Os;
import android.text.TextUtils;
import android.util.LocalLog;
import android.util.Log;

import com.android.internal.util.HexDump;
import com.android.internal.util.TokenBucket;

import java.io.FileDescriptor;
import java.io.IOException;


/**
 * Critical connectivity packet tracking daemon.
 *
 * Tracks ARP, DHCPv4, and IPv6 RS/RA/NS/NA packets.
 *
 * This class's constructor, start() and stop() methods must only be called
 * from the same thread on which the passed in |log| is accessed.
 *
 * Log lines include a hexdump of the packet, which can be decoded via:
 *
 *     echo -n H3XSTR1NG | sed -e 's/\([0-9A-F][0-9A-F]\)/\1 /g' -e 's/^/000000 /'
 *                       | text2pcap - -
 *                       | tcpdump -n -vv -e -r -
 *
 * @hide
 */
public class ConnectivityPacketTracker {
    private static final String TAG = ConnectivityPacketTracker.class.getSimpleName();
    private static final boolean DBG = false;
    private static final String MARK_START = "--- START ---";
    private static final String MARK_STOP = "--- STOP ---";
    private static final String MARK_NAMED_START = "--- START (%s) ---";
    private static final String MARK_NAMED_STOP = "--- STOP (%s) ---";
    // Use a TokenBucket to limit CPU usage of logging packets in steady state.
    private static final int TOKEN_FILL_RATE = 50;   // Maximum one packet every 20ms.
    private static final int MAX_BURST_LENGTH = 100; // Maximum burst 100 packets.

    private final String mTag;
    private final LocalLog mLog;
    private final PacketReader mPacketListener;
    private final TokenBucket mTokenBucket = new TokenBucket(TOKEN_FILL_RATE, MAX_BURST_LENGTH);
    private long mLastRateLimitLogTimeMs = 0;
    private boolean mRunning;
    private String mDisplayName;

    public ConnectivityPacketTracker(Handler h, InterfaceParams ifParams, LocalLog log) {
        if (ifParams == null) throw new IllegalArgumentException("null InterfaceParams");

        mTag = TAG + "." + ifParams.name;
        mLog = log;
        mPacketListener = new PacketListener(h, ifParams);
    }

    public void start(String displayName) {
        mRunning = true;
        mDisplayName = displayName;
        mPacketListener.start();
    }

    public void stop() {
        mPacketListener.stop();
        mRunning = false;
        mDisplayName = null;
    }

    private final class PacketListener extends PacketReader {
        private final InterfaceParams mInterface;

        PacketListener(Handler h, InterfaceParams ifParams) {
            super(h, ifParams.defaultMtu);
            mInterface = ifParams;
        }

        @Override
        protected FileDescriptor createFd() {
            FileDescriptor s = null;
            try {
                s = Os.socket(AF_PACKET, SOCK_RAW | SOCK_NONBLOCK, 0);
                NetworkStackUtils.attachControlPacketFilter(s, ARPHRD_ETHER);
                Os.bind(s, makePacketSocketAddress(ETH_P_ALL, mInterface.index));
            } catch (ErrnoException | IOException e) {
                logError("Failed to create packet tracking socket: ", e);
                closeFd(s);
                return null;
            }
            return s;
        }

        @Override
        protected void handlePacket(byte[] recvbuf, int length) {
            if (!mTokenBucket.get()) {
                // Rate limited. Log once every second so the user knows packets are missing.
                final long now = SystemClock.elapsedRealtime();
                if (now >= mLastRateLimitLogTimeMs + 1000) {
                    addLogEntry("Warning: too many packets, rate-limiting to one every " +
                                TOKEN_FILL_RATE + "ms");
                    mLastRateLimitLogTimeMs = now;
                }
                return;
            }

            final String summary;
            try {
                summary = ConnectivityPacketSummary.summarize(mInterface.macAddr, recvbuf, length);
                if (summary == null) return;
            } catch (Exception e) {
                if (DBG) Log.d(mTag, "Error creating packet summary", e);
                return;
            }

            if (DBG) Log.d(mTag, summary);
            addLogEntry(summary + "\n[" + HexDump.toHexString(recvbuf, 0, length) + "]");
        }

        @Override
        protected void onStart() {
            final String msg = TextUtils.isEmpty(mDisplayName)
                    ? MARK_START
                    : String.format(MARK_NAMED_START, mDisplayName);
            mLog.log(msg);
        }

        @Override
        protected void onStop() {
            String msg = TextUtils.isEmpty(mDisplayName)
                    ? MARK_STOP
                    : String.format(MARK_NAMED_STOP, mDisplayName);
            if (!mRunning) msg += " (packet listener stopped unexpectedly)";
            mLog.log(msg);
        }

        @Override
        protected void logError(String msg, Exception e) {
            Log.e(mTag, msg, e);
            addLogEntry(msg + e);
        }

        private void addLogEntry(String entry) {
            mLog.log(entry);
        }
    }
}
