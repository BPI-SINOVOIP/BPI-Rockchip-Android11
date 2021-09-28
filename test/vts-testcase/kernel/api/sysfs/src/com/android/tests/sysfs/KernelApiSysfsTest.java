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
package com.android.tests.sysfs;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.TargetFileUtils;
import com.android.tradefed.util.TargetFileUtils.FilePermission;
import com.google.common.base.Strings;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.UUID;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.junit.Test;
import org.junit.runner.RunWith;

/* A test to check check sysfs files. */
@RunWith(DeviceJUnit4ClassRunner.class)
public class KernelApiSysfsTest extends BaseHostJUnit4Test {
    /* Check for the existence of required files in /sys/class/android_usb. */
    @Test
    public void testAndroidUSB() throws Exception {
        String state = "/sys/class/android_usb/android0/state";
        assertTrue(TargetFileUtils.isReadOnly(state, getDevice()));
        String content = getDevice().pullFileContents(state).trim();
        HashSet<String> possibles =
                new HashSet<>(Arrays.asList("DISCONNECTED", "CONNECTED", "CONFIGURED"));
        assertTrue(possibles.contains(content));
    }

    /**
     * Check the format of cpu online file.
     *
     * Confirm /sys/devices/system/cpu/online exists and is read-only.
     * Parse contents to ensure it is a comma-separated series of ranges
     * (%d-%d) and/or integers.
     */
    @Test
    public void testCpuOnlineFormat() throws Exception {
        String filePath = "/sys/devices/system/cpu/online";
        assertTrue(TargetFileUtils.isReadOnly(filePath, getDevice()));
        String content = getDevice().pullFileContents(filePath).trim();
        assertTrue(content.matches("(\\d+(-\\d+)?)(,\\d+(-\\d+)?)*"));
    }

    private void isReadOnlyAndIntegerContent(String filePath) throws Exception {
        assertTrue("Failed readonly check of " + filePath,
                TargetFileUtils.isReadOnly(filePath, getDevice()));
        String content = getDevice().pullFileContents(filePath).trim();
        // Confirm the content is integer.
        Integer.parseInt(content);
    }

    private void isReadWriteAndIntegerContent(String filePath) throws Exception {
        assertTrue("Failed readwrite check of " + filePath,
                TargetFileUtils.isReadWriteOnly(filePath, getDevice()));
        String content = getDevice().pullFileContents(filePath).trim();
        // Confirm the content is integer.
        Integer.parseInt(content);
    }

