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
public class Poc19_05 extends SecurityTestCase {

    /**
     * b/129556464
     */
    @Test
    @SecurityTest(minPatchLevel = "2019-05")
    public void testPocCVE_2019_2052() throws Exception {
        int code = AdbUtils.runProxyAutoConfig("CVE-2019-2052", getDevice());
        assertTrue(code != 139); // 128 + signal 11
    }

    /**
     * b/129556111
     */
    @Test
    @SecurityTest(minPatchLevel = "2019-05")
    public void testPocCVE_2019_2045() throws Exception {
        int code = AdbUtils.runProxyAutoConfig("CVE-2019-2045", getDevice());
        assertTrue(code != 139); // 128 + signal 11
    }

    /*
     * b/129556718
     */
    @Test
    @SecurityTest(minPatchLevel = "2019-05")
    public void testPocCVE_2019_2047() throws Exception {
        int code = AdbUtils.runProxyAutoConfig("CVE-2019-2047", getDevice());
        assertTrue(code != 139); // 128 + signal 11
    }

    /**
     * CVE-2019-2257
     */
    @Test
    @SecurityTest(minPatchLevel = "2019-05")
    public void testPocCVE_2019_2257() throws Exception {
        String result = AdbUtils.runCommandLine(
                                "dumpsys package com.qualcomm.qti.telephonyservice", getDevice());
        assertFalse(result.contains(
                            "permission com.qualcomm.permission.USE_QTI_TELEPHONY_SERVICE"));
    }

    /**
     * b/117555811
     */
    @Test
    @SecurityTest(minPatchLevel = "2019-05")
    public void testPocCVE_2019_2051() throws Exception {
        int code = AdbUtils.runProxyAutoConfig("CVE-2019-2051", getDevice());
        assertTrue(code != 139); // 128 + signal 11
    }
}
