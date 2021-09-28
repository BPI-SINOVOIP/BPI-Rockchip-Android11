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

package android.wifibroadcasts.cts;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.util.CommandResult;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Scanner;

/**
 * Test to check the APK logs to Logcat.
 *
 * When this test builds, it also builds
 * {@link android.wifibroadcasts.app.WifiBroadcastsDeviceActivity} into an
 * APK which is then installed at runtime and started. That activity prints a message to
 * Logcat if an unexpected broadcast is received.
 *
 * Instead of extending DeviceTestCase, this JUnit4 test extends IDeviceTest and is run with
 * tradefed's DeviceJUnit4ClassRunner
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class WifiBroadcastsHostJUnit4Test implements IDeviceTest {

    /**
     * The package name of the APK.
     */
    private static final String PACKAGE = "android.wifibroadcasts.app";

    /**
     * The class name of the main activity in the APK.
     */
    private static final String CLASS = "WifiBroadcastsDeviceActivity";

    /**
     * The command to launch the main activity.
     */
    private static final String START_COMMAND = String.format(
            "am start -W -a android.intent.action.MAIN -n %s/%s.%s", PACKAGE, PACKAGE, CLASS);

    /**
     * The command to clear the main activity.
     */
    private static final String CLEAR_COMMAND = String.format("pm clear %s", PACKAGE);

    /**
     * The prohibited string to look for.
     */
    private static final String PROHIBITED_STRING = "UNEXPECTED WIFI BROADCAST RECEIVED";

    /**
     * The maximum total number of times to attempt a ping.
     */
    private static final int MAXIMUM_PING_TRIES = 180;

    /**
     * The maximum number of times to attempt a ping before toggling wifi.
     */
    private static final int MAXIMUM_PING_TRIES_PER_CONNECTION = 60;

    /**
     * Name for wifi feature test
     */
    private static final String FEATURE_WIFI = "android.hardware.wifi";

    private ITestDevice mDevice;

    @Override
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    @Override
    public ITestDevice getDevice() {
        return mDevice;
    }

    /**
     * Tests the string was not logged to Logcat from the activity.
     *
     * @throws Exception
     */
    @Test
    public void testCleanLogcat() throws Exception {
        ITestDevice device = getDevice();
        assertNotNull("Device not set", device);
        if (!device.hasFeature(FEATURE_WIFI)) {
            return;
        }
        // Clear activity
        device.executeShellCommand(CLEAR_COMMAND);
        // No mobile data or wifi or bluetooth to start with
        device.executeShellCommand("svc data disable; svc wifi disable; svc bluetooth disable");
        // Clear logcat.
        device.executeAdbCommand("logcat", "-c");
        // Ensure the screen is on, so that rssi polling happens
        device.executeShellCommand("input keyevent KEYCODE_WAKEUP");
        // Start the APK
        device.executeShellCommand(START_COMMAND);
        // Bring up wifi
        device.executeShellCommand("svc wifi enable; sleep 1");
        // Make sure wifi comes up
        String pingResult = "";
        CommandResult pingCommandResult = null;
        boolean pingSucceeded = false;
        for (int tries = 0; tries < MAXIMUM_PING_TRIES; tries++) {
            if (tries > 0 && tries % MAXIMUM_PING_TRIES_PER_CONNECTION == 0) {
                // if we have been trying for a while, toggle wifi off and then on.
                device.executeShellCommand("svc wifi disable; sleep 1; svc wifi enable; sleep 3");
            }
            // We don't require internet connectivity, just a configured address
            pingCommandResult = device.executeShellV2Command("ping -c 4 -W 2 -t 1 8.8.8.8");
            pingResult = String.join("/", pingCommandResult.getStdout(),
                                          pingCommandResult.getStderr(),
                                          pingCommandResult.getStatus().toString());
            if (pingResult.contains("4 packets transmitted")) {
                pingSucceeded = true;
                break;
            }
            Thread.sleep(1000);
        }
        // Stop wifi
        device.executeShellCommand("svc wifi disable");

        assertTrue("Wi-Fi network unavailable - test could not complete " + pingResult,
                pingSucceeded);

        // Dump logcat.
        String logs = device.executeAdbCommand("logcat", "-v", "brief", "-d", CLASS + ":I", "*:S");
        // Search for prohibited string.
        Scanner in = new Scanner(logs);
        try {
            while (in.hasNextLine()) {
                String line = in.nextLine();
                if (line.startsWith("I/" + CLASS)) {
                    String payload = line.split(":")[1].trim();
                    assertFalse(payload, payload.contains(PROHIBITED_STRING));
                }
            }
        } finally {
            in.close();
        }
        //Re-enable Wi-Fi as part of CTS Pre-conditions
        device.executeShellCommand("svc wifi enable; sleep 1");
    }
}
