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
import org.junit.Test;
import org.junit.runner.RunWith;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import static org.junit.Assume.assumeFalse;

@RunWith(DeviceJUnit4ClassRunner.class)
public class CVE_2020_0383 extends SecurityTestCase {

    /**
     * b/150160279
     * Vulnerability Behaviour: SIGSEGV in self
     */
    @SecurityTest(minPatchLevel = "2020-09")
    @Test
    public void testPocCVE_2020_0383() throws Exception {
        assumeFalse(moduleIsPlayManaged("com.google.android.media"));
        String inputFiles[] = {"cve_2020_0383.xmf", "cve_2020_0383.info"};
        String binaryName = "CVE-2020-0383";
        AdbUtils.runPocAssertNoCrashesNotVulnerable(binaryName,
                AdbUtils.TMP_PATH + inputFiles[0] + " " + AdbUtils.TMP_PATH + inputFiles[1],
                inputFiles, AdbUtils.TMP_PATH, getDevice());
    }
}
