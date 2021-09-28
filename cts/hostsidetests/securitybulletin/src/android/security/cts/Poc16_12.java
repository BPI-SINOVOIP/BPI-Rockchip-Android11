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
public class Poc16_12 extends SecurityTestCase {

    //Criticals
    /**
     *  b/31796940
     */
    @Test
    @SecurityTest(minPatchLevel = "2016-12")
    public void testPocCVE_2016_8406() throws Exception {
        assertNotKernelPointer(() -> {
            String cmd = "ls /sys/kernel/slab 2>/dev/null | grep nf_conntrack";
            String result =  AdbUtils.runCommandLine(cmd, getDevice());
            String pattern = "nf_conntrack_";
            int index = result.indexOf(pattern);
            if (index == -1) {
                return null;
            }
            return result.substring(index + pattern.length());
        }, getDevice());
    }
}
