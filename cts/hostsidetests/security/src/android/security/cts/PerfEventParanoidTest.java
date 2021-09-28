/*
 * Copyright (C) 2016 The Android Open Source Project
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
package android.security.cts;

import com.android.compatibility.common.util.CddTest;
import com.android.compatibility.common.util.PropertyUtil;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.testtype.DeviceTestCase;

/**
 * Tests permission/security controls of the perf_event_open syscall.
 */
public class PerfEventParanoidTest extends DeviceTestCase {

    // A reference to the device under test.
    private ITestDevice mDevice;

    private static final String PERF_EVENT_PARANOID_PATH = "/proc/sys/kernel/perf_event_paranoid";
    private static final String PERF_EVENT_LSM_SYSPROP = "sys.init.perf_lsm_hooks";

    private static final int ANDROID_R_API_LEVEL = 30;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mDevice = getDevice();
    }

    @CddTest(requirement="9.7")
    public void testPerfEventRestricted() throws DeviceNotAvailableException {
        // Property set to "1" if init detected that the kernel has the perf_event_open SELinux
        // hooks, otherwise left unset.
        long lsmHookPropValue = mDevice.getIntProperty(PERF_EVENT_LSM_SYSPROP, 0);

        // Contents of the perf_event_paranoid sysctl procfs file.
        String paranoidCmd = "cat " + PERF_EVENT_PARANOID_PATH;
        String paranoidOut = mDevice.executeShellCommand(paranoidCmd);

        if (PropertyUtil.getFirstApiLevel(mDevice) >= ANDROID_R_API_LEVEL) {
            // On devices launching with Android R or above, the kernel must have the LSM hooks.
            if (lsmHookPropValue != 1) {
                fail("\nDevices launching with Android R or above are required to have SELinux "
                        + "hooks for the perf_event_open(2) syscall.\n"
                        + "Please apply the relevant patch for your kernel located here:\n"
                        + "https://android-review.googlesource.com/q/hashtag:perf_event_lsm");
            }
        } else {
            // Devices upgrading to Android R can have either the LSM hooks, or
            // default to perf_event_paranoid=3.
            if (lsmHookPropValue != 1 && !paranoidOut.equals("3\n")) {
                fail("\nDevice required to have either:\n"
                        + " (a) SELinux hooks for the perf_event_open(2) syscall\n"
                        + " (b) /proc/sys/kernel/perf_event_paranoid=3\n"
                        + "For (a), apply the relevant patch for your kernel located here:\n"
                        + "https://android-review.googlesource.com/q/hashtag:perf_event_lsm\n"
                        + "For (b), apply the relevant patch for your kernel located here:\n"
                        + "https://android-review.googlesource.com/#/q/topic:CONFIG_SECURITY_PERF_EVENTS_RESTRICT\n"
                        + "Device values: SELinux=" + lsmHookPropValue
                        + ", paranoid=" + paranoidOut);
            }
        }
    }
}
