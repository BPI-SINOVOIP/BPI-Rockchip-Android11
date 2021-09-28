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
public class Poc17_01 extends SecurityTestCase {

    //Criticals
    /**
     *  b/31797770
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-01")
    public void testPocCVE_2016_8425() throws Exception {
        if(containsDriver(getDevice(), "/dev/nvhost-vic")) {
            AdbUtils.runPoc("CVE-2016-8425", getDevice(), 60);
        }
    }

    /**
     *  b/31799206
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-01")
    public void testPocCVE_2016_8426() throws Exception {
        if(containsDriver(getDevice(), "/dev/nvhost-gpu")) {
            AdbUtils.runPoc("CVE-2016-8426", getDevice(), 60);
        }
    }

    /**
     *  b/31799885
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-01")
    public void testPocCVE_2016_8427() throws Exception {
        if(containsDriver(getDevice(), "/dev/nvhost-gpu") ||
              containsDriver(getDevice(), "/dev/nvhost-dbg-gpu")) {
            AdbUtils.runPoc("CVE-2016-8427", getDevice(), 60);
        }
    }

    /**
     *  b/31993456
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-01")
    public void testPocCVE_2016_8428() throws Exception {
        if(containsDriver(getDevice(), "/dev/nvmap")) {
            AdbUtils.runPoc("CVE-2016-8428", getDevice(), 60);
        }
    }

    /**
     *  b/32160775
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-01")
    public void testPocCVE_2016_8429() throws Exception {
        if(containsDriver(getDevice(), "/dev/nvmap")) {
            AdbUtils.runPoc("CVE-2016-8429", getDevice(), 60);
        }
    }

    /**
     *  b/32225180
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-01")
    public void testPocCVE_2016_8430() throws Exception {
        if(containsDriver(getDevice(), "/dev/nvhost-vic")) {
            AdbUtils.runPoc("CVE-2016-8430", getDevice(), 60);
        }
    }

   /**
     *  b/32402179
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-01")
    public void testPocCVE_2016_8431() throws Exception {
        if(containsDriver(getDevice(), "/dev/dri/renderD129")) {
            AdbUtils.runPoc("CVE-2016-8431", getDevice(), 60);
        }
    }

    /**
     *  b/32447738
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-01")
    public void testPocCVE_2016_8432() throws Exception {
        if(containsDriver(getDevice(), "/dev/dri/renderD129")) {
            AdbUtils.runPoc("CVE-2016-8432", getDevice(), 60);
        }
    }

    /**
     *  b/32125137
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-01")
    public void testPocCVE_2016_8434() throws Exception {
        if(containsDriver(getDevice(), "/dev/kgsl-3d0")) {
            // This poc is very verbose so we ignore the output to avoid using a lot of memory.
            AdbUtils.runPocNoOutput("CVE-2016-8434", getDevice(), 60);
        }
    }

    /**
     *  b/31668540
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-01")
    public void testPocCVE_2016_8460() throws Exception {
        if(containsDriver(getDevice(), "/dev/nvmap")) {
            String result = AdbUtils.runPoc("CVE-2016-8460", getDevice(), 60);
            assertTrue(!result.equals("Vulnerable"));
        }
    }

    /**
     *  b/32255299
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-01")
    public void testPocCVE_2017_0386() throws Exception {
        AdbUtils.runPocAssertExitStatusNotVulnerable("CVE-2017-0386", getDevice(), 60);
    }
}
