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

package android.testharness.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test that collects test results from the device after enabling Test Harness Mode
 *
 * <p><b>Note:</b> When this test executes, it places the device into "Test Harness Mode," which
 * should set up settings on the device (perhaps differently from how it was originally set up).
 * When running CTS tests after this suite, a factory reset will have to be performed and ADB will
 * need to be reauthorized.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class TestHarnessModeDeviceTest extends BaseHostJUnit4Test {
    private static final int ONE_MINUTE = 60 * 1000;

    @Test
    public void testHarnessModeDeletesFiles() throws Exception {
        // For reasons I can't explain, writing files to the sdcard immediately after boot complete
        // fails. Let's retry 10 times, waiting 3 seconds each time

        for (int i = 0; i < 10; i++) {
            // Write some files to the device that we can check for after the test is complete.
            getDevice().executeShellCommand("touch /sdcard/test.txt");
            getDevice().executeShellCommand("touch /data/local/tmp/test.txt");

            if (getDevice().doesFileExist("/sdcard/test.txt")
                    && getDevice().doesFileExist("/data/local/tmp/test.txt")) {
                break;
            }
            Thread.sleep(3_000);
        }

        // Ensure the files are there, otherwise they may have been "erased" simply by never showing
        // up
        assertFileExists("/sdcard/test.txt");
        assertFileExists("/data/local/tmp/test.txt");

        enableTestHarnessModeAndWait();

        assertFileDoesNotExist("/sdcard/test.txt");
        assertFileDoesNotExist("/data/local/tmp/test.txt");
    }

    @Test
    public void testHarnessModeRemovesAndResetsSettings() throws Exception {
        String originalAnimationScale =
                getDevice().executeShellCommand("settings get global transition_animation_scale");

        getDevice().executeShellCommand("settings put global test_harness_test 1234");
        getDevice().executeShellCommand("settings put secure test_harness_test 1234");
        getDevice().executeShellCommand("settings put system test_harness_test 1234");
        getDevice().executeShellCommand("settings put global transition_animation_scale 2.0");

        enableTestHarnessModeAndWait();

        assertEquals(
                "New global setting was not removed",
                "null",
                getDevice().executeShellCommand("settings get global test_harness_test").trim());
        assertEquals(
                "New secure setting was not removed",
                "null",
                getDevice().executeShellCommand("settings get secure test_harness_test").trim());
        assertEquals(
                "New system setting was not removed",
                "null",
                getDevice().executeShellCommand("settings get system test_harness_test").trim());
        assertEquals(
                "Modified global setting was not restored",
                originalAnimationScale,
                getDevice()
                        .executeShellCommand("settings get global transition_animation_scale"));
    }

    @Test
    public void testHarnessModeRemovesInstalledAppsAndData() throws Exception {
        installPackage("CtsTestHarnessModeDeviceApp.apk");
        runTest("dirtyDevice");

        enableTestHarnessModeAndWait();

        installPackage("CtsTestHarnessModeDeviceApp.apk");
        Assert.assertTrue(runTest("testDeviceInTestHarnessMode"));
        Assert.assertTrue(runTest("ensureActivityNotObscuredByKeyboardSetUpScreen"));
        Assert.assertTrue(runTest("testDeviceIsClean"));
    }

    private boolean runTest(String methodName) throws Exception {
        return runDeviceTests(
                "android.testharness.app",
                "android.testharness.app.TestHarnessModeDeviceTest",
                methodName);
    }

    private void enableTestHarnessModeAndWait()
            throws InterruptedException, DeviceNotAvailableException {
        if (getDevice().executeShellV2Command("cmd testharness enable").getExitCode() != 0) {
            throw new RuntimeException("Failed to enable test harness mode");
        }
        Thread.sleep(20 * 1000); // Wait for the device to start resetting.
        try {
            getDevice().waitForDeviceOnline(5 * ONE_MINUTE);
            getDevice().waitForBootComplete(ONE_MINUTE);

            Thread.sleep(20 * 1000); // Wait 20 more seconds to ensure that the device has booted
        } catch (DeviceNotAvailableException e) {
            Assert.fail("Device did not come back online after 5 minutes. "
                    + "Did the ADB keys not get stored?");
        }
    }

    private void assertFileExists(String devicePath) throws DeviceNotAvailableException {
        assertTrue(
                String.format("%s does not exist on the device", devicePath),
                getDevice().doesFileExist(devicePath));
    }

    private void assertFileDoesNotExist(String path) throws DeviceNotAvailableException {
        assertFalse(
                String.format("'%s' exists on the device, but it should have been wiped.", path),
                getDevice().doesFileExist(path));
    }
}
