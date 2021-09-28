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
public class Poc16_07 extends SecurityTestCase {
    /**
     *  b/28740702
     */
    @Test
    @SecurityTest(minPatchLevel = "2016-07")
    public void testPocCVE_2016_3818() throws Exception {
        AdbUtils.runPoc("CVE-2016-3818", getDevice(), 60);
    }

    /**
     *  b/27890802
     */
    @Test
    @SecurityTest(minPatchLevel = "2016-07")
    public void testPocCVE_2016_3746() throws Exception {
        AdbUtils.runPocAssertNoCrashes("CVE-2016-3746", getDevice(), "mediaserver");
    }

    /**
     *  b/28557020
     */
    @Test
    @SecurityTest(minPatchLevel = "2016-07")
    public void testPocCVE_2014_9803() throws Exception {
        AdbUtils.runPocAssertExitStatusNotVulnerable("CVE-2014-9803", getDevice(), 60);
    }

    /**
     * b/27903498
     */
    @Test
    @SecurityTest(minPatchLevel = "2016-07")
    public void testPocCVE_2016_3747() throws Exception {
        AdbUtils.runPocAssertNoCrashes("CVE-2016-3747", getDevice(), "mediaserver");
    }
}
