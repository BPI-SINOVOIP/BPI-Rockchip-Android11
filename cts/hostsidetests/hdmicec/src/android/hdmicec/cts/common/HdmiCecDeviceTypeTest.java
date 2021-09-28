/*
 * Copyright 2020 The Android Open Source Project
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

package android.hdmicec.cts.common;

import static com.google.common.truth.Truth.assertThat;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Scanner;

/** Tests to see that a valid HDMI CEC device type is declared by the device. */
@RunWith(DeviceJUnit4ClassRunner.class)
public final class HdmiCecDeviceTypeTest extends BaseHostJUnit4Test {

    private static List<String> validTypes = new ArrayList<>(
        Arrays.asList("", "0", "4", "4,5", "5,4"));
    /**
     * Tests that the device declares a valid HDMI CEC device type.
     */
    @Test
    public void checkHdmiCecDeviceType() throws Exception {
        ITestDevice device = getDevice();
        String logs = device.executeShellCommand("cmd package list features");
        Scanner in = new Scanner(logs);
        while (in.hasNextLine()) {
            String line = in.nextLine();
            if (line.equals("feature:android.software.leanback")) {
                // Remove "" as valid device type if android.software.leanback feature is supported
                validTypes.remove("");
            }
        }
        String deviceType = device.executeShellCommand("getprop ro.hdmi.device_type");
        assertThat(deviceType.trim()).isIn(validTypes);
    }
}
