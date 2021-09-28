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

package android.net.ipsec.ike;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.net.InetAddresses;

import com.android.internal.net.TestUtils;
import com.android.internal.net.ipsec.ike.exceptions.InvalidSyntaxException;

import org.junit.Test;

import java.net.InetAddress;
import java.nio.ByteBuffer;

public final class IkeTrafficSelectorTest {
    private static final String TS_IPV4_ONE_HEX_STRING = "070000100010fff0c0000264c0000365";
    private static final int TS_ONE_START_PORT = 16;
    private static final int TS_ONE_END_PORT = 65520;
    private static final InetAddress TS_ONE_START_ADDRESS =
            InetAddresses.parseNumericAddress("192.0.2.100");
    private static final InetAddress TS_ONE_END_ADDRESS =
            InetAddresses.parseNumericAddress("192.0.3.101");

    private static final String TS_IPV4_TWO_HEX_STRING = "070000100000ffffc0000464c0000466";
    private static final int TS_TWO_START_PORT = 0;
    private static final int TS_TWO_END_PORT = 65535;
    private static final InetAddress TS_TWO_START_ADDRESS =
            InetAddresses.parseNumericAddress("192.0.4.100");
    private static final InetAddress TS_TWO_END_ADDRESS =
            InetAddresses.parseNumericAddress("192.0.4.102");

