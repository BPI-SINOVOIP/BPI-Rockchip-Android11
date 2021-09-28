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
import com.android.compatibility.common.util.CrashUtils;
import static org.junit.Assume.assumeFalse;

@RunWith(DeviceJUnit4ClassRunner.class)
public class CVE_2016_2182 extends SecurityTestCase {

    /**
     * b/32096880
     * Vulnerability Behaviour: SIGSEGV in self
     */
    @SecurityTest(minPatchLevel = "2017-03")
    @Test
    public void testPocCVE_2016_2182() throws Exception {
        assumeFalse(moduleIsPlayManaged("com.google.android.conscrypt"));
        String binaryName = "CVE-2016-2182";
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig(binaryName, getDevice());
        testConfig.config = new CrashUtils.Config().setProcessPatterns(binaryName);
        testConfig.config.checkMinAddress(false);
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }
}
