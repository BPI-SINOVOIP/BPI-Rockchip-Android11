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

package android.adbmanager.cts;

import com.android.compatibility.common.util.CddTest;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.util.CommandResult;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests the AdbManager System APIs via shell commands.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class AdbManagerHostDeviceTest extends BaseHostJUnit4Test {
    private static final String FEATURE_WIFI = "android.hardware.wifi";
    private static final String FEATURE_CAMERA_ANY = "android.hardware.camera.any";

    private boolean hasFeature(String feature) throws Exception {
        CommandResult result = getDevice().executeShellV2Command("pm has-feature " + feature);
        return Boolean.parseBoolean(result.getStdout().trim());
    }

    @Test
    @CddTest(requirement="6.1/C-1-1")
    public void test_isadbWifiSupported() throws Exception {
        boolean expected = hasFeature(FEATURE_WIFI);

        CommandResult result = getDevice().executeShellV2Command("cmd adb is-wifi-supported");

        Assert.assertTrue(new Integer(0).equals(result.getExitCode()));
        Assert.assertEquals(expected, Boolean.parseBoolean(result.getStdout().trim()));
    }

    @Test
    @CddTest(requirement="6.1/C-1-2")
    public void test_isadbWifiQrSupported() throws Exception {
        boolean expected = hasFeature(FEATURE_WIFI) && hasFeature(FEATURE_CAMERA_ANY);

        CommandResult result = getDevice().executeShellV2Command("cmd adb is-wifi-qr-supported");

        Assert.assertTrue(new Integer(0).equals(result.getExitCode()));
        Assert.assertEquals(expected, Boolean.parseBoolean(result.getStdout().trim()));
    }
}
