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
public class Poc19_11 extends SecurityTestCase {

    /**
     * b/138441919
     */
    @Test
    @SecurityTest(minPatchLevel = "2019-11")
    public void testPocBug_138441919() throws Exception {
        int code = AdbUtils.runProxyAutoConfig("bug_138441919", getDevice());
        assertTrue(code != 139); // 128 + signal 11
    }

    /**
     * b/139806216
     */
    @Test
    @SecurityTest(minPatchLevel = "2019-11")
    public void testPocBug_139806216() throws Exception {
        int code = AdbUtils.runProxyAutoConfig("bug_139806216", getDevice());
        assertTrue(code != 139 && code != 135); // 128 + signal 11, 128 + signal 7
    }
}