    /**
     * Check each cpu's scaling_cur_freq, scaling_min_freq, scaling_max_freq,
     * scaling_available_frequencies, and time_in_state files.
     */
    @Test
    public void testPerCpuCpufreq() throws Exception {
        String filePath = "/sys/devices/system/cpu/present";
        assertTrue(TargetFileUtils.isReadOnly(filePath, getDevice()));
        String presentCpus = getDevice().pullFileContents(filePath).trim();
        String[] cpuRanges = presentCpus.split(",");
        List<Integer> cpuList = new ArrayList<>();
        Pattern p = Pattern.compile("(\\d+)(-\\d+)?");
        for (String range : cpuRanges) {
            Matcher m = p.matcher(range);
            assertTrue("Malformatted range in " + filePath, m.find());
            int startCpu = Integer.parseInt(m.group(1));
            int endCpu = startCpu;
            if (m.group(2) != null)
                endCpu = Integer.parseInt(m.group(2).substring(1));
            for (int i = startCpu; i <= endCpu; i++) {
                cpuList.add(i);
            }
        }

        String f;
        String content;
        for (int cpu : cpuList) {
            f = String.format("/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu);
            if (getDevice().doesFileExist(f)) {
                isReadOnlyAndIntegerContent(f);
            }

            f = String.format("/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq", cpu);
            if (getDevice().doesFileExist(f)) {
                isReadWriteAndIntegerContent(f);
            }

            f = String.format("/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", cpu);
            if (getDevice().doesFileExist(f)) {
                isReadWriteAndIntegerContent(f);
            }

            f = String.format(
                    "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_available_frequencies", cpu);
            if (getDevice().doesFileExist(f)) {
                assertTrue(TargetFileUtils.isReadOnly(f, getDevice()));
                content = getDevice().pullFileContents(f).trim();
                if (!Strings.isNullOrEmpty(content)) {
                    String[] availFreqs = content.split(" ");
                    for (String freq : availFreqs) {
                        Integer.parseInt(freq);
                    }
                }
            }

            f = String.format("/sys/devices/system/cpu/cpu%d/cpufreq/stats/time_in_state", cpu);
            if (getDevice().doesFileExist(f)) {
                assertTrue(TargetFileUtils.isReadOnly(f, getDevice()));
                content = getDevice().pullFileContents(f).trim();
                if (!Strings.isNullOrEmpty(content)) {
                    for (String line : content.split("\\n")) {
                        String[] values = line.split(" ");
                        for (String value : values) {
                            Integer.parseInt(value);
                        }
                    }
                }
            }
        }
    }

    /* Check /sys/kernel/wakeup_reasons/last_resume_reason. */
    @Test
    public void testLastResumeReason() throws Exception {
        String filePath = "/sys/kernel/wakeup_reasons/last_resume_reason";
        assertTrue(TargetFileUtils.isReadOnly(filePath, getDevice()));
    }

    /* Check the value of /sys/devices/system/cpu/kernel_max. */
    @Test
    public void testKernelMax() throws Exception {
        String filePath = "/sys/devices/system/cpu/kernel_max";
        isReadOnlyAndIntegerContent(filePath);
    }

    private String[] findFiles(String dir, String nameFilter) throws Exception {
        String output = getDevice().executeShellCommand(
                String.format("find %s -name \"%s\" -maxdepth 1 -type l", dir, nameFilter));
        if (output.trim().isEmpty()) {
            return new String[0];
        }
        return output.split("\r?\n");
    }

    /* Check for /sys/class/net/?/mtu. */
    @Test
    public void testNetMTU() throws Exception {
        String[] dirList = findFiles("/sys/class/net", "*");
        for (String entry : dirList) {
            isReadWriteAndIntegerContent(entry + "/mtu");
        }
    }

    /* Check that at least one rtc exists with hctosys = 1. */
    @Test
    public void testRtcHctosys() throws Exception {
        String[] rtcList = findFiles("/sys/class/rtc", "rtc*");
        for (String entry : rtcList) {
            String content = getDevice().pullFileContents(entry + "/hctosys");
            if (Strings.isNullOrEmpty(content)) {
                continue;
            }
            try {
                int hctosys = Integer.parseInt(content.trim());
                if (hctosys == 1) {
                    return;
                }
            } catch (NumberFormatException e) {
                continue;
            }
        }
        fail("No RTC with hctosys=1 present");
    }

    /* Check that locking and unlocking a wake lock works.. */
    @Test
    public void testWakeLock() throws Exception {
        String wakeLockPath = "/sys/power/wake_lock";
        String wakeUnLockPath = "/sys/power/wake_unlock";
        String lockName = "KernelApiSysfsTestWakeLock" + UUID.randomUUID().toString();

        // Enable wake lock
        getDevice().executeShellCommand(String.format("echo %s > %s", lockName, wakeLockPath));

        // Confirm wake lock is enabled
        String results = getDevice().pullFileContents(wakeLockPath).trim();
        HashSet<String> activeSources = new HashSet<>(Arrays.asList(results.split(" ")));
        assertTrue(String.format("active wake lock not reported in %s", wakeLockPath),
                activeSources.contains(lockName));

        // Disable wake lock
        getDevice().executeShellCommand(String.format("echo %s > %s", lockName, wakeUnLockPath));

        // Confirm wake lock is no longer enabled
        results = getDevice().pullFileContents(wakeLockPath).trim();
        activeSources = new HashSet<>(Arrays.asList(results.split(" ")));
        assertTrue(String.format("inactive wake lock reported in %s", wakeLockPath),
                !activeSources.contains(lockName));

        results = getDevice().pullFileContents(wakeUnLockPath).trim();
        activeSources = new HashSet<>(Arrays.asList(results.split(" ")));
        assertTrue(String.format("inactive wake lock not reported in %s", wakeUnLockPath),
                activeSources.contains(lockName));
    }

    @Test
    public void testWakeupCount() throws Exception {
        String filePath = "/sys/power/wakeup_count";
        assertTrue("Failed readwrite check of " + filePath,
                TargetFileUtils.isReadWriteOnly(filePath, getDevice()));
    }

    /* /sys/power/state controls the system sleep states. */
    @Test
    public void testSysPowerState() throws Exception {
        String filePath = "/sys/power/state";
        assertTrue("Failed readwrite check of " + filePath,
                TargetFileUtils.isReadWriteOnly(filePath, getDevice()));
        String content = getDevice().pullFileContents(filePath).trim();
        HashSet<String> allowedStates =
                new HashSet<String>(Arrays.asList("freeze", "mem", "disk", "standby"));
        for (String state : content.split(" ")) {
            assertTrue(String.format("Invalid system power state: '%s'", state),
                    allowedStates.contains(state));
        }
    }
}
