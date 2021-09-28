/**
 * Copyright (C) 2021 The Android Open Source Project
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
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
public class CVE_2021_0439 extends SecurityTestCase {

    /**
     * b/174243830
     * Vulnerability Behaviour: SIGSEGV in system_server
     */
    @SecurityTest(minPatchLevel = "2021-04")
    @Test
    public void testPocCVE_2021_0439() throws Exception {
        String processPatternStrings[] = {"system_server"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2021-0439", null, getDevice(),
                processPatternStrings);
    }
}
