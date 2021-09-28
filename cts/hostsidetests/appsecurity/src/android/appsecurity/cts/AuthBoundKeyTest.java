/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.appsecurity.cts;

import static org.junit.Assume.assumeTrue;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import java.io.FileNotFoundException;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests that verify some of the behaviors of Auth-Bound keys
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class AuthBoundKeyTest extends BaseAppSecurityTest {
    static final String PKG = "com.android.cts.authboundkeyapp";
    static final String CLASS = PKG + ".AuthBoundKeyAppTest";
    static final String APK = "AuthBoundKeyApp.apk";

    @Before
    public void setUp() throws Exception {
        Utils.prepareSingleUser(getDevice());
        getDevice().uninstallPackage(PKG);
    }

    @After
    public void tearDown() throws Exception {
        getDevice().uninstallPackage(PKG);
    }

    @Test
    public void useInvalidatedAuthBoundKey()
            throws DeviceNotAvailableException, FileNotFoundException {
        assumeTrue("Device does not support secure lock",
                   getDevice().hasFeature("android.software.secure_lock_screen"));
        new InstallMultiple().addFile(APK).run();
        getDevice().executeShellCommand("cmd lock_settings set-pin 1234");
        runDeviceTests(PKG, CLASS, "testGenerateAuthBoundKey");
        getDevice().executeShellCommand("cmd lock_settings clear --old 1234");
        runDeviceTests(PKG, CLASS, "testUseKey");
        getDevice().executeShellCommand("cmd lock_settings set-pin 12345");
        getDevice().executeShellCommand("input keyevent 26");  // Screen on
        getDevice().executeShellCommand("input keyevent 82");  // Bring up lock screen
        getDevice().executeShellCommand("input text 12345");   // Input password
        getDevice().executeShellCommand("input keyevent 66");  // Submit input
        // try:finally clause to ensure the pin is wiped before testing continues
        try {
            runDeviceTests(PKG, CLASS, "testUseKey");
        } finally {
            getDevice().executeShellCommand("cmd lock_settings clear --old 12345");
        }
    }
}
