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

package android.net.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.net.InetAddresses;
import android.net.IpPrefix;
import android.net.RouteInfo;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.net.InetAddress;
import java.util.ArrayList;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public final class NetUtilsTest {
    private static final InetAddress V4_ADDR1 = toInetAddress("75.208.7.1");
    private static final InetAddress V4_ADDR2 = toInetAddress("75.208.7.2");
    private static final InetAddress V6_ADDR1 = toInetAddress("2001:0db8:85a3::8a2e:0370:7334");
    private static final InetAddress V6_ADDR2 = toInetAddress("2001:0db8:85a3::8a2e:0370:7335");

    private static final InetAddress V4_GATEWAY = toInetAddress("75.208.8.1");
    private static final InetAddress V6_GATEWAY = toInetAddress("fe80::6:0000:613");

    private static InetAddress toInetAddress(String addr) {
        return InetAddresses.parseNumericAddress(addr);
    }
    @Test
    public void testAddressTypeMatches() {
        assertTrue(NetUtils.addressTypeMatches(V4_ADDR1, V4_ADDR2));
        assertTrue(NetUtils.addressTypeMatches(V6_ADDR1, V6_ADDR2));
        assertFalse(NetUtils.addressTypeMatches(V4_ADDR1, V6_ADDR1));
        assertFalse(NetUtils.addressTypeMatches(V6_ADDR1, V4_ADDR1));
    }

    @Test
    public void testSelectBestRoute() {
        final List<RouteInfo> routes = new ArrayList<>();
        final InetAddress v4_dest = InetAddresses.parseNumericAddress("75.208.8.15");
        final InetAddress v6_dest = InetAddresses.parseNumericAddress("2001:db8:cafe::123");

        RouteInfo route = NetUtils.selectBestRoute(null, v4_dest);
        assertEquals(null, route);
        route = NetUtils.selectBestRoute(routes, null);
        assertEquals(null, route);

        route = NetUtils.selectBestRoute(routes, v4_dest);
        assertEquals(null, route);

        final RouteInfo v4_expected = new RouteInfo(new IpPrefix("75.208.8.10/24"),
                V4_GATEWAY, "wlan0");
        routes.add(v4_expected);
        // "75.208.8.10/16" is not an expected result since it is not the longest prefix.
        routes.add(new RouteInfo(new IpPrefix("75.208.8.10/16"), V4_GATEWAY, "wlan0"));
        routes.add(new RouteInfo(new IpPrefix("75.208.7.32/24"), V4_GATEWAY, "wlan0"));

        final RouteInfo v6_expected = new RouteInfo(new IpPrefix("2001:db8:cafe::/64"),
                V6_GATEWAY, "wlan0");
        routes.add(v6_expected);
        // "2001:db8:cafe::123/32" is not an expected result since it is not the longest prefix.
        routes.add(new RouteInfo(new IpPrefix("2001:db8:cafe::123/32"), V6_GATEWAY, "wlan0"));
        routes.add(new RouteInfo(new IpPrefix("2001:db8:beef::/64"), V6_GATEWAY, "wlan0"));

        // Verify expected v4 route is selected
        route = NetUtils.selectBestRoute(routes, v4_dest);
        assertEquals(v4_expected, route);

        // Verify expected v6 route is selected
        route = NetUtils.selectBestRoute(routes, v6_dest);
        assertEquals(v6_expected, route);

        // Remove expected v4 route
        routes.remove(v4_expected);
        route = NetUtils.selectBestRoute(routes, v4_dest);
        assertNotEquals(v4_expected, route);

        // Remove expected v6 route
        routes.remove(v6_expected);
        route = NetUtils.selectBestRoute(routes, v4_dest);
        assertNotEquals(v6_expected, route);
    }
}

