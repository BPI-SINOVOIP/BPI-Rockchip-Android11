/*
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

package android.adb.cts;

import com.android.tradefed.testtype.DeviceTestCase;
import com.android.tradefed.testtype.IDeviceTest;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

public class AdbHostTest extends DeviceTestCase implements IDeviceTest {
    // TODO: there are too many copies of this in CTS!
    public static File copyResourceToTempFile(String resName) throws IOException {
        InputStream is = AdbHostTest.class.getResourceAsStream(resName);
        File tf = File.createTempFile("AdbHostTest", ".tmp");
        FileOutputStream os = new FileOutputStream(tf);
        byte[] buf = new byte[8192];
        int length;
        while ((length = is.read(buf)) != -1) {
            os.write(buf, 0, length);
        }
        os.flush();
        os.close();
        tf.deleteOnExit();
        return tf;
    }

    // *** READ THIS IF YOUR DEVICE IS FAILING THIS TEST ***
    // This test checks to see that Microsoft OS Descriptors are enabled on the adb USB interface,
    // so that users don't have to install a specific driver for their device in order to use adb
    // with it (and OEMs don't have to sign and host that driver).
    //
    // adbd exports Microsoft OS descriptors when setting up its endpoint descriptors, but the
    // kernel requires that the OS descriptor functionality be enabled separately by writing "1" to
    // the gadget's configfs file at /config/usb_gadget/g<N>/os_desc/use (where <N> is probably 1).
    // To pass this test, you need to modify your USB HAL implementation to do this.
    //
    // See https://android.googlesource.com/platform/hardware/google/pixel/+/5c7c6d5edcbe04454eb9a40f947ac0a3045f64eb
    // for the patch that enabled this on Pixel devices.
    public void testHasMsOsDescriptors() throws Exception {
        File check_ms_os_desc = copyResourceToTempFile("/check_ms_os_desc");
        check_ms_os_desc.setExecutable(true);

        String serial = getDevice().getSerialNumber();
        if (serial.startsWith("emulator-")) {
            return;
        }

        if (getDevice().isAdbTcp()) { // adb over WiFi, no point checking it
            return;
        }

        ProcessBuilder pb = new ProcessBuilder(check_ms_os_desc.getAbsolutePath());
        pb.environment().put("ANDROID_SERIAL", serial);
        pb.redirectOutput(ProcessBuilder.Redirect.PIPE);
        pb.redirectErrorStream(true);
        Process p = pb.start();
        int result = p.waitFor();
        BufferedReader br = new BufferedReader(new InputStreamReader(p.getInputStream()));
        String line;
        StringBuilder output = new StringBuilder();
        while ((line = br.readLine()) != null) {
            output.append(line + "\n");
        }
        assertTrue("check_ms_os_desc failed:\n" + output, result == 0);
    }
}
