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

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import libcore.util.HexEncoding;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.nio.ByteBuffer;
import java.util.LinkedHashMap;
import java.util.Map;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class TcpInfoTest {
    private static final int TCP_INFO_LENGTH_V1 = 192;
    private static final int SHORT_TEST_TCP_INFO = 8;
    private static final String TCP_ESTABLISHED = "TCP_ESTABLISHED";
    private static final String TCP_FIN_WAIT1 = "TCP_FIN_WAIT1";
    private static final String TCP_SYN_SENT = "TCP_SYN_SENT";
    private static final String UNKNOWN_20 = "UNKNOWN:20";
    // Refer to rfc793 for the value definition.
    private static final String TCP_INFO_HEX =
            "01" +                // state = TCP_ESTABLISHED
            "00" +                // ca_state = TCP_CA_OPEN
            "00" +                // retransmits = 0
            "00" +                // probes = 0
            "00" +                // backoff = 0
            "07" +                // option = TCPI_OPT_WSCALE|TCPI_OPT_SACK|TCPI_OPT_TIMESTAMPS
            "88" +                // wscale = 8
            "00" +                // delivery_rate_app_limited = 0
            "001B914A" +          // rto = 1806666
            "00000000" +          // ato = 0
            "0000052E" +          // sndMss = 1326
            "00000218" +          // rcvMss = 536
            "00000000" +          // unsacked = 0
            "00000000" +          // acked = 0
            "00000000" +          // lost = 0
            "00000000" +          // retrans = 0
            "00000000" +          // fackets = 0
            "000000BB" +          // lastDataSent = 187
            "00000000" +          // lastAckSent = 0
            "000000BB" +          // lastDataRecv = 187
            "000000BB" +          // lastDataAckRecv = 187
            "000005DC" +          // pmtu = 1500
            "00015630" +          // rcvSsthresh = 87600
            "00092C3E" +          // rttt = 601150
            "0004961F" +          // rttvar = 300575
            "00000578" +          // sndSsthresh = 1400
            "0000000A" +          // sndCwnd = 10
            "000005A8" +          // advmss = 1448
            "00000003" +          // reordering = 3
            "00000000" +          // rcvrtt = 0
            "00015630" +          // rcvspace = 87600
            "00000000" +          // totalRetrans = 0
            "000000000000AC53" +  // pacingRate = 44115
            "FFFFFFFFFFFFFFFF" +  // maxPacingRate = 18446744073709551615
            "0000000000000001" +  // bytesAcked = 1
            "0000000000000000" +  // bytesReceived = 0
            "00000002" +          // SegsOut = 2
            "00000001" +          // SegsIn = 1
            "00000000" +          // NotSentBytes = 0
            "00092C3E" +          // minRtt = 601150
            "00000000" +          // DataSegsIn = 0
            "00000000" +          // DataSegsOut = 0
            "0000000000000000" +  // deliverRate = 0
            "0000000000000000" +  // busyTime = 0
            "0000000000000000" +  // RwndLimited = 0
            "0000000000000000";   // sndBufLimited = 0
    private static final byte[] TCP_INFO_BYTES =
            HexEncoding.decode(TCP_INFO_HEX.toCharArray(), false);
    private static final TcpInfo TEST_TCPINFO =
            new TcpInfo(0 /* retransmits */, 0 /* lost */, 2 /* segsOut */, 1 /* segsIn */);

    private static final String EXPANDED_TCP_INFO_HEX = TCP_INFO_HEX
            + "00000000"         // tcpi_delivered
            + "00000000";        // tcpi_delivered_ce
    private static final byte[] EXPANDED_TCP_INFO_BYTES =
            HexEncoding.decode(EXPANDED_TCP_INFO_HEX.toCharArray(), false);
    private static final int EXPANDED_TCP_INFO_LENGTH =
            EXPANDED_TCP_INFO_BYTES.length - TCP_INFO_BYTES.length;
    @Test
    public void testParseTcpInfo() {
        final ByteBuffer buffer = ByteBuffer.wrap(TCP_INFO_BYTES);
        // Length is less than required
        final TcpInfo nullInfo = TcpInfo.parse(buffer, SHORT_TEST_TCP_INFO);
        assertEquals(nullInfo, null);

        final TcpInfo parsedInfo = TcpInfo.parse(buffer, TCP_INFO_LENGTH_V1);
        assertEquals(parsedInfo, TEST_TCPINFO);

        // Make a data that TcpInfo is not started from the begining of the buffer.
        final ByteBuffer bufferWithHeader =
                ByteBuffer.allocate(EXPANDED_TCP_INFO_BYTES.length + TCP_INFO_BYTES.length);
        bufferWithHeader.put(EXPANDED_TCP_INFO_BYTES);
        bufferWithHeader.put(TCP_INFO_BYTES);
        final TcpInfo infoWithHeader = TcpInfo.parse(buffer, TCP_INFO_LENGTH_V1);
        bufferWithHeader.position(EXPANDED_TCP_INFO_BYTES.length);
        assertEquals(parsedInfo, TEST_TCPINFO);
    }

    @Test
    public void testFieldOffset() {
        assertEquals(TcpInfo.RETRANSMITS_OFFSET, 2);
        assertEquals(TcpInfo.LOST_OFFSET, 32);
        assertEquals(TcpInfo.SEGS_OUT_OFFSET, 136);
        assertEquals(TcpInfo.SEGS_IN_OFFSET, 140);
    }

    @Test
    public void testParseTcpInfoExpanded() {
        final ByteBuffer buffer = ByteBuffer.wrap(EXPANDED_TCP_INFO_BYTES);
        final TcpInfo parsedInfo =
                TcpInfo.parse(buffer, TCP_INFO_LENGTH_V1 + EXPANDED_TCP_INFO_LENGTH);

        assertEquals(parsedInfo, TEST_TCPINFO);
        assertEquals(buffer.limit(), buffer.position());

        // reset the index.
        buffer.position(0);
        final TcpInfo parsedInfoShorterLen = TcpInfo.parse(buffer, TCP_INFO_LENGTH_V1);
        assertEquals(parsedInfoShorterLen, TEST_TCPINFO);
        assertEquals(TCP_INFO_LENGTH_V1, buffer.position());
    }

    @Test
    public void testTcpStateName() {
        assertEquals(TcpInfo.getTcpStateName(4), TCP_FIN_WAIT1);
        assertEquals(TcpInfo.getTcpStateName(1), TCP_ESTABLISHED);
        assertEquals(TcpInfo.getTcpStateName(2), TCP_SYN_SENT);
        assertEquals(TcpInfo.getTcpStateName(20), UNKNOWN_20);
    }

    private static final String MALFORMED_TCP_INFO_HEX =
            "01" +                // state = TCP_ESTABLISHED
            "00" +                // ca_state = TCP_CA_OPEN
            "00" +                // retransmits = 0
            "00" +                // probes = 0
            "00" +                // backoff = 0
            "07" +                // option = TCPI_OPT_WSCALE|TCPI_OPT_SACK|TCPI_OPT_TIMESTAMPS
            "88" +                // wscale = 8
            "00" +                // delivery_rate_app_limited = 0
            "001B";               // Incomplete bytes, expect to be an int.
    private static final byte[] MALFORMED_TCP_INFO_BYTES =
            HexEncoding.decode(MALFORMED_TCP_INFO_HEX.toCharArray(), false);
    @Test
    public void testMalformedTcpInfo() {
        final ByteBuffer buffer = ByteBuffer.wrap(MALFORMED_TCP_INFO_BYTES);

        TcpInfo parsedInfo = TcpInfo.parse(buffer, SHORT_TEST_TCP_INFO);
        assertEquals(parsedInfo, null);

        parsedInfo = TcpInfo.parse(buffer, TCP_INFO_LENGTH_V1);
        assertEquals(parsedInfo, null);
    }

    // Make a TcpInfo contains only first 8 bytes.
    private Map<TcpInfo.Field, Number> makeShortTestTcpInfoHash() {
        final Map<TcpInfo.Field, Number> info = new LinkedHashMap<>();
        info.put(TcpInfo.Field.STATE, (byte) 0x01);
        info.put(TcpInfo.Field.CASTATE, (byte) 0x00);
        info.put(TcpInfo.Field.RETRANSMITS, (byte) 0x00);
        info.put(TcpInfo.Field.PROBES, (byte) 0x00);
        info.put(TcpInfo.Field.BACKOFF, (byte) 0x00);
        info.put(TcpInfo.Field.OPTIONS, (byte) 0x07);
        info.put(TcpInfo.Field.WSCALE, (byte) 0x88);
        info.put(TcpInfo.Field.DELIVERY_RATE_APP_LIMITED, (byte) 0x00);

        return info;
    }

    private Map<TcpInfo.Field, Number> makeTestTcpInfoHash() {
        final Map<TcpInfo.Field, Number> info = makeShortTestTcpInfoHash();
        info.put(TcpInfo.Field.RTO, 1806666);
        info.put(TcpInfo.Field.ATO, 0);
        info.put(TcpInfo.Field.SND_MSS, 1326);
        info.put(TcpInfo.Field.RCV_MSS, 536);
        info.put(TcpInfo.Field.UNACKED, 0);
        info.put(TcpInfo.Field.SACKED, 0);
        info.put(TcpInfo.Field.LOST, 0);
        info.put(TcpInfo.Field.RETRANS, 0);
        info.put(TcpInfo.Field.FACKETS, 0);
        info.put(TcpInfo.Field.LAST_DATA_SENT, 187);
        info.put(TcpInfo.Field.LAST_ACK_SENT, 0);
        info.put(TcpInfo.Field.LAST_DATA_RECV, 187);
        info.put(TcpInfo.Field.LAST_ACK_RECV, 187);
        info.put(TcpInfo.Field.PMTU, 1500);
        info.put(TcpInfo.Field.RCV_SSTHRESH, 87600);
        info.put(TcpInfo.Field.RTT, 601150);
        info.put(TcpInfo.Field.RTTVAR, 300575);
        info.put(TcpInfo.Field.SND_SSTHRESH, 1400);
        info.put(TcpInfo.Field.SND_CWND, 10);
        info.put(TcpInfo.Field.ADVMSS, 1448);
        info.put(TcpInfo.Field.REORDERING, 3);
        info.put(TcpInfo.Field.RCV_RTT, 0);
        info.put(TcpInfo.Field.RCV_SPACE, 87600);
        info.put(TcpInfo.Field.TOTAL_RETRANS, 0);
        info.put(TcpInfo.Field.PACING_RATE, 44115L);
        info.put(TcpInfo.Field.MAX_PACING_RATE, -1L);
        info.put(TcpInfo.Field.BYTES_ACKED, 1L);
        info.put(TcpInfo.Field.BYTES_RECEIVED, 0L);
        info.put(TcpInfo.Field.SEGS_OUT, 2);
        info.put(TcpInfo.Field.SEGS_IN, 1);
        info.put(TcpInfo.Field.NOTSENT_BYTES, 0);
        info.put(TcpInfo.Field.MIN_RTT, 601150);
        info.put(TcpInfo.Field.DATA_SEGS_IN, 0);
        info.put(TcpInfo.Field.DATA_SEGS_OUT, 0);
        info.put(TcpInfo.Field.DELIVERY_RATE, 0L);
        info.put(TcpInfo.Field.BUSY_TIME, 0L);
        info.put(TcpInfo.Field.RWND_LIMITED, 0L);
        info.put(TcpInfo.Field.SNDBUF_LIMITED, 0L);

        return info;
    }
}
