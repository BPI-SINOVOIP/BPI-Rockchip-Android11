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
public class Poc18_02 extends SecurityTestCase {

    /**
     * b/68953950
     */
    @Test
    @SecurityTest(minPatchLevel = "2018-02")
    public void testPocCVE_2017_13232() throws Exception {
        AdbUtils.runCommandLine("logcat -c" , getDevice());
        AdbUtils.runPocNoOutput("CVE-2017-13232", getDevice(), 60);
        String logcatOutput = AdbUtils.runCommandLine("logcat -d", getDevice());
        assertNotMatchesMultiLine("APM_AudioPolicyManager: getOutputForAttr\\(\\) " +
                                  "invalid attributes: usage=.{1,15} content=.{1,15} " +
                                  "flags=.{1,15} tags=\\[A{256,}\\]", logcatOutput);
    }

    /**
     *  b/65853158
     */
    @Test
    @SecurityTest(minPatchLevel = "2018-02")
    public void testPocCVE_2017_13273() throws Exception {
        AdbUtils.runCommandLine("dmesg -c" ,getDevice());
        AdbUtils.runCommandLine("setenforce 0",getDevice());
        if(containsDriver(getDevice(), "/dev/xt_qtaguid") &&
           containsDriver(getDevice(), "/proc/net/xt_qtaguid/ctrl")) {
            AdbUtils.runPoc("CVE-2017-13273", getDevice(), 60);
            String dmesgOut = AdbUtils.runCommandLine("cat /sys/fs/pstore/console-ramoops",
                              getDevice());
            assertNotMatchesMultiLine("CVE-2017-132736 Tainted:" + "[\\s\\n\\S]*" +
                 "Kernel panic - not syncing: Fatal exception in interrupt", dmesgOut);
        }
        AdbUtils.runCommandLine("setenforce 1",getDevice());
    }
}
