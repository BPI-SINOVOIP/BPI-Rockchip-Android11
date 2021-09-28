/*
 * Copyright (C) 2016 The Android Open Source Project
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

import static android.appsecurity.cts.Utils.waitForBootCompleted;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.platform.test.annotations.RequiresDevice;

import com.android.ddmlib.Log;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Set of tests that verify behavior of direct boot, if supported.
 * <p>
 * Note that these tests drive PIN setup manually instead of relying on device
 * administrators, which are not supported by all devices.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class DirectBootHostTest extends BaseHostJUnit4Test {
    private static final String TAG = "DirectBootHostTest";

    private static final String PKG = "com.android.cts.encryptionapp";
    private static final String CLASS = PKG + ".EncryptionAppTest";
    private static final String APK = "CtsEncryptionApp.apk";

    private static final String OTHER_APK = "CtsSplitApp.apk";
    private static final String OTHER_PKG = "com.android.cts.splitapp";

    private static final String MODE_NATIVE = "native";
    private static final String MODE_EMULATED = "emulated";
    private static final String MODE_NONE = "none";

    private static final String FEATURE_DEVICE_ADMIN = "feature:android.software.device_admin";
    private static final String FEATURE_SECURE_LOCK_SCREEN =
            "feature:android.software.secure_lock_screen";
    private static final String FEATURE_AUTOMOTIVE = "feature:android.hardware.type.automotive";

    private static final long SHUTDOWN_TIME_MS = 30 * 1000;

    @Before
    public void setUp() throws Exception {
        Utils.prepareSingleUser(getDevice());
        assertNotNull(getAbi());
        assertNotNull(getBuild());

        getDevice().uninstallPackage(PKG);
        getDevice().uninstallPackage(OTHER_PKG);
    }

    @After
    public void tearDown() throws Exception {
        getDevice().uninstallPackage(PKG);
        getDevice().uninstallPackage(OTHER_PKG);
    }

    /**
     * Automotive devices MUST support native FBE.
     */
    @Test
    public void testAutomotiveNativeFbe() throws Exception {
        if (!isSupportedDevice()) {
            Log.v(TAG, "Device not supported; skipping test");
            return;
        } else if (!isAutomotiveDevice()) {
            Log.v(TAG, "Device not automotive; skipping test");
            return;
        }

        assertTrue("Automotive devices must support native FBE",
            MODE_NATIVE.equals(getFbeMode()));
    }

    /**
     * If device has native FBE, verify lifecycle.
     */
    @Test
    public void testDirectBootNative() throws Exception {
        if (!isSupportedDevice()) {
            Log.v(TAG, "Device not supported; skipping test");
            return;
        } else if (!MODE_NATIVE.equals(getFbeMode())) {
            Log.v(TAG, "Device doesn't have native FBE; skipping test");
            return;
        }

        doDirectBootTest(MODE_NATIVE);
    }

    /**
     * If device doesn't have native FBE, enable emulation and verify lifecycle.
     */
    @Test
    @RequiresDevice
    public void testDirectBootEmulated() throws Exception {
        if (!isSupportedDevice()) {
            Log.v(TAG, "Device not supported; skipping test");
            return;
        } else if (MODE_NATIVE.equals(getFbeMode())) {
            Log.v(TAG, "Device has native FBE; skipping test");
            return;
        }

        doDirectBootTest(MODE_EMULATED);
    }

    /**
     * If device doesn't have native FBE, verify normal lifecycle.
     */
    @Test
    public void testDirectBootNone() throws Exception {
        if (!isSupportedDevice()) {
            Log.v(TAG, "Device not supported; skipping test");
            return;
        } else if (MODE_NATIVE.equals(getFbeMode())) {
            Log.v(TAG, "Device has native FBE; skipping test");
            return;
        }

        doDirectBootTest(MODE_NONE);
    }

    public void doDirectBootTest(String mode) throws Exception {
        boolean doTest = true;
        try {
            // Set up test app and secure lock screens
            new InstallMultiple().addFile(APK).run();
            new InstallMultiple().addFile(OTHER_APK).run();

            // To receive boot broadcasts, kick our other app out of stopped state
            getDevice().executeShellCommand("am start -a android.intent.action.MAIN"
                    + " --user current"
                    + " -c android.intent.category.LAUNCHER com.android.cts.splitapp/.MyActivity");

            // Give enough time for PackageManager to persist stopped state
            Thread.sleep(15000);

            runDeviceTestsAsCurrentUser(PKG, CLASS, "testSetUp");

            // Give enough time for vold to update keys
            Thread.sleep(15000);

            // Reboot system into known state with keys ejected
            if (MODE_EMULATED.equals(mode)) {
                final String res = getDevice().executeShellCommand("sm set-emulate-fbe true");
                if (res != null && res.contains("Emulation not supported")) {
                    doTest = false;
                }
                getDevice().waitForDeviceNotAvailable(SHUTDOWN_TIME_MS);
                getDevice().waitForDeviceOnline(120000);
            } else {
                getDevice().rebootUntilOnline();
            }
            waitForBootCompleted(getDevice());

            if (doTest) {
                if (MODE_NONE.equals(mode)) {
                    runDeviceTestsAsCurrentUser(PKG, CLASS, "testVerifyUnlockedAndDismiss");
                } else {
                    runDeviceTestsAsCurrentUser(PKG, CLASS, "testVerifyLockedAndDismiss");
                }
            }

        } finally {
            try {
                // Remove secure lock screens and tear down test app
                runDeviceTestsAsCurrentUser(PKG, CLASS, "testTearDown");
            } finally {
                getDevice().uninstallPackage(PKG);

                // Get ourselves back into a known-good state
                if (MODE_EMULATED.equals(mode)) {
                    getDevice().executeShellCommand("sm set-emulate-fbe false");
                    getDevice().waitForDeviceNotAvailable(SHUTDOWN_TIME_MS);
                    getDevice().waitForDeviceOnline();
                } else {
                    getDevice().rebootUntilOnline();
                }
                getDevice().waitForDeviceAvailable();
            }
        }
    }

    private void runDeviceTestsAsCurrentUser(
            String packageName, String testClassName, String testMethodName)
                throws DeviceNotAvailableException {
        Utils.runDeviceTestsAsCurrentUser(getDevice(), packageName, testClassName, testMethodName);
    }

    private String getFbeMode() throws Exception {
        return getDevice().executeShellCommand("sm get-fbe-mode").trim();
    }

    private boolean isSupportedDevice() throws Exception {
        return getDevice().hasFeature(FEATURE_DEVICE_ADMIN)
                && getDevice().hasFeature(FEATURE_SECURE_LOCK_SCREEN);
    }

    private boolean isAutomotiveDevice() throws Exception {
        return getDevice().hasFeature(FEATURE_AUTOMOTIVE);
    }

    private class InstallMultiple extends BaseInstallMultiple<InstallMultiple> {
        public InstallMultiple() {
            super(getDevice(), getBuild(), getAbi());
            addArg("--force-queryable");
        }
    }
}
