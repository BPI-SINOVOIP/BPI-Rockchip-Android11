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

import java.util.Arrays;
import java.util.concurrent.Callable;

import android.platform.test.annotations.SecurityTest;
import org.junit.Test;
import org.junit.runner.RunWith;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import static org.junit.Assert.*;

@RunWith(DeviceJUnit4ClassRunner.class)
public class Poc17_05 extends SecurityTestCase {

    /*
     * CVE-2016-5862
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-05")
    public void testPocCVE_2016_5862() throws Exception {
        if (containsDriver(getDevice(), "/dev/snd/controlC0")) {
            AdbUtils.runPocNoOutput("CVE-2016-5862",getDevice(), 60);
        }
    }

    /**
     * CVE-2016-5867
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-05")
    public void testPocCVE_2016_5867() throws Exception {
        if (containsDriver(getDevice(), "/dev/snd/controlC0")) {
            AdbUtils.runPocAssertExitStatusNotVulnerable("CVE-2016-5867", getDevice(), 60);
        }
    }
}
