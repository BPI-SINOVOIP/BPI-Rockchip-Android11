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

package android.os.cts;


import static android.os.PowerManagerInternalProto.Wakefulness.WAKEFULNESS_ASLEEP;
import static android.os.PowerManagerInternalProto.Wakefulness.WAKEFULNESS_AWAKE;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assume.assumeTrue;

import android.os.PowerManagerInternalProto.Wakefulness;

import com.android.compatibility.common.util.ProtoUtils;
import com.android.server.power.PowerManagerServiceDumpProto;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
public class QuiescentBootTests extends BaseHostJUnit4Test {
    private static final int REBOOT_TIMEOUT = 60000;

    private static final String FEATURE_LEANBACK_ONLY = "android.software.leanback_only";
    private static final String CMD_DUMPSYS_POWER = "dumpsys power --proto";
    private static final String CMD_INPUT_WAKEUP = "input keyevent WAKEUP";
    private static final String CMD_INPUT_POWER = "input keyevent POWER";

    // A reference to the device under test, which gives us a handle to run commands.
    private ITestDevice mDevice;

    @Before
    public synchronized void setUp() throws Exception {
        mDevice = getDevice();
        assumeTrue("Test only applicable to TVs.", hasDeviceFeature(FEATURE_LEANBACK_ONLY));
    }

    @After
    public void tearDown() throws Exception {
        if (hasDeviceFeature(FEATURE_LEANBACK_ONLY)) {
            mDevice.executeShellCommand(CMD_INPUT_WAKEUP);
        }
    }

    @Test
    public void testQuiescentBoot_sysPropSet_asleep() throws Exception {
        mDevice.executeAdbCommand("reboot", "quiescent");
        mDevice.waitForBootComplete(REBOOT_TIMEOUT);
        assertEquals("Quiescent system property (ro.boot.quiescent) not set.",
                "1", mDevice.getProperty("ro.boot.quiescent"));
        assertEquals("Expected to boot into sleep state.", WAKEFULNESS_ASLEEP, getWakefulness());
    }

    @Test
    public void testQuiescentBoot_wakesUpWithPowerButton() throws Exception {
        mDevice.executeAdbCommand("reboot", "quiescent");
        mDevice.waitForBootComplete(REBOOT_TIMEOUT);
        mDevice.executeShellCommand(CMD_INPUT_POWER);
        assertEquals("Expected to wake up when pressing the power button.",
                WAKEFULNESS_AWAKE, getWakefulness());
    }

    @Test
    public void testQuiescentBoot_asleepAfterQuiescentReboot() throws Exception {
        mDevice.executeAdbCommand("reboot", "quiescent");
        mDevice.waitForBootComplete(REBOOT_TIMEOUT);

        mDevice.executeAdbCommand("reboot", "quiescent");
        mDevice.waitForBootComplete(REBOOT_TIMEOUT);

        assertEquals("Quiescent system property (ro.boot.quiescent) not set.",
                "1", mDevice.getProperty("ro.boot.quiescent"));
        assertEquals("Expected to boot into sleep state.", WAKEFULNESS_ASLEEP, getWakefulness());
    }

    @Test
    public void testQuiescentBoot_awakeAfterReboot() throws Exception {
        mDevice.executeAdbCommand("reboot", "quiescent");
        mDevice.waitForBootComplete(REBOOT_TIMEOUT);

        mDevice.executeAdbCommand("reboot");
        mDevice.waitForBootComplete(REBOOT_TIMEOUT);

        assertNull("Quiescent system property (ro.boot.quiescent) unexpectedly set.",
                mDevice.getProperty("ro.boot.quiescent"));
        assertEquals("Expected to boot in awake state.", WAKEFULNESS_AWAKE, getWakefulness());
    }

    private Wakefulness getWakefulness() throws Exception {
        return ((PowerManagerServiceDumpProto) ProtoUtils.getProto(getDevice(),
                PowerManagerServiceDumpProto.parser(),
                CMD_DUMPSYS_POWER)).getWakefulness();
    }
}
