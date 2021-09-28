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
import static org.junit.Assert.assertTrue;

import android.net.MacAddress;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public final class MacAddressUtilsTest {

    // Matches WifiInfo.DEFAULT_MAC_ADDRESS
    private static final String DEFAULT_MAC_ADDRESS = "02:00:00:00:00:00";

    @Test
    public void testIsMulticastAddress() {
        MacAddress[] multicastAddresses = {
            // broadcast address
            MacAddress.fromString("ff:ff:ff:ff:ff:ff"),
            MacAddress.fromString("07:00:d3:56:8a:c4"),
            MacAddress.fromString("33:33:aa:bb:cc:dd"),
        };
        MacAddress[] unicastAddresses = {
            // all zero address
            MacAddress.fromString("00:00:00:00:00:00"),
            MacAddress.fromString("00:01:44:55:66:77"),
            MacAddress.fromString("08:00:22:33:44:55"),
            MacAddress.fromString("06:00:00:00:00:00"),
        };

        for (MacAddress mac : multicastAddresses) {
            String msg = mac.toString() + " expected to be a multicast address";
            assertTrue(msg, MacAddressUtils.isMulticastAddress(mac));
        }
        for (MacAddress mac : unicastAddresses) {
            String msg = mac.toString() + " expected not to be a multicast address";
            assertFalse(msg, MacAddressUtils.isMulticastAddress(mac));
        }
    }

    @Test
    public void testMacAddressRandomGeneration() {
        final int iterations = 1000;

        for (int i = 0; i < iterations; i++) {
            MacAddress mac = MacAddressUtils.createRandomUnicastAddress();
            String stringRepr = mac.toString();

            assertTrue(stringRepr + " expected to be a locally assigned address",
                    mac.isLocallyAssigned());
            assertEquals(MacAddress.TYPE_UNICAST, mac.getAddressType());
            assertFalse(mac.equals(DEFAULT_MAC_ADDRESS));
        }
    }
}
