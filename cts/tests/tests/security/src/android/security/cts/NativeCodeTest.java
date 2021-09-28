/*
 * Copyright (C) 2013 The Android Open Source Project
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

import junit.framework.TestCase;

@SecurityTest
public class NativeCodeTest extends TestCase {

    static {
        System.loadLibrary("ctssecurity_jni");
    }

    @SecurityTest
    public void testSysVipc() throws Exception {
        assertTrue("Android does not support Sys V IPC, it must "
                   + "be removed from the kernel. In the kernel config: "
                   + "Change \"CONFIG_SYSVIPC=y\" to \"# CONFIG_SYSVIPC is not set\" "
                   + "and rebuild.",
                   doSysVipcTest());
    }

    /**
     * Test that SysV IPC has been removed from the kernel.
     *
     * Returns true if SysV IPC has been removed.
     *
     * System V IPCs are not compliant with Android's application lifecycle because allocated
     * resources are not freed by the low memory killer. This lead to global kernel resource leakage.
     *
     * For example, there is no way to automatically release a SysV semaphore
     * allocated in the kernel when:
     * - a buggy or malicious process exits
     * - a non-buggy and non-malicious process crashes or is explicitly killed.
     *
     * Killing processes automatically to make room for new ones is an
     * important part of Android's application lifecycle implementation. This means
     * that, even assuming only non-buggy and non-malicious code, it is very likely
     * that over time, the kernel global tables used to implement SysV IPCs will fill
     * up.
     */
    private static native boolean doSysVipcTest();

}
