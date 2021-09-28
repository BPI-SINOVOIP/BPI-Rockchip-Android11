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
import com.android.compatibility.common.util.CrashUtils;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
public class CVE_2019_2014 extends SecurityTestCase {

    /**
     * b/120499324
     * Vulnerability Behaviour: SIGABRT in self
     */
    @SecurityTest(minPatchLevel = "2019-03")
    @Test
    public void testPocCVE_2019_2014() throws Exception {
        pocPusher.only64();
        String binaryName = "CVE-2019-2014";
        String signals[] = {CrashUtils.SIGSEGV, CrashUtils.SIGBUS, CrashUtils.SIGABRT};
        AdbUtils.pocConfig testConfig = new AdbUtils.pocConfig(binaryName, getDevice());
        testConfig.config = new CrashUtils.Config().setProcessPatterns(binaryName);
        testConfig.config.setSignals(signals);
        AdbUtils.runPocAssertNoCrashesNotVulnerable(testConfig);
    }
}
