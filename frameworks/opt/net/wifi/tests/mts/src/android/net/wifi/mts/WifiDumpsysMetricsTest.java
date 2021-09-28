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

package android.net.wifi.mts;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.util.Base64;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.server.wifi.proto.nano.WifiMetricsProto.WifiLog;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class WifiDumpsysMetricsTest {

    private static final String WIFI_DUMP_PROTO_CMD = "/system/bin/dumpsys wifi wifiMetricsProto";

    private static final String START_TAG = "WifiMetrics:\n";
    private static final String END_TAG = "\nEndWifiMetrics";

    @Before
    public void setUp() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        assumeTrue(WifiFeature.isWifiSupported(context));
    }

    /**
     * Test that Wifi dumps metrics in the expected format.
     *
     * Assumption:
     *   - Test device should have at least 1 saved network
     */
    @Test
    public void testWifiDumpMetrics() throws Exception {
        // DO NOT run under shell identity. Shell has a lot more permissions.
        // `dumpsys wifi wifiMetricsProto` should ONLY need `android.permission.DUMP`
        String rawDumpOutput = StreamReader.runProcessCommand(WIFI_DUMP_PROTO_CMD);

        assertThat(rawDumpOutput).isNotNull();

        int protoStart = rawDumpOutput.indexOf(START_TAG);
        int protoEnd = rawDumpOutput.indexOf(END_TAG);

        assertWithMessage("Expected to find \"WifiMetrics:\", but instead found: " + rawDumpOutput)
                .that(protoStart).isAtLeast(0);
        assertWithMessage(
                "Expected to find \"EndWifiMetrics\", but instead found: " + rawDumpOutput)
                .that(protoEnd).isAtLeast(protoStart);

        String protoString = rawDumpOutput.substring(protoStart + START_TAG.length(), protoEnd);
        byte[] protoBytes = Base64.decode(protoString, Base64.DEFAULT);

        WifiLog wifiLog = WifiLog.parseFrom(protoBytes);

        assertThat(wifiLog.numSavedNetworks).isAtLeast(1);
        assertThat(wifiLog.numOpenNetworks).isAtLeast(0);
        assertThat(wifiLog.numHiddenNetworks).isAtLeast(0);
        assertThat(wifiLog.numWificondCrashes).isAtLeast(0);
        assertThat(wifiLog.numSupplicantCrashes).isAtLeast(0);
    }
}
