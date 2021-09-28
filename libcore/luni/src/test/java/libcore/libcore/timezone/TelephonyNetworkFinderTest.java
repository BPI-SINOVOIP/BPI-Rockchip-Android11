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

package libcore.libcore.timezone;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import libcore.timezone.TelephonyNetwork;
import libcore.timezone.TelephonyNetworkFinder;

import org.junit.Test;

import java.util.Arrays;
import java.util.List;

public class TelephonyNetworkFinderTest {

    @Test
    public void testCreateAndLookups() {
        TelephonyNetwork network = TelephonyNetwork.create("123", "456", "gb");
        List<TelephonyNetwork> networkList = list(network);
        TelephonyNetworkFinder finder = TelephonyNetworkFinder.create(networkList);
        assertEquals(network, finder.findNetworkByMccMnc("123", "456"));
        assertNull(finder.findNetworkByMccMnc("XXX", "XXX"));
        assertNull(finder.findNetworkByMccMnc("123", "XXX"));
        assertNull(finder.findNetworkByMccMnc("456", "123"));
        assertNull(finder.findNetworkByMccMnc("111", "222"));
        assertEquals(networkList, finder.getAll());
    }

    private static <X> List<X> list(X... values) {
        return Arrays.asList(values);
    }
}
