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
package com.android.tradefed.presubmit;

import static org.junit.Assert.assertEquals;

import android.platform.test.annotations.RequiresDevice;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.TestDeviceState;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.util.RunUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** A set of tests that are meant to test Tradefed transition of states. */
@RequiresDevice
@RunWith(DeviceJUnit4ClassRunner.class)
public class DeviceStateTransitionFuncTests extends BaseHostJUnit4Test {

    @Before
    public void setUp() throws Exception {
        // Reboot to make sure device is in standard state.
        getDevice().reboot();
    }

    @Test
    public void testFastbootTransition() throws DeviceNotAvailableException {
        assertEquals(TestDeviceState.ONLINE, getDevice().getDeviceState());
        getDevice().rebootIntoBootloader();
        assertEquals(TestDeviceState.FASTBOOT, getDevice().getDeviceState());
        // Reboot out of fastboot
        getDevice().reboot();
        assertEquals(TestDeviceState.ONLINE, getDevice().getDeviceState());
        // Give it some times in case we have a FastbootMonitor bug which re-update the device too
        // late.
        RunUtil.getDefault().sleep(30000);
        assertEquals(TestDeviceState.ONLINE, getDevice().getDeviceState());
    }
}
