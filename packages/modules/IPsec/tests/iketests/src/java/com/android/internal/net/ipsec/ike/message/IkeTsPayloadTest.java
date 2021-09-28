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

package com.android.internal.net.ipsec.ike.message;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.net.InetAddresses;
import android.net.ipsec.ike.IkeTrafficSelector;

import com.android.internal.net.TestUtils;

import org.junit.Test;

import java.net.Inet4Address;
import java.nio.ByteBuffer;

public final class IkeTsPayloadTest {
    private static final String TS_INITIATOR_PAYLOAD_HEX_STRING =
            "2d00002802000000070000100010fff0c0000264c0000365070000100000ffffc0000464c0000466";

    private static final int NUMBER_OF_TS = 2;

    private static final int TS_ONE_START_PORT = 16;
    private static final int TS_ONE_END_PORT = 65520;
    private static final Inet4Address TS_ONE_START_ADDRESS =
            (Inet4Address) (InetAddresses.parseNumericAddress("192.0.2.100"));
    private static final Inet4Address TS_ONE_END_ADDRESS =
            (Inet4Address) (InetAddresses.parseNumericAddress("192.0.3.101"));

    private static final int TS_TWO_START_PORT = 0;
    private static final int TS_TWO_END_PORT = 65535;
    private static final Inet4Address TS_TWO_START_ADDRESS =
            (Inet4Address) (InetAddresses.parseNumericAddress("192.0.4.100"));
    private static final Inet4Address TS_TWO_END_ADDRESS =
            (Inet4Address) (InetAddresses.parseNumericAddress("192.0.4.102"));

    private IkeTrafficSelector mTsOne;
    private IkeTrafficSelector mTsTwo;

    public IkeTsPayloadTest() {
        mTsOne =
                new IkeTrafficSelector(
                        IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV4_ADDR_RANGE,
                        TS_ONE_START_PORT,
                        TS_ONE_END_PORT,
                        TS_ONE_START_ADDRESS,
                        TS_ONE_END_ADDRESS);
        mTsTwo =
                new IkeTrafficSelector(
                        IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV4_ADDR_RANGE,
                        TS_TWO_START_PORT,
                        TS_TWO_END_PORT,
                        TS_TWO_START_ADDRESS,
                        TS_TWO_END_ADDRESS);
    }

    @Test
    public void testDecodeTsInitiatorPayload() throws Exception {
        ByteBuffer inputBuffer =
                ByteBuffer.wrap(TestUtils.hexStringToByteArray(TS_INITIATOR_PAYLOAD_HEX_STRING));

        IkePayload payload =
                IkePayloadFactory.getIkePayload(
                                IkePayload.PAYLOAD_TYPE_TS_INITIATOR, false, inputBuffer)
                        .first;
        assertTrue(payload instanceof IkeTsPayload);

        IkeTsPayload tsPayload = (IkeTsPayload) payload;
        assertEquals(IkePayload.PAYLOAD_TYPE_TS_INITIATOR, tsPayload.payloadType);
        assertEquals(NUMBER_OF_TS, tsPayload.numTs);
    }

    @Test
    public void testBuildAndEncodeTsPayload() throws Exception {
        IkeTsPayload tsPayload =
                new IkeTsPayload(true /*isInitiator*/, new IkeTrafficSelector[] {mTsOne, mTsTwo});

        ByteBuffer byteBuffer = ByteBuffer.allocate(tsPayload.getPayloadLength());
        tsPayload.encodeToByteBuffer(IkePayload.PAYLOAD_TYPE_TS_RESPONDER, byteBuffer);

        byte[] expectedBytes = TestUtils.hexStringToByteArray(TS_INITIATOR_PAYLOAD_HEX_STRING);
        assertArrayEquals(expectedBytes, byteBuffer.array());
    }

    @Test
    public void testContains() throws Exception {
        IkeTrafficSelector tsOneNarrowPortRange =
                new IkeTrafficSelector(
                        IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV4_ADDR_RANGE,
                        TS_ONE_START_PORT + 1,
                        TS_ONE_END_PORT,
                        TS_ONE_START_ADDRESS,
                        TS_ONE_END_ADDRESS);

        IkeTrafficSelector tsOneNarrowAddressRange =
                new IkeTrafficSelector(
                        IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV4_ADDR_RANGE,
                        TS_ONE_START_PORT,
                        TS_ONE_END_PORT,
                        (Inet4Address) (InetAddresses.parseNumericAddress("192.0.2.200")),
                        TS_ONE_END_ADDRESS);

        IkeTsPayload tsPayload =
                new IkeTsPayload(true /*isInitiator*/, new IkeTrafficSelector[] {mTsOne, mTsTwo});

        IkeTsPayload tsNarrowPortRangePayload =
                new IkeTsPayload(
                        true /*isInitiator*/,
                        new IkeTrafficSelector[] {tsOneNarrowPortRange, mTsTwo});

        IkeTsPayload tsNarrowAddressRangePayload =
                new IkeTsPayload(
                        true /*isInitiator*/,
                        new IkeTrafficSelector[] {tsOneNarrowAddressRange, mTsTwo});

        assertTrue(tsPayload.contains(tsPayload));
        assertTrue(tsPayload.contains(tsNarrowPortRangePayload));
        assertTrue(tsPayload.contains(tsNarrowAddressRangePayload));

        assertFalse(tsNarrowPortRangePayload.contains(tsPayload));
        assertFalse(tsNarrowAddressRangePayload.contains(tsPayload));
        assertFalse(tsNarrowAddressRangePayload.contains(tsNarrowPortRangePayload));
        assertFalse(tsNarrowPortRangePayload.contains(tsNarrowAddressRangePayload));
    }
}
