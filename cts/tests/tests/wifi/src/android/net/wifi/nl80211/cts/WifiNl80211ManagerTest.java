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

package android.net.wifi.nl80211.cts;

import static android.net.wifi.nl80211.WifiNl80211Manager.OemSecurityType;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.net.wifi.ScanResult;
import android.net.wifi.cts.WifiFeature;
import android.net.wifi.nl80211.WifiNl80211Manager;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;

/** CTS tests for {@link WifiNl80211Manager}. */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class WifiNl80211ManagerTest {

    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getContext();
        // skip tests if Wifi is not supported
        assumeTrue(WifiFeature.isWifiSupported(mContext));
    }

    @Test
    public void testOemSecurityTypeConstructor() {
        OemSecurityType securityType = new OemSecurityType(
                ScanResult.PROTOCOL_WPA,
                Arrays.asList(ScanResult.KEY_MGMT_PSK, ScanResult.KEY_MGMT_SAE),
                Arrays.asList(ScanResult.CIPHER_NONE, ScanResult.CIPHER_TKIP),
                ScanResult.CIPHER_CCMP);

        assertThat(securityType.protocol).isEqualTo(ScanResult.PROTOCOL_WPA);
        assertThat(securityType.keyManagement)
                .isEqualTo(Arrays.asList(ScanResult.KEY_MGMT_PSK, ScanResult.KEY_MGMT_SAE));
        assertThat(securityType.pairwiseCipher)
                .isEqualTo(Arrays.asList(ScanResult.CIPHER_NONE, ScanResult.CIPHER_TKIP));
        assertThat(securityType.groupCipher).isEqualTo(ScanResult.CIPHER_CCMP);
    }

    @Test
    public void testSendMgmtFrame() {
        try {
            WifiNl80211Manager manager = mContext.getSystemService(WifiNl80211Manager.class);
            manager.sendMgmtFrame("wlan0", new byte[]{}, -1, Runnable::run,
                    new WifiNl80211Manager.SendMgmtFrameCallback() {
                        @Override
                        public void onAck(int elapsedTimeMs) {}

                        @Override
                        public void onFailure(int reason) {}
                    });
        } catch (Exception ignore) {}
    }

    @Test
    public void testGetTxPacketCounters() {
        try {
            WifiNl80211Manager manager = mContext.getSystemService(WifiNl80211Manager.class);
            manager.getTxPacketCounters("wlan0");
        } catch (Exception ignore) {}
    }

    @Test
    public void testSetOnServiceDeadCallback() {
        try {
            WifiNl80211Manager manager = mContext.getSystemService(WifiNl80211Manager.class);
            manager.setOnServiceDeadCallback(() -> {});
        } catch (Exception ignore) {}
    }
}
