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
public class Poc17_02 extends SecurityTestCase {
    /**
     *  b/32799236
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-02")
    public void testPocCVE_2017_0426() throws Exception {
        AdbUtils.runCommandLine("logcat -c", getDevice());
        AdbUtils.runPoc("CVE-2017-0426", getDevice(), 60);
        String logcatOut = AdbUtils.runCommandLine("logcat -d", getDevice());
        assertNotMatchesMultiLine("Bugreports file in wrong path", logcatOut);
    }

    /**
     *  b/32706020
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-02")
    public void testPocCVE_2017_0415() throws Exception {
        AdbUtils.runPocAssertNoCrashes("CVE-2017-0415", getDevice(), "mediaserver");
    }

    /**
     *  b/31799863
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-02")
    public void testPocCVE_2016_8482() throws Exception {
        if(containsDriver(getDevice(), "/dev/nvmap")) {
            AdbUtils.runPoc("CVE-2016-8482", getDevice(), 60);
        }
    }
}
