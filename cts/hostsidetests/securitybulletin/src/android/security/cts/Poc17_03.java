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

import java.util.concurrent.Callable;

import android.platform.test.annotations.SecurityTest;
import org.junit.Test;
import org.junit.runner.RunWith;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import static org.junit.Assert.*;

@RunWith(DeviceJUnit4ClassRunner.class)
public class Poc17_03 extends SecurityTestCase {

    /**
     *  b/31824853
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-03")
    public void testPocCVE_2016_8479() throws Exception {
        if (containsDriver(getDevice(), "/dev/kgsl-3d0")) {
             AdbUtils.runPocNoOutput("CVE-2016-8479", getDevice(), TIMEOUT_NONDETERMINISTIC);
            // CTS begins the next test before device finishes rebooting,
            // sleep to allow time for device to reboot.
            Thread.sleep(70000);
        }
    }

    /**
     *  b/33940449
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-03")
    public void testPocCVE_2017_0508() throws Exception {
        if (containsDriver(getDevice(), "/dev/ion") &&
            containsDriver(getDevice(), "/dev/dri/renderD129")) {
            AdbUtils.runPocNoOutput("CVE-2017-0508", getDevice(), 30);
            // CTS begins the next test before device finishes rebooting,
            // sleep to allow time for device to reboot.
            Thread.sleep(60000);
        }
    }

    /**
     *  b/33899363
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-03")
    public void testPocCVE_2017_0333() throws Exception {
        if (containsDriver(getDevice(), "/dev/dri/renderD128")) {
            AdbUtils.runPocNoOutput("CVE-2017-0333", getDevice(), 30);
            // Device takes up to 30 seconds to crash after ioctl call
            Thread.sleep(30000);
        }
    }

    /**
     *  b/33245849
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-03")
    public void testPocCVE_2017_0334() throws Exception {
        if (containsDriver(getDevice(), "/dev/dri/renderD129")) {
            String out = AdbUtils.runPoc("CVE-2017-0334", getDevice());
            // info leak sample
            // "leaked ptr is 0xffffffc038ed1980"
            String[] lines = out.split("\n");
            String pattern = "Leaked ptr is 0x";
            assertNotKernelPointer(new Callable<String>() {
                int index = 0;
                @Override
                public String call() {
                    for (; index < lines.length; index++) {
                        String line = lines[index];
                        int index = line.indexOf(pattern);
                        if (index == -1) {
                            continue;
                        }
                        return line.substring(index + pattern.length());
                    }
                    return null;
                }
            }, null);
        }
    }

    /**
     * b/32707507
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-03")
    public void testPocCVE_2017_0479() throws Exception {
        AdbUtils.runPocAssertNoCrashes("CVE-2017-0479", getDevice(), "audioserver");
    }

    /*
     *  b/33178389
     */
    @Test
    @SecurityTest(minPatchLevel = "2017-03")
    public void testPocCVE_2017_0490() throws Exception {
        String bootCountBefore =
                AdbUtils.runCommandLine("settings get global boot_count", getDevice());
        AdbUtils.runCommandLine("service call wifi 43 s16 content://settings/global/boot_count s16 "
                + "\"application/x-wifi-config\"",
                getDevice());
        String bootCountAfter =
                AdbUtils.runCommandLine("settings get global boot_count", getDevice());
        // Poc nukes the boot_count setting, reboot to restore it to a sane value
        getDevice().reboot();
        updateKernelStartTime();
        assertEquals(bootCountBefore, bootCountAfter);
    }

}
