/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.game.qualification.test;

import static org.junit.Assert.assertTrue;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
public class DeviceConfigurationTest extends BaseHostJUnit4Test {
    /**
     * CONFIG_HZ must be >= 250 to ensure system responsiveness.
     */
    @Test
    public void testConfigHzHighEnough()
        throws DeviceNotAvailableException {

        /* Insist on the kernel tick rate being at least 250Hz. */

        String output = getDevice().executeShellCommand("gzip -dc /proc/config.gz | grep CONFIG_HZ=");
        assertTrue(output.contains("CONFIG_HZ="));
        int tickRate = Integer.parseInt(output.split("\n")[0].split("=")[1]);
        assertTrue("Kernel tick rate is " + tickRate + " but certification requires at least 250", tickRate >= 250);
    }
}
