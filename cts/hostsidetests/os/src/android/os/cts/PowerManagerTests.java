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

package android.os.cts;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;

import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import java.util.regex.Pattern;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
public class PowerManagerTests extends BaseHostJUnit4Test {
    // We need a running test app to test cached processes releasing wake locks.
    private static final String PACKAGE_NAME = "android.os.powermanagertests";
    private static final String APK_NAME = "CtsHostPowerManagerTestApp.apk";
    private static final String WAKE_LOCK = "WakeLockTest";

    private static final String CHECK_CACHED_CMD = "dumpsys activity processes";
    private static final String CACHED_REGEXP = "Proc # [0-9]+: cch.*" + PACKAGE_NAME;
    private static final Pattern CACHED_PATTERN = Pattern.compile(CACHED_REGEXP);

    private static final String GET_WAKE_LOCKS_CMD = "dumpsys power";
    private static final String WAKE_LOCK_ACQUIRED_REGEXP =
            "PARTIAL_WAKE_LOCK.*" + WAKE_LOCK + ".*ACQ";
    private static final String WAKE_LOCK_DISABLED_REGEXP =
                        "PARTIAL_WAKE_LOCK.*" + WAKE_LOCK + ".*DISABLED";
    private static final Pattern WAKE_LOCK_ACQUIRED_PATTERN =
            Pattern.compile(WAKE_LOCK_ACQUIRED_REGEXP);
    private static final Pattern WAKE_LOCK_DISABLED_PATTERN =
            Pattern.compile(WAKE_LOCK_DISABLED_REGEXP);

    // A reference to the device under test, which gives us a handle to run commands.
    private ITestDevice mDevice;

    @Before
    public synchronized void setUp() throws Exception {
        mDevice = getDevice();
        installPackage(APK_NAME);
    }

    /**
     * Tests that cached process releases wake lock.
     *
     * @throws Exception
     */
    @Test
    @Ignore("b/135053775")
    public void testCachedProcessReleasesWakeLock() throws Exception {
        // Turn screen on and unlock keyguard
        mDevice.executeShellCommand("input keyevent 26; input keyevent 82");
        makeCachedProcess(PACKAGE_NAME);
        // Short wait before checking cached process
        Thread.sleep(1000);
        String processes = mDevice.executeShellCommand(CHECK_CACHED_CMD);
        assertTrue(CACHED_PATTERN.matcher(processes).find());

        String wakelocks = mDevice.executeShellCommand(GET_WAKE_LOCKS_CMD);
        assertFalse("Wake lock disabled early",
                WAKE_LOCK_DISABLED_PATTERN.matcher(wakelocks).find());
        assertTrue("Wake lock not acquired", WAKE_LOCK_ACQUIRED_PATTERN.matcher(wakelocks).find());

        // ActivityManager will inform PowerManager of processes with idle uids.
        // PowerManager will disable wakelocks held by such processes.
        mDevice.executeShellCommand("am make-uid-idle " + PACKAGE_NAME);

        wakelocks = mDevice.executeShellCommand(GET_WAKE_LOCKS_CMD);
        assertFalse("Wake lock still acquired",
                WAKE_LOCK_ACQUIRED_PATTERN.matcher(wakelocks).find());
        assertTrue("Wake lock not disabled", WAKE_LOCK_DISABLED_PATTERN.matcher(wakelocks).find());
    }

    private void makeCachedProcess(String packageName) throws Exception {
        String startAppTemplate = "am start -W -a android.intent.action.MAIN -p %s"
                + " -c android.intent.category.LAUNCHER";
        String viewUriTemplate = "am start -W -a android.intent.action.VIEW -d %s";
        mDevice.executeShellCommand(String.format(startAppTemplate, packageName));
        // Starting two random apps to make packageName cached
        mDevice.executeShellCommand(String.format(viewUriTemplate, "mailto:"));
        mDevice.executeShellCommand(String.format(viewUriTemplate, "tel:"));
    }
}
