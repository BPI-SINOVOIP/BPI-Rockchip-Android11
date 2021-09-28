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

import android.net.wifi.p2p.WifiP2pWfdInfo;
import android.test.AndroidTestCase;

public class WifiP2pWfdInfoTest extends AndroidTestCase {

    private final int TEST_DEVICE_TYPE = WifiP2pWfdInfo.DEVICE_TYPE_WFD_SOURCE;
    private final boolean TEST_DEVICE_ENABLE_STATUS = true;
    private final boolean TEST_SESSION_STATUS = true;
    private final int TEST_CONTROL_PORT = 9999;
    private final int TEST_MAX_THROUGHPUT = 1024;
    private final boolean TEST_CONTENT_PROTECTION_SUPPORTED_STATUS = true;

    public void testWifiP2pWfdInfo() {
        WifiP2pWfdInfo info = new WifiP2pWfdInfo();

        info.setDeviceType(TEST_DEVICE_TYPE);
        info.setEnabled(TEST_DEVICE_ENABLE_STATUS);
        info.setSessionAvailable(true);
        info.setControlPort(TEST_CONTROL_PORT);
        info.setMaxThroughput(TEST_MAX_THROUGHPUT);
        info.setContentProtectionSupported(true);

        WifiP2pWfdInfo copiedInfo = new WifiP2pWfdInfo(info);
        assertEquals(TEST_DEVICE_TYPE, copiedInfo.getDeviceType());
        assertEquals(TEST_DEVICE_ENABLE_STATUS, copiedInfo.isEnabled());
        assertEquals(TEST_SESSION_STATUS, copiedInfo.isSessionAvailable());
        assertEquals(TEST_CONTROL_PORT, copiedInfo.getControlPort());
        assertEquals(TEST_MAX_THROUGHPUT, copiedInfo.getMaxThroughput());
        assertEquals(TEST_CONTENT_PROTECTION_SUPPORTED_STATUS,
                copiedInfo.isContentProtectionSupported());
    }
}
