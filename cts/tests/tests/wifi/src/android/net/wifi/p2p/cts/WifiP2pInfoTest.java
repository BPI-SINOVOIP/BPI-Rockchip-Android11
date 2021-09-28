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

package android.net.wifi.p2p.cts;

import android.net.InetAddresses;
import android.net.wifi.p2p.WifiP2pInfo;
import android.test.AndroidTestCase;

public class WifiP2pInfoTest extends AndroidTestCase {

    public String TEST_GROUP_OWNER_ADDRESS = "192.168.43.1";

    public void testWifiP2pInfoNoGroup() {
        WifiP2pInfo info = new WifiP2pInfo();
        info.groupFormed = false;

        WifiP2pInfo copiedInfo = new WifiP2pInfo(info);
        assertEquals(info.groupFormed, copiedInfo.groupFormed);
        assertEquals(info.isGroupOwner, copiedInfo.isGroupOwner);
        assertEquals(info.groupOwnerAddress, copiedInfo.groupOwnerAddress);
    }

    public void testWifiP2pInfoGroupOwner() {
        WifiP2pInfo info = new WifiP2pInfo();
        info.groupFormed = true;
        info.isGroupOwner = true;
        info.groupOwnerAddress = InetAddresses.parseNumericAddress(TEST_GROUP_OWNER_ADDRESS);

        WifiP2pInfo copiedInfo = new WifiP2pInfo(info);
        assertEquals(info.groupFormed, copiedInfo.groupFormed);
        assertEquals(info.isGroupOwner, copiedInfo.isGroupOwner);
        assertEquals(info.groupOwnerAddress, copiedInfo.groupOwnerAddress);
    }

    public void testWifiP2pInfoGroupClient() {
        WifiP2pInfo info = new WifiP2pInfo();
        info.groupFormed = true;
        info.isGroupOwner = false;
        info.groupOwnerAddress = InetAddresses.parseNumericAddress(TEST_GROUP_OWNER_ADDRESS);

        WifiP2pInfo copiedInfo = new WifiP2pInfo(info);
        assertEquals(info.groupFormed, copiedInfo.groupFormed);
        assertEquals(info.isGroupOwner, copiedInfo.isGroupOwner);
        assertEquals(info.groupOwnerAddress, copiedInfo.groupOwnerAddress);
    }
}
