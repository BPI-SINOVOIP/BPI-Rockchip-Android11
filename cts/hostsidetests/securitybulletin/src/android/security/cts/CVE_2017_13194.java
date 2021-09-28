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

import static org.junit.Assert.*;
import static org.junit.Assume.*;

@RunWith(DeviceJUnit4ClassRunner.class)
public class CVE_2017_13194 extends SecurityTestCase {

    /**
     * b/64710201
     * Vulnerability Behaviour: SIGSEGV in media.codec
     */
    @SecurityTest(minPatchLevel = "2018-01")
    @Test
    public void testPocCVE_2017_13194() throws Exception {
        assumeFalse(moduleIsPlayManaged("com.google.android.media.swcodec"));
        pocPusher.only64();
        String processPatternStrings[] = {"media\\.codec", "omx@\\d+?\\.\\d+?-service"};
        AdbUtils.runPocAssertNoCrashesNotVulnerable("CVE-2017-13194", null, getDevice(),
                processPatternStrings);
    }
}
