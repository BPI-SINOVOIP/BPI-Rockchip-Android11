/**
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

package android.security.cts;

import android.platform.test.annotations.SecurityTest;
import org.junit.Test;
import org.junit.runner.RunWith;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import static org.junit.Assert.*;

@RunWith(DeviceJUnit4ClassRunner.class)
public class Poc18_06 extends SecurityTestCase {

    /**
     * CVE-2018-5884
     */
    @Test
    @SecurityTest(minPatchLevel = "2018-06")
    public void testPocCVE_2018_5884() throws Exception {
        String wfd_service = AdbUtils.runCommandLine(
                "pm list package com.qualcomm.wfd.service", getDevice());
        if (wfd_service.contains("com.qualcomm.wfd.service")) {
            String result = AdbUtils.runCommandLine(
                    "am broadcast -a qualcomm.intent.action.WIFI_DISPLAY_BITRATE --ei format 3 --ei value 32",
            getDevice());
            assertNotMatchesMultiLine("Broadcast completed", result);
        }
    }

    /**
     *  b/73172817
     */
    @Test
    @SecurityTest
    public void testPocCVE_2018_9344() throws Exception {
        AdbUtils.runPocAssertNoCrashes("CVE-2018-9344", getDevice(),
                "android\\.hardware\\.cas@\\d+?\\.\\d+?-service");
    }
}
