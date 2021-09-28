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

package android.net.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.net.InetAddresses;
import android.net.IpPrefix;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.util.ArrayList;
import java.util.List;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class IpRangeTest {

    private static InetAddress address(String addr) {
        return InetAddresses.parseNumericAddress(addr);
    }

    private static final Inet4Address IPV4_ADDR = (Inet4Address) address("192.0.2.4");
    private static final Inet4Address IPV4_RANGE_END = (Inet4Address) address("192.0.3.1");
    private static final Inet6Address IPV6_ADDR = (Inet6Address) address("2001:db8::");
    private static final Inet6Address IPV6_RANGE_END = (Inet6Address) address("2001:db9:010f::");

    private static final byte[] IPV4_BYTES = IPV4_ADDR.getAddress();
    private static final byte[] IPV6_BYTES = IPV6_ADDR.getAddress();

    @Test
    public void testConstructorBadArguments() {
        try {
            new IpRange(null, IPV6_ADDR);
            fail("Expected NullPointerException: null start address");
        } catch (NullPointerException expected) {
        }

        try {
            new IpRange(IPV6_ADDR, null);
            fail("Expected NullPointerException: null end address");
        } catch (NullPointerException expected) {
        }

        try {
            new IpRange(null, null);
            fail("Expected NullPointerException: null addresses");
        } catch (NullPointerException expected) {
        }

        try {
            new IpRange(null);
            fail("Expected NullPointerException: null start address");
        } catch (NullPointerException expected) {
        }

        try {
            new IpRange(address("10.10.10.10"), address("1.2.3.4"));
            fail("Expected IllegalArgumentException: start address after end address");
        } catch (IllegalArgumentException expected) {
        }

        try {
            new IpRange(address("ffff::"), address("abcd::"));
            fail("Expected IllegalArgumentException: start address after end address");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testConstructor() {
        IpRange r = new IpRange(new IpPrefix(IPV4_ADDR, 32));
        assertEquals(IPV4_ADDR, r.getStartAddr());
        assertEquals(IPV4_ADDR, r.getEndAddr());

        r = new IpRange(new IpPrefix(IPV4_ADDR, 16));
        assertEquals(address("192.0.0.0"), r.getStartAddr());
        assertEquals(address("192.0.255.255"), r.getEndAddr());

        r = new IpRange(IPV4_ADDR, IPV4_RANGE_END);
        assertEquals(IPV4_ADDR, r.getStartAddr());
        assertEquals(IPV4_RANGE_END, r.getEndAddr());

        r = new IpRange(new IpPrefix(IPV6_ADDR, 128));
        assertEquals(IPV6_ADDR, r.getStartAddr());
        assertEquals(IPV6_ADDR, r.getEndAddr());

        r = new IpRange(new IpPrefix(IPV6_ADDR, 16));
        assertEquals(address("2001::"), r.getStartAddr());
        assertEquals(address("2001:ffff:ffff:ffff:ffff:ffff:ffff:ffff"), r.getEndAddr());

        r = new IpRange(IPV6_ADDR, IPV6_RANGE_END);
        assertEquals(IPV6_ADDR, r.getStartAddr());
        assertEquals(IPV6_RANGE_END, r.getEndAddr());
    }

    @Test
    public void testContainsRangeEqualRanges() {
        final IpRange r1 = new IpRange(new IpPrefix(IPV6_ADDR, 35));
        final IpRange r2 = new IpRange(new IpPrefix(IPV6_ADDR, 35));

        assertTrue(r1.containsRange(r2));
        assertTrue(r2.containsRange(r1));
        assertEquals(r1, r2);
    }

    @Test
    public void testContainsRangeSubset() {
        final IpRange r1 = new IpRange(new IpPrefix(IPV6_ADDR, 64));
        final IpRange r2 = new IpRange(new IpPrefix(address("2001:db8::0101"), 128));

        assertTrue(r1.containsRange(r2));
        assertFalse(r2.containsRange(r1));
        assertNotEquals(r1, r2);
    }

    @Test
    public void testContainsRangeTruncatesLowerOrderBits() {
        final IpRange r1 = new IpRange(new IpPrefix(IPV6_ADDR, 100));
        final IpRange r2 = new IpRange(new IpPrefix(address("2001:db8::0101"), 100));

        assertTrue(r1.containsRange(r2));
        assertTrue(r2.containsRange(r1));
        assertEquals(r1, r2);
    }

    @Test
    public void testContainsRangeSubsetSameStartAddr() {
        final IpRange r1 = new IpRange(new IpPrefix(IPV6_ADDR, 35));
        final IpRange r2 = new IpRange(new IpPrefix(IPV6_ADDR, 39));

        assertTrue(r1.containsRange(r2));
        assertFalse(r2.containsRange(r1));
        assertNotEquals(r1, r2);
    }

    @Test
    public void testContainsRangeOverlapping() {
        final IpRange r1 = new IpRange(new IpPrefix(address("2001:db9::"), 32));
        final IpRange r2 = new IpRange(address("2001:db8::"), address("2001:db9::1"));

        assertFalse(r1.containsRange(r2));
        assertFalse(r2.containsRange(r1));
        assertNotEquals(r1, r2);
    }

    @Test
    public void testOverlapsRangeEqualRanges() {
        final IpRange r1 = new IpRange(new IpPrefix(IPV6_ADDR, 35));
        final IpRange r2 = new IpRange(new IpPrefix(IPV6_ADDR, 35));

        assertTrue(r1.overlapsRange(r2));
        assertTrue(r2.overlapsRange(r1));
        assertEquals(r1, r2);
    }

    @Test
    public void testOverlapsRangeSubset() {
        final IpRange r1 = new IpRange(new IpPrefix(IPV6_ADDR, 35));
        final IpRange r2 = new IpRange(new IpPrefix(IPV6_ADDR, 39));

        assertTrue(r1.overlapsRange(r2));
        assertTrue(r2.overlapsRange(r1));
        assertNotEquals(r1, r2);
    }

    @Test
    public void testOverlapsRangeDisjoint() {
        final IpRange r1 = new IpRange(new IpPrefix(IPV6_ADDR, 32));
        final IpRange r2 = new IpRange(new IpPrefix(address("2001:db9::"), 32));

        assertFalse(r1.overlapsRange(r2));
        assertFalse(r2.overlapsRange(r1));
        assertNotEquals(r1, r2);
    }

    @Test
    public void testOverlapsRangePartialOverlapLow() {
        final IpRange r1 = new IpRange(new IpPrefix(address("2001:db9::"), 32));
        final IpRange r2 = new IpRange(address("2001:db8::"), address("2001:db9::1"));

        assertTrue(r1.overlapsRange(r2));
        assertTrue(r2.overlapsRange(r1));
        assertNotEquals(r1, r2);
    }

    @Test
    public void testOverlapsRangePartialOverlapHigh() {
        final IpRange r1 = new IpRange(new IpPrefix(address("2001:db7::"), 32));
        final IpRange r2 = new IpRange(address("2001:db7::ffff"), address("2001:db8::"));

        assertTrue(r1.overlapsRange(r2));
        assertTrue(r2.overlapsRange(r1));
        assertNotEquals(r1, r2);
    }

    @Test
    public void testIpRangeToPrefixesIpv4FullRange() throws Exception {
        final IpRange range = new IpRange(address("0.0.0.0"), address("255.255.255.255"));
        final List<IpPrefix> prefixes = new ArrayList<>();
        prefixes.add(new IpPrefix("0.0.0.0/0"));

        assertEquals(prefixes, range.asIpPrefixes());
    }

    @Test
    public void testIpRangeToPrefixesIpv4() throws Exception {
        final IpRange range = new IpRange(IPV4_ADDR, IPV4_RANGE_END);
        final List<IpPrefix> prefixes = new ArrayList<>();
        prefixes.add(new IpPrefix("192.0.2.128/25"));
        prefixes.add(new IpPrefix("192.0.2.64/26"));
        prefixes.add(new IpPrefix("192.0.2.32/27"));
        prefixes.add(new IpPrefix("192.0.2.16/28"));
        prefixes.add(new IpPrefix("192.0.2.8/29"));
        prefixes.add(new IpPrefix("192.0.2.4/30"));
        prefixes.add(new IpPrefix("192.0.3.0/31"));

        assertEquals(prefixes, range.asIpPrefixes());
    }

    @Test
    public void testIpRangeToPrefixesIpv6FullRange() throws Exception {
        final IpRange range =
                new IpRange(address("::"), address("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
        final List<IpPrefix> prefixes = new ArrayList<>();
        prefixes.add(new IpPrefix("::/0"));

        assertEquals(prefixes, range.asIpPrefixes());
    }

    @Test
    public void testIpRangeToPrefixesIpv6() throws Exception {
        final IpRange range = new IpRange(IPV6_ADDR, IPV6_RANGE_END);
        final List<IpPrefix> prefixes = new ArrayList<>();
        prefixes.add(new IpPrefix("2001:db8::/32"));
        prefixes.add(new IpPrefix("2001:db9::/40"));
        prefixes.add(new IpPrefix("2001:db9:100::/45"));
        prefixes.add(new IpPrefix("2001:db9:108::/46"));
        prefixes.add(new IpPrefix("2001:db9:10c::/47"));
        prefixes.add(new IpPrefix("2001:db9:10e::/48"));
        prefixes.add(new IpPrefix("2001:db9:10f::/128"));

        assertEquals(prefixes, range.asIpPrefixes());
    }
}
