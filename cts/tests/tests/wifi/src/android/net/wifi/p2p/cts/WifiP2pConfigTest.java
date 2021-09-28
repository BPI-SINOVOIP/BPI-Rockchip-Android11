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

package android.net.wifi.p2p.cts;

import android.net.MacAddress;
import android.net.wifi.p2p.WifiP2pConfig;
import android.net.wifi.p2p.WifiP2pGroup;
import android.test.AndroidTestCase;

public class WifiP2pConfigTest extends AndroidTestCase {
    private static final String TEST_NETWORK_NAME = "DIRECT-xy-Hello";
    private static final String TEST_PASSPHRASE = "8etterW0r1d";
    private static final int TEST_OWNER_BAND = WifiP2pConfig.GROUP_OWNER_BAND_5GHZ;
    private static final int TEST_OWNER_FREQ = 2447;
    private static final String TEST_DEVICE_ADDRESS = "aa:bb:cc:dd:ee:ff";

    public void testWifiP2pConfigCopyConstructor() {
        WifiP2pConfig config = new WifiP2pConfig.Builder()
                .setNetworkName(TEST_NETWORK_NAME)
                .setPassphrase(TEST_PASSPHRASE)
                .setGroupOperatingBand(TEST_OWNER_BAND)
                .setDeviceAddress(MacAddress.fromString(TEST_DEVICE_ADDRESS))
                .enablePersistentMode(true)
                .build();

        WifiP2pConfig copiedConfig = new WifiP2pConfig(config);

        assertEquals(copiedConfig.deviceAddress, TEST_DEVICE_ADDRESS);
        assertEquals(copiedConfig.getNetworkName(), TEST_NETWORK_NAME);
        assertEquals(copiedConfig.getPassphrase(), TEST_PASSPHRASE);
        assertEquals(copiedConfig.getGroupOwnerBand(), TEST_OWNER_BAND);
        assertEquals(copiedConfig.getNetworkId(), WifiP2pGroup.NETWORK_ID_PERSISTENT);
    }

    public void testWifiP2pConfigBuilderForPersist() {
        WifiP2pConfig config = new WifiP2pConfig.Builder()
                .setNetworkName(TEST_NETWORK_NAME)
                .setPassphrase(TEST_PASSPHRASE)
                .setGroupOperatingBand(TEST_OWNER_BAND)
                .setDeviceAddress(MacAddress.fromString(TEST_DEVICE_ADDRESS))
                .enablePersistentMode(true)
                .build();

        assertEquals(config.deviceAddress, TEST_DEVICE_ADDRESS);
        assertEquals(config.getNetworkName(), TEST_NETWORK_NAME);
        assertEquals(config.getPassphrase(), TEST_PASSPHRASE);
        assertEquals(config.getGroupOwnerBand(), TEST_OWNER_BAND);
        assertEquals(config.getNetworkId(), WifiP2pGroup.NETWORK_ID_PERSISTENT);
    }

    public void testWifiP2pConfigBuilderForNonPersist() {
        WifiP2pConfig config = new WifiP2pConfig.Builder()
                .setNetworkName(TEST_NETWORK_NAME)
                .setPassphrase(TEST_PASSPHRASE)
                .setGroupOperatingFrequency(TEST_OWNER_FREQ)
                .setDeviceAddress(MacAddress.fromString(TEST_DEVICE_ADDRESS))
                .enablePersistentMode(false)
                .build();

        assertEquals(config.deviceAddress, TEST_DEVICE_ADDRESS);
        assertEquals(config.getNetworkName(), TEST_NETWORK_NAME);
        assertEquals(config.getPassphrase(), TEST_PASSPHRASE);
        assertEquals(config.getGroupOwnerBand(), TEST_OWNER_FREQ);
        assertEquals(config.getNetworkId(), WifiP2pGroup.NETWORK_ID_TEMPORARY);
    }
}
