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
package com.android.networkstack.netlink;

import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.internal.annotations.VisibleForTesting;

import java.nio.BufferOverflowException;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.util.Objects;

/**
 * Class for tcp_info.
 *
 * Corresponds to {@code struct tcp_info} from bionic/libc/kernel/uapi/linux/tcp.h
 */
public class TcpInfo {
    public enum Field {
        STATE(Byte.BYTES),
        CASTATE(Byte.BYTES),
        RETRANSMITS(Byte.BYTES),
        PROBES(Byte.BYTES),
        BACKOFF(Byte.BYTES),
        OPTIONS(Byte.BYTES),
        WSCALE(Byte.BYTES),
        DELIVERY_RATE_APP_LIMITED(Byte.BYTES),
        RTO(Integer.BYTES),
        ATO(Integer.BYTES),
        SND_MSS(Integer.BYTES),
        RCV_MSS(Integer.BYTES),
        UNACKED(Integer.BYTES),
        SACKED(Integer.BYTES),
        LOST(Integer.BYTES),
        RETRANS(Integer.BYTES),
        FACKETS(Integer.BYTES),
        LAST_DATA_SENT(Integer.BYTES),
        LAST_ACK_SENT(Integer.BYTES),
        LAST_DATA_RECV(Integer.BYTES),
        LAST_ACK_RECV(Integer.BYTES),
        PMTU(Integer.BYTES),
        RCV_SSTHRESH(Integer.BYTES),
        RTT(Integer.BYTES),
        RTTVAR(Integer.BYTES),
        SND_SSTHRESH(Integer.BYTES),
        SND_CWND(Integer.BYTES),
        ADVMSS(Integer.BYTES),
        REORDERING(Integer.BYTES),
        RCV_RTT(Integer.BYTES),
        RCV_SPACE(Integer.BYTES),
        TOTAL_RETRANS(Integer.BYTES),
        PACING_RATE(Long.BYTES),
        MAX_PACING_RATE(Long.BYTES),
        BYTES_ACKED(Long.BYTES),
        BYTES_RECEIVED(Long.BYTES),
        SEGS_OUT(Integer.BYTES),
        SEGS_IN(Integer.BYTES),
        NOTSENT_BYTES(Integer.BYTES),
        MIN_RTT(Integer.BYTES),
        DATA_SEGS_IN(Integer.BYTES),
        DATA_SEGS_OUT(Integer.BYTES),
        DELIVERY_RATE(Long.BYTES),
        BUSY_TIME(Long.BYTES),
        RWND_LIMITED(Long.BYTES),
        SNDBUF_LIMITED(Long.BYTES);

        public final int size;

        Field(int s) {
            size = s;
        }
    }

    private static final String TAG = "TcpInfo";
    @VisibleForTesting
    static final int LOST_OFFSET = getFieldOffset(Field.LOST);
    @VisibleForTesting
    static final int RETRANSMITS_OFFSET = getFieldOffset(Field.RETRANSMITS);
    @VisibleForTesting
    static final int SEGS_IN_OFFSET = getFieldOffset(Field.SEGS_IN);
    @VisibleForTesting
    static final int SEGS_OUT_OFFSET = getFieldOffset(Field.SEGS_OUT);
    final int mSegsIn;
    final int mSegsOut;
    final int mLost;
    final int mRetransmits;

    private static int getFieldOffset(@NonNull final Field needle) {
        int offset = 0;
        for (final Field field : Field.values()) {
            if (field == needle) return offset;
            offset += field.size;
        }
        throw new IllegalArgumentException("Unknown field");
    }

    private TcpInfo(@NonNull ByteBuffer bytes, int infolen) {
        // SEGS_IN is the last required field in the buffer, so if the buffer is long enough for
        // SEGS_IN it's long enough for everything
        if (SEGS_IN_OFFSET + Field.SEGS_IN.size > infolen) {
            throw new IllegalArgumentException("Length " + infolen + " is less than required.");
        }
        final int start = bytes.position();
        mSegsIn = bytes.getInt(start + SEGS_IN_OFFSET);
        mSegsOut = bytes.getInt(start + SEGS_OUT_OFFSET);
        mLost = bytes.getInt(start + LOST_OFFSET);
        mRetransmits = bytes.get(start + RETRANSMITS_OFFSET);
        // tcp_info structure grows over time as new fields are added. Jump to the end of the
        // structure, as unknown fields might remain at the end of the structure if the tcp_info
        // struct was expanded.
        bytes.position(Math.min(infolen + start, bytes.limit()));
    }

    @VisibleForTesting
    TcpInfo(int retransmits, int lost, int segsOut, int segsIn) {
        mRetransmits = retransmits;
        mLost = lost;
        mSegsOut = segsOut;
        mSegsIn = segsIn;
    }

    /** Parse a TcpInfo from a giving ByteBuffer with a specific length. */
    @Nullable
    public static TcpInfo parse(@NonNull ByteBuffer bytes, int infolen) {
        try {
            return new TcpInfo(bytes, infolen);
        } catch (BufferUnderflowException | BufferOverflowException | IllegalArgumentException
                | IndexOutOfBoundsException e) {
            Log.e(TAG, "parsing error.", e);
            return null;
        }
    }

    private static String decodeWscale(byte num) {
        return String.valueOf((num >> 4) & 0x0f)  + ":" + String.valueOf(num & 0x0f);
    }

    /**
     *  Returns a string representing a given tcp state.
     *  Map to enum in bionic/libc/include/netinet/tcp.h
     */
    @VisibleForTesting
    static String getTcpStateName(int state) {
        switch (state) {
            case 1: return "TCP_ESTABLISHED";
            case 2: return "TCP_SYN_SENT";
            case 3: return "TCP_SYN_RECV";
            case 4: return "TCP_FIN_WAIT1";
            case 5: return "TCP_FIN_WAIT2";
            case 6: return "TCP_TIME_WAIT";
            case 7: return "TCP_CLOSE";
            case 8: return "TCP_CLOSE_WAIT";
            case 9: return "TCP_LAST_ACK";
            case 10: return "TCP_LISTEN";
            case 11: return "TCP_CLOSING";
            default: return "UNKNOWN:" + Integer.toString(state);
        }
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof TcpInfo)) return false;
        TcpInfo other = (TcpInfo) obj;

        return mSegsIn == other.mSegsIn && mSegsOut == other.mSegsOut
            && mRetransmits == other.mRetransmits && mLost == other.mLost;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mLost, mRetransmits, mSegsIn, mSegsOut);
    }

    @Override
    public String toString() {
        return "TcpInfo{lost=" + mLost + ", retransmit=" + mRetransmits + ", received=" + mSegsIn
                + ", sent=" + mSegsOut + "}";
    }
}
