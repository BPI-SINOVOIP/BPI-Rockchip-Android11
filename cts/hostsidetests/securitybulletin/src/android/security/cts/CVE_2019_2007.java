/*
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
import org.junit.Test;
import org.junit.runner.RunWith;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

@RunWith(DeviceJUnit4ClassRunner.class)
public class CVE_2019_2007 extends SecurityTestCase {

    /**
     *  b/120789744
     *  Vulnerability Behaviour: EXIT_VULNERABLE (113)
     */
    @SecurityTest(minPatchLevel = "2019-03")
    @Test
    public void testPocCVE_2019_2007() throws Exception {
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2019-2007", null, getDevice());
    }
}
