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
public class Poc20_03 extends SecurityTestCase {

    /**
     * b/147882143
     * b/152874234
     */
    @Test
    @SecurityTest(minPatchLevel = "2020-03")
    public void testPocCVE_2020_0069() throws Exception {
        if(containsDriver(getDevice(), "/dev/mtk_cmdq") ||
           containsDriver(getDevice(), "/proc/mtk_cmdq") ||
           containsDriver(getDevice(), "/dev/mtk_mdp")) {
            AdbUtils.runPocAssertExitStatusNotVulnerable("CVE-2020-0069",
                                                         getDevice(), 300);
        }
    }
}
