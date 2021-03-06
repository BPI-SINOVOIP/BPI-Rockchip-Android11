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

import static android.net.MacAddress.TYPE_BROADCAST;
import static android.net.MacAddress.TYPE_MULTICAST;
import static android.net.MacAddress.TYPE_UNICAST;

import static com.android.testutils.ParcelUtilsKt.assertParcelSane;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.net.MacAddress;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.net.Inet6Address;
import java.util.Arrays;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class MacAddressTest {

    static class TestCase {
        final String macAddress;
        final String ouiString;
        final int addressType;
        final boolean isLocallyAssigned;

        TestCase(String macAddress, String ouiString, int addressType, boolean isLocallyAssigned) {
            this.macAddress = macAddress;
            this.ouiString = ouiString;
            this.addressType = addressType;
            this.isLocallyAssigned = isLocallyAssigned;
        }
    }

    static final boolean LOCALLY_ASSIGNED = true;
    static final boolean GLOBALLY_UNIQUE = false;

    static String typeToString(int addressType) {
        switch (addressType) {
            case TYPE_UNICAST:
                return "TYPE_UNICAST";
            case TYPE_BROADCAST:
                return "TYPE_BROADCAST";
            case TYPE_MULTICAST:
                return "TYPE_MULTICAST";
            default:
                return "UNKNOWN";
        }
    }

    static String localAssignedToString(boolean isLocallyAssigned) {
        return isLocallyAssigned ? "LOCALLY_ASSIGNED" : "GLOBALLY_UNIQUE";
    }

    @Test
    public void testMacAddress() {
        TestCase[] tests = {
            new TestCase("ff:ff:ff:ff:ff:ff", "ff:ff:ff", TYPE_BROADCAST, LOCALLY_ASSIGNED),
            new TestCase("d2:c4:22:4d:32:a8", "d2:c4:22", TYPE_UNICAST, LOCALLY_ASSIGNED),
            new TestCase("33:33:aa:bb:cc:dd", "33:33:aa", TYPE_MULTICAST, LOCALLY_ASSIGNED),
            new TestCase("06:00:00:00:00:00", "06:00:00", TYPE_UNICAST, LOCALLY_ASSIGNED),
            new TestCase("07:00:d3:56:8a:c4", "07:00:d3", TYPE_MULTICAST, LOCALLY_ASSIGNED),
            new TestCase("00:01:44:55:66:77", "00:01:44", TYPE_UNICAST, GLOBALLY_UNIQUE),
            new TestCase("08:00:22:33:44:55", "08:00:22", TYPE_UNICAST, GLOBALLY_UNIQUE),
        };

        for (TestCase tc : tests) {
            MacAddress mac = MacAddress.fromString(tc.macAddress);

            if (!tc.ouiString.equals(mac.toOuiString())) {
                fail(String.format("expected OUI string %s, got %s",
                        tc.ouiString, mac.toOuiString()));
            }

            if (tc.isLocallyAssigned != mac.isLocallyAssigned()) {
                fail(String.format("expected %s to be %s, got %s", mac,
                        localAssignedToString(tc.isLocallyAssigned),
                        localAssignedToString(mac.isLocallyAssigned())));
            }

            if (tc.addressType != mac.getAddressType()) {
                fail(String.format("expected %s address type to be %s, got %s", mac,
                        typeToString(tc.addressType), typeToString(mac.getAddressType())));
            }

            if (!tc.macAddress.equals(mac.toString())) {
                fail(String.format("expected toString() to return %s, got %s",
                        tc.macAddress, mac.toString()));
            }

            if (!mac.equals(MacAddress.fromBytes(mac.toByteArray()))) {
                byte[] bytes = mac.toByteArray();
                fail(String.format("expected mac address from bytes %s to be %s, got %s",
                        Arrays.toString(bytes),
                        MacAddress.fromBytes(bytes),
                        mac));
            }
        }
    }

    @Test
    public void testConstructorInputValidation() {
        String[] invalidStringAddresses = {
            "",
            "abcd",
            "1:2:3:4:5",
            "1:2:3:4:5:6:7",
            "10000:2:3:4:5:6",
        };

        for (String s : invalidStringAddresses) {
            try {
                MacAddress mac = MacAddress.fromString(s);
                fail("MacAddress.fromString(" + s + ") should have failed, but returned " + mac);
            } catch (IllegalArgumentException excepted) {
            }
        }

        try {
            MacAddress mac = MacAddress.fromString(null);
            fail("MacAddress.fromString(null) should have failed, but returned " + mac);
        } catch (NullPointerException excepted) {
        }

        byte[][] invalidBytesAddresses = {
            {},
            {1,2,3,4,5},
            {1,2,3,4,5,6,7},
        };

        for (byte[] b : invalidBytesAddresses) {
            try {
                MacAddress mac = MacAddress.fromBytes(b);
                fail("MacAddress.fromBytes(" + Arrays.toString(b)
                        + ") should have failed, but returned " + mac);
            } catch (IllegalArgumentException excepted) {
            }
        }

        try {
            MacAddress mac = MacAddress.fromBytes(null);
            fail("MacAddress.fromBytes(null) should have failed, but returned " + mac);
        } catch (NullPointerException excepted) {
        }
    }

    @Test
    public void testMatches() {
        // match 4 bytes prefix
        assertTrue(MacAddress.fromString("aa:bb:cc:dd:ee:11").matches(
                MacAddress.fromString("aa:bb:cc:dd:00:00"),
                MacAddress.fromString("ff:ff:ff:ff:00:00")));

        // match bytes 0,1,2 and 5
        assertTrue(MacAddress.fromString("aa:bb:cc:dd:ee:11").matches(
                MacAddress.fromString("aa:bb:cc:00:00:11"),
                MacAddress.fromString("ff:ff:ff:00:00:ff")));

        // match 34 bit prefix
        assertTrue(MacAddress.fromString("aa:bb:cc:dd:ee:11").matches(
                MacAddress.fromString("aa:bb:cc:dd:c0:00"),
                MacAddress.fromString("ff:ff:ff:ff:c0:00")));

        // fail to match 36 bit prefix
        assertFalse(MacAddress.fromString("aa:bb:cc:dd:ee:11").matches(
                MacAddress.fromString("aa:bb:cc:dd:40:00"),
                MacAddress.fromString("ff:ff:ff:ff:f0:00")));

        // match all 6 bytes
        assertTrue(MacAddress.fromString("aa:bb:cc:dd:ee:11").matches(
                MacAddress.fromString("aa:bb:cc:dd:ee:11"),
                MacAddress.fromString("ff:ff:ff:ff:ff:ff")));

        // match none of 6 bytes
        assertTrue(MacAddress.fromString("aa:bb:cc:dd:ee:11").matches(
                MacAddress.fromString("00:00:00:00:00:00"),
                MacAddress.fromString("00:00:00:00:00:00")));
    }

    /**
     * Tests that link-local address generation from MAC is valid.
     */
    @Test
    public void testLinkLocalFromMacGeneration() {
        final MacAddress mac = MacAddress.fromString("52:74:f2:b1:a8:7f");
        final byte[] inet6ll = {(byte) 0xfe, (byte) 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50,
                0x74, (byte) 0xf2, (byte) 0xff, (byte) 0xfe, (byte) 0xb1, (byte) 0xa8, 0x7f};
        final Inet6Address llv6 = mac.getLinkLocalIpv6FromEui48Mac();
        assertTrue(llv6.isLinkLocalAddress());
        assertArrayEquals(inet6ll, llv6.getAddress());
    }

    @Test
    public void testParcelMacAddress() {
        final MacAddress mac = MacAddress.fromString("52:74:f2:b1:a8:7f");

        assertParcelSane(mac, 1);
    }
}