    private static final InetAddress TS_THREE_START_ADDRESS =
            InetAddresses.parseNumericAddress("::");
    private static final InetAddress TS_THREE_END_ADDRESS =
            InetAddresses.parseNumericAddress("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
    String TS_IPV6_THREE_HEX_ADDRESS =
            "080000280000ffff00000000000000000000000000000000ffffffffffffffffffffffffffffffff";

    private static final String TX_IPV4_INVALID_PORT_RANGE_HEX_STRING =
            "0700001022221111c0000464c0000466";
    private static final String TX_IPV4_INVALID_ADDRESS_RANGE_HEX_STRING =
            "070000100000ffffc0000466c0000366";

    private static final int TS_TYPE_OFFSET = 0;
    private static final int PROTOCOL_ID_OFFSET = 1;
    private static final int TS_LENGTH_OFFSET = 2;

    private IkeTrafficSelector mTsOne;
    private IkeTrafficSelector mTsTwo;
    private IkeTrafficSelector mTsIpv6Three;

    public IkeTrafficSelectorTest() {
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
        mTsIpv6Three =
                new IkeTrafficSelector(
                        IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV6_ADDR_RANGE,
                        TS_TWO_START_PORT,
                        TS_TWO_END_PORT,
                        TS_THREE_START_ADDRESS,
                        TS_THREE_END_ADDRESS);
    }

    @Test
    public void testDecodeIkeTrafficSelectors() throws Exception {
        int numTs = 2;

        byte[] tsBytes =
                TestUtils.hexStringToByteArray(TS_IPV4_ONE_HEX_STRING + TS_IPV4_TWO_HEX_STRING);
        IkeTrafficSelector[] selectors =
                IkeTrafficSelector.decodeIkeTrafficSelectors(numTs, tsBytes);

        assertEquals(numTs, selectors.length);

        // Verify first traffic selector
        IkeTrafficSelector tsOne = selectors[0];

        assertEquals(IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV4_ADDR_RANGE, tsOne.tsType);
        assertEquals(IkeTrafficSelector.IP_PROTOCOL_ID_UNSPEC, tsOne.ipProtocolId);
        assertEquals(IkeTrafficSelector.TRAFFIC_SELECTOR_IPV4_LEN, tsOne.selectorLength);
        assertEquals(TS_ONE_START_PORT, tsOne.startPort);
        assertEquals(TS_ONE_END_PORT, tsOne.endPort);
        assertEquals(TS_ONE_START_ADDRESS, tsOne.startingAddress);
        assertEquals(TS_ONE_END_ADDRESS, tsOne.endingAddress);

        // Verify second traffic selector
        IkeTrafficSelector tsTwo = selectors[1];

        assertEquals(IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV4_ADDR_RANGE, tsTwo.tsType);
        assertEquals(IkeTrafficSelector.IP_PROTOCOL_ID_UNSPEC, tsTwo.ipProtocolId);
        assertEquals(IkeTrafficSelector.TRAFFIC_SELECTOR_IPV4_LEN, tsTwo.selectorLength);
        assertEquals(TS_TWO_START_PORT, tsTwo.startPort);
        assertEquals(TS_TWO_END_PORT, tsTwo.endPort);
        assertEquals(TS_TWO_START_ADDRESS, tsTwo.startingAddress);
        assertEquals(TS_TWO_END_ADDRESS, tsTwo.endingAddress);
    }

    @Test
    public void testBuildAndEncodeIkeTrafficSelector() throws Exception {
        IkeTrafficSelector ts =
                new IkeTrafficSelector(
                        IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV4_ADDR_RANGE,
                        TS_ONE_START_PORT,
                        TS_ONE_END_PORT,
                        TS_ONE_START_ADDRESS,
                        TS_ONE_END_ADDRESS);

        ByteBuffer byteBuffer = ByteBuffer.allocate(ts.selectorLength);
        ts.encodeToByteBuffer(byteBuffer);

        byte[] expectedBytes = TestUtils.hexStringToByteArray(TS_IPV4_ONE_HEX_STRING);
        assertArrayEquals(expectedBytes, byteBuffer.array());
    }

    @Test
    public void testEquals() throws Exception {
        IkeTrafficSelector tsOneOther =
                new IkeTrafficSelector(
                        IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV4_ADDR_RANGE,
                        TS_ONE_START_PORT,
                        TS_ONE_END_PORT,
                        TS_ONE_START_ADDRESS,
                        TS_ONE_END_ADDRESS);

        assertEquals(mTsOne, tsOneOther);
        assertNotEquals(mTsOne, mTsTwo);
    }

    @Test
    public void testContains() throws Exception {
        IkeTrafficSelector tsOneSubset =
                new IkeTrafficSelector(
                        IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV4_ADDR_RANGE,
                        TS_ONE_START_PORT + 1,
                        TS_ONE_END_PORT,
                        TS_ONE_START_ADDRESS,
                        TS_ONE_END_ADDRESS);
        assertTrue(mTsOne.contains(tsOneSubset));
        assertFalse(tsOneSubset.contains(mTsOne));

        assertTrue(mTsOne.contains(mTsOne));
        assertFalse(mTsOne.contains(mTsTwo));
    }

    @Test
    public void testDecodeIkeTrafficSelectorWithInvalidTsType() throws Exception {
        int numTs = 1;
        byte[] tsBytes = TestUtils.hexStringToByteArray(TS_IPV4_ONE_HEX_STRING);
        tsBytes[TS_TYPE_OFFSET] = -1;

        try {
            IkeTrafficSelector.decodeIkeTrafficSelectors(numTs, tsBytes);
            fail("Expected to fail due to invalid Traffic Selector Type.");
        } catch (InvalidSyntaxException expected) {

        }
    }

    @Test
    public void testDecodeIkeTrafficSelectorWithInvalidIpProtocol() throws Exception {
        int numTs = 1;
        byte[] tsBytes = TestUtils.hexStringToByteArray(TS_IPV4_ONE_HEX_STRING);
        tsBytes[PROTOCOL_ID_OFFSET] = -1;

        try {
            IkeTrafficSelector.decodeIkeTrafficSelectors(numTs, tsBytes);
            fail("Expected to fail due to invalid IP Protocol ID.");
        } catch (InvalidSyntaxException expected) {

        }
    }

    @Test
    public void testDecodeIkeTrafficSelectorWithExpectedTrailing() throws Exception {
        int numTs = 1;
        byte[] tsBytes = TestUtils.hexStringToByteArray(TS_IPV4_ONE_HEX_STRING + "FFFF");

        try {
            IkeTrafficSelector.decodeIkeTrafficSelectors(numTs, tsBytes);
            fail("Expected to fail due to unexpected trailing characters.");
        } catch (InvalidSyntaxException expected) {

        }
    }

    @Test
    public void testDecodeIkeTrafficSelectorWithInvalidTsLength() throws Exception {
        int numTs = 1;
        byte[] tsBytes = TestUtils.hexStringToByteArray(TS_IPV4_ONE_HEX_STRING);

        // Traffic Selector field is two octets
        tsBytes[TS_LENGTH_OFFSET] = 0;
        tsBytes[TS_LENGTH_OFFSET + 1] = 0;

        try {
            IkeTrafficSelector.decodeIkeTrafficSelectors(numTs, tsBytes);
            fail("Expected to fail due to invalid Traffic Selector length.");
        } catch (InvalidSyntaxException expected) {

        }
    }

    @Test
    public void testDecodeIkeTrafficSelectorWithInvalidPortRange() throws Exception {
        int numTs = 1;
        byte[] tsBytes = TestUtils.hexStringToByteArray(TX_IPV4_INVALID_PORT_RANGE_HEX_STRING);

        try {
            IkeTrafficSelector.decodeIkeTrafficSelectors(numTs, tsBytes);
            fail("Expected to fail when start port is larger than end port.");
        } catch (InvalidSyntaxException expected) {

        }
    }

    @Test
    public void testDecodeIkeTrafficSelectorWithInvalidAddressRange() throws Exception {
        int numTs = 1;
        byte[] tsBytes = TestUtils.hexStringToByteArray(TX_IPV4_INVALID_ADDRESS_RANGE_HEX_STRING);

        try {
            IkeTrafficSelector.decodeIkeTrafficSelectors(numTs, tsBytes);
            fail("Expected to fail when starting address is larger than ending address.");
        } catch (InvalidSyntaxException expected) {

        }
    }

    @Test
    public void testBuildIkeTrafficSelectorWithInvalidTsType() throws Exception {
        try {
            IkeTrafficSelector ts =
                    new IkeTrafficSelector(
                            0,
                            TS_ONE_START_PORT,
                            TS_ONE_END_PORT,
                            TS_ONE_START_ADDRESS,
                            TS_ONE_END_ADDRESS);
            fail("Expected to fail due to unrecognized Traffic Selector type.");
        } catch (IllegalArgumentException expected) {

        }
    }

    @Test
    public void testBuildIkeTrafficSelectorWithInvalidPortRange() throws Exception {
        try {
            IkeTrafficSelector ts =
                    new IkeTrafficSelector(
                            IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV4_ADDR_RANGE,
                            TS_ONE_END_PORT,
                            TS_ONE_START_PORT,
                            TS_ONE_START_ADDRESS,
                            TS_ONE_END_ADDRESS);
            fail("Expected to fail due to invalid port range.");
        } catch (IllegalArgumentException expected) {

        }
    }

    @Test
    public void testBuildIkeTrafficSelectorWithMismatchedAddressType() throws Exception {
        InetAddress inet6Address = InetAddresses.parseNumericAddress("0:2001:0:db8::1");
        try {
            IkeTrafficSelector ts =
                    new IkeTrafficSelector(
                            IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV4_ADDR_RANGE,
                            TS_ONE_START_PORT,
                            TS_ONE_END_PORT,
                            inet6Address,
                            TS_ONE_END_ADDRESS);
            fail("Expected to fail due to mismatched address format.");
        } catch (IllegalArgumentException expected) {

        }
    }

    @Test
    public void testBuildIkeTrafficSelectorWithInvalidAddressRange() throws Exception {
        try {
            IkeTrafficSelector ts =
                    new IkeTrafficSelector(
                            IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV4_ADDR_RANGE,
                            TS_ONE_START_PORT,
                            TS_ONE_END_PORT,
                            TS_ONE_END_ADDRESS,
                            TS_ONE_START_ADDRESS);
            fail("Expected to fail due to invalid address range.");
        } catch (IllegalArgumentException e) {
        }
    }

    @Test
    public void testBuildAndEncodeIpv6TrafficSelector() throws Exception {
        ByteBuffer byteBuffer = ByteBuffer.allocate(mTsIpv6Three.selectorLength);
        mTsIpv6Three.encodeToByteBuffer(byteBuffer);

        byte[] expectedBytes = TestUtils.hexStringToByteArray(TS_IPV6_THREE_HEX_ADDRESS);
        assertArrayEquals(expectedBytes, byteBuffer.array());
    }

    @Test
    public void testIpv6TsEquals() throws Exception {
        IkeTrafficSelector tsThreeOther =
                new IkeTrafficSelector(
                        IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV6_ADDR_RANGE,
                        TS_TWO_START_PORT,
                        TS_TWO_END_PORT,
                        TS_THREE_START_ADDRESS,
                        TS_THREE_END_ADDRESS);

        IkeTrafficSelector tsThreeOtherDifferentPort =
                new IkeTrafficSelector(
                        IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV6_ADDR_RANGE,
                        TS_ONE_START_PORT,
                        TS_ONE_END_PORT,
                        TS_THREE_START_ADDRESS,
                        TS_THREE_END_ADDRESS);

        assertEquals(mTsIpv6Three, tsThreeOther);
        assertNotEquals(mTsIpv6Three, tsThreeOtherDifferentPort);
    }

    @Test
    public void testIpv6TsContains() throws Exception {
        IkeTrafficSelector tsIpv6ThreeSubsetPortRange =
                new IkeTrafficSelector(
                        IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV6_ADDR_RANGE,
                        TS_ONE_START_PORT,
                        TS_ONE_END_PORT,
                        TS_THREE_START_ADDRESS,
                        TS_THREE_END_ADDRESS);
        IkeTrafficSelector tsIpv6ThreeSubsetAddrRange =
                new IkeTrafficSelector(
                        IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV6_ADDR_RANGE,
                        TS_TWO_START_PORT,
                        TS_TWO_END_PORT,
                        TS_THREE_START_ADDRESS,
                        InetAddresses.parseNumericAddress(
                                "2001:0db8:85a3:0000:0000:8a2e:0370:7334"));

        assertTrue(mTsIpv6Three.contains(tsIpv6ThreeSubsetPortRange));
        assertTrue(mTsIpv6Three.contains(tsIpv6ThreeSubsetAddrRange));
        assertFalse(tsIpv6ThreeSubsetPortRange.contains(mTsIpv6Three));
        assertFalse(tsIpv6ThreeSubsetAddrRange.contains(mTsIpv6Three));
    }

    @Test
    public void testDecodeIpv6TS() throws Exception {
        int numTs = 1;

        byte[] tsBytes = TestUtils.hexStringToByteArray(TS_IPV6_THREE_HEX_ADDRESS);
        IkeTrafficSelector[] selectors =
                IkeTrafficSelector.decodeIkeTrafficSelectors(numTs, tsBytes);

        assertEquals(numTs, selectors.length);
        assertTrue(selectors[0].equals(mTsIpv6Three));
    }

    @Test
    public void testDecodeWithBothIpv6AndIpv4TS() throws Exception {
        int numTs = 2;
        byte[] tsBytes =
                TestUtils.hexStringToByteArray(TS_IPV6_THREE_HEX_ADDRESS + TS_IPV4_ONE_HEX_STRING);
        IkeTrafficSelector[] selectors =
                IkeTrafficSelector.decodeIkeTrafficSelectors(numTs, tsBytes);

        assertEquals(numTs, selectors.length);
        assertTrue(selectors[0].equals(mTsIpv6Three));
        assertTrue(selectors[1].equals(mTsOne));
    }

    @Test
    public void testDecodeIpv6TSWithInvalidLength() throws Exception {
        int numTs = 2;
        byte[] tsBytes =
                TestUtils.hexStringToByteArray(TS_IPV6_THREE_HEX_ADDRESS + "FFFF");

        try {
            IkeTrafficSelector[] selectors =
                    IkeTrafficSelector.decodeIkeTrafficSelectors(numTs, tsBytes);
            fail("Expected to fail with invalid syntax exception");
        } catch(InvalidSyntaxException e) {
        }

    }
}
