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

package android.appsecurity.cts;

import static android.appsecurity.cts.Utils.waitForBootCompleted;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Set of tests that verify app data isolation works.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class AppDataIsolationTests extends BaseAppSecurityTest {

    private static final String APPA_APK = "CtsAppDataIsolationAppA.apk";
    private static final String APP_SHARED_A_APK = "CtsAppDataIsolationAppSharedA.apk";
    private static final String APP_DIRECT_BOOT_A_APK = "CtsAppDataIsolationAppDirectBootA.apk";
    private static final String APPA_PKG = "com.android.cts.appdataisolation.appa";
    private static final String APPA_CLASS =
            "com.android.cts.appdataisolation.appa.AppATests";
    private static final String APPA_METHOD_CREATE_CE_DE_DATA = "testCreateCeDeAppData";
    private static final String APPA_METHOD_CHECK_CE_DATA_EXISTS = "testAppACeDataExists";
    private static final String APPA_METHOD_CHECK_CE_DATA_DOES_NOT_EXIST =
            "testAppACeDataDoesNotExist";
    private static final String APPA_METHOD_CHECK_DE_DATA_EXISTS = "testAppADeDataExists";
    private static final String APPA_METHOD_CHECK_DE_DATA_DOES_NOT_EXIST =
            "testAppADeDataDoesNotExist";
    private static final String APPA_METHOD_CHECK_CUR_PROFILE_ACCESSIBLE =
            "testAppACurProfileDataAccessible";
    private static final String APPA_METHOD_CHECK_REF_PROFILE_NOT_ACCESSIBLE =
            "testAppARefProfileDataNotAccessible";
    private static final String APPA_METHOD_UNLOCK_DEVICE_AND_VERIFY_CE_DE_EXTERNAL_EXIST =
            "testAppAUnlockDeviceAndVerifyCeDeExternalDataExist";
    private static final String APPA_METHOD_CANNOT_ACCESS_APPB_DIR = "testCannotAccessAppBDataDir";

    private static final String APPA_METHOD_TEST_UNLOCK_DEVICE =
            "testUnlockDevice";

    private static final String APPB_APK = "CtsAppDataIsolationAppB.apk";
    private static final String APP_SHARED_B_APK = "CtsAppDataIsolationAppSharedB.apk";
    private static final String APPB_PKG = "com.android.cts.appdataisolation.appb";
    private static final String APPB_CLASS =
            "com.android.cts.appdataisolation.appb.AppBTests";
    private static final String APPB_METHOD_CAN_NOT_ACCESS_APPA_DIR = "testCanNotAccessAppADataDir";
    private static final String APPB_METHOD_CAN_ACCESS_APPA_DIR = "testCanAccessAppADataDir";

    private static final String FBE_MODE_NATIVE = "native";
    private static final String FBE_MODE_EMULATED = "emulated";

    private static final String CHECK_IF_FUSE_DATA_ISOLATION_IS_ENABLED_COMMANDLINE =
            "getprop persist.sys.vold_app_data_isolation_enabled";
    private static final String APPA_METHOD_CREATE_EXTERNAL_DIRS = "testCreateExternalDirs";
    private static final String APPA_METHOD_TEST_ISOLATED_PROCESS = "testIsolatedProcess";
    private static final String APPA_METHOD_TEST_APP_ZYGOTE_ISOLATED_PROCESS =
            "testAppZygoteIsolatedProcess";
    private static final String APPB_METHOD_CAN_NOT_ACCESS_APPA_EXTERNAL_DIRS =
            "testCanNotAccessAppAExternalDirs";
    private static final String APPB_METHOD_CAN_ACCESS_APPA_EXTERNAL_DIRS =
            "testCanAccessAppAExternalDirs";
    private static final String APPA_METHOD_CHECK_EXTERNAL_DIRS_DO_NOT_EXIST =
            "testAppAExternalDirsDoNotExist";
    private static final String APPA_METHOD_CHECK_EXTERNAL_DIRS_DO_EXIST =
            "testAppAExternalDirsDoExist";
    private static final String APPA_METHOD_CHECK_EXTERNAL_DIRS_UNAVAILABLE =
            "testAppAExternalDirsUnavailable";

    @Before
    public void setUp() throws Exception {
        Utils.prepareSingleUser(getDevice());
        getDevice().uninstallPackage(APPA_PKG);
        getDevice().uninstallPackage(APPB_PKG);
    }

    @After
    public void tearDown() throws Exception {
        getDevice().uninstallPackage(APPA_PKG);
        getDevice().uninstallPackage(APPB_PKG);
    }

    private void forceStopPackage(String packageName) throws Exception {
        getDevice().executeShellCommand("am force-stop " + packageName);
    }

    private void reboot() throws Exception {
        getDevice().reboot();
        waitForBootCompleted(getDevice());
    }

    @Test
    public void testAppAbleToAccessItsDataAfterForceStop() throws Exception {
        // Install AppA and verify no data stored
        new InstallMultiple().addFile(APPA_APK).run();
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CE_DATA_DOES_NOT_EXIST);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_DE_DATA_DOES_NOT_EXIST);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_EXTERNAL_DIRS_DO_NOT_EXIST);

        // Create data in CE, DE and external storage
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CREATE_CE_DE_DATA);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CREATE_EXTERNAL_DIRS);

        // Verify CE, DE and external storage contains data, cur profile is accessible and ref
        // profile is not accessible
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CE_DATA_EXISTS);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_DE_DATA_EXISTS);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_EXTERNAL_DIRS_DO_EXIST);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CUR_PROFILE_ACCESSIBLE);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_REF_PROFILE_NOT_ACCESSIBLE);

        // Force stop and verify CE, DE and external storage contains data, cur profile is
        // accessible and ref profile is not accessible, to confirm it's binding back the same data
        // directory, not binding to a wrong one / create a new one.
        forceStopPackage(APPA_PKG);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CE_DATA_EXISTS);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_DE_DATA_EXISTS);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_EXTERNAL_DIRS_DO_EXIST);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CUR_PROFILE_ACCESSIBLE);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_REF_PROFILE_NOT_ACCESSIBLE);
    }

    @Test
    public void testAppAbleToAccessItsDataAfterReboot() throws Exception {
        // Install AppA and verify no data stored
        new InstallMultiple().addFile(APPA_APK).run();
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CE_DATA_DOES_NOT_EXIST);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_DE_DATA_DOES_NOT_EXIST);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_EXTERNAL_DIRS_DO_NOT_EXIST);

        // Create data in CE, DE and external storage
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CREATE_CE_DE_DATA);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CREATE_EXTERNAL_DIRS);

        // Verify CE, DE and external storage contains data, cur profile is accessible and ref
        // profile is not accessible
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CE_DATA_EXISTS);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_DE_DATA_EXISTS);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_EXTERNAL_DIRS_DO_EXIST);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CUR_PROFILE_ACCESSIBLE);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_REF_PROFILE_NOT_ACCESSIBLE);

        // Reboot and verify CE, DE and external storage contains data, cur profile is accessible
        // and ref profile is not accessible
        reboot();
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CE_DATA_EXISTS);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_DE_DATA_EXISTS);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_EXTERNAL_DIRS_DO_EXIST);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CUR_PROFILE_ACCESSIBLE);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_REF_PROFILE_NOT_ACCESSIBLE);
    }

    private boolean isFbeModeEmulated() throws Exception {
        String mode = getDevice().executeShellCommand("sm get-fbe-mode").trim();
        if (mode.equals(FBE_MODE_EMULATED)) {
            return true;
        } else if (mode.equals(FBE_MODE_NATIVE)) {
            return false;
        }
        fail("Unknown FBE mode: " + mode);
        return false;
    }

    @Test
    public void testDirectBootModeWorks() throws Exception {
        assumeTrue("Screen lock is not supported so skip direct boot test",
                hasDeviceFeature("android.software.secure_lock_screen"));
        // Install AppA and verify no data stored
        new InstallMultiple().addFile(APP_DIRECT_BOOT_A_APK).run();
        new InstallMultiple().addFile(APPB_APK).run();
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CE_DATA_DOES_NOT_EXIST);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_DE_DATA_DOES_NOT_EXIST);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_EXTERNAL_DIRS_DO_NOT_EXIST);

        // Create data in CE, DE and external storage
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CREATE_CE_DE_DATA);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CREATE_EXTERNAL_DIRS);

        // Verify CE, DE and external storage contains data, cur profile is accessible and ref
        // profile is not accessible
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CE_DATA_EXISTS);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_DE_DATA_EXISTS);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_EXTERNAL_DIRS_DO_EXIST);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CUR_PROFILE_ACCESSIBLE);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_REF_PROFILE_NOT_ACCESSIBLE);

        try {
            // Setup screenlock
            getDevice().executeShellCommand("settings put global require_password_to_decrypt 0");
            getDevice().executeShellCommand("locksettings set-disabled false");
            getDevice().executeShellCommand("locksettings set-pin 12345");

            // Give enough time for vold to update keys
            Thread.sleep(15000);

            // Follow DirectBootHostTest, reboot system into known state with keys ejected
            if (isFbeModeEmulated()) {
                final String res = getDevice().executeShellCommand("sm set-emulate-fbe true");
                if (res != null && res.contains("Emulation not supported")) {
                    LogUtil.CLog.i("FBE emulation is not supported, skipping test");
                    return;
                }
                getDevice().waitForDeviceNotAvailable(30000);
                getDevice().waitForDeviceOnline(120000);
            } else {
                getDevice().rebootUntilOnline();
            }
            waitForBootCompleted(getDevice());

            // Verify DE data is still readable and writeable, while CE and external data are not
            // accessible
            runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_DE_DATA_EXISTS);
            runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CE_DATA_DOES_NOT_EXIST);
            runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_EXTERNAL_DIRS_UNAVAILABLE);
            runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CUR_PROFILE_ACCESSIBLE);
            runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_REF_PROFILE_NOT_ACCESSIBLE);
            // Verify cannot access other apps data
            runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CANNOT_ACCESS_APPB_DIR);

            // Unlock device and verify CE, DE and external data still exist, without killing the
            // process, as test process usually will be killed after the test
            runDeviceTests(APPA_PKG, APPA_CLASS,
                    APPA_METHOD_UNLOCK_DEVICE_AND_VERIFY_CE_DE_EXTERNAL_EXIST);

            // Restart test app and verify CE, DE and external storage contains data, cur profile is
            // accessible and ref profile is not accessible
            runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CE_DATA_EXISTS);
            runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_DE_DATA_EXISTS);
            runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_EXTERNAL_DIRS_DO_EXIST);
            runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CUR_PROFILE_ACCESSIBLE);
            runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_REF_PROFILE_NOT_ACCESSIBLE);
        } finally {
            try {
                // Always try to unlock first, then clear screenlock setting
                try {
                    runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_TEST_UNLOCK_DEVICE);
                } catch (Exception e) {}
                getDevice().executeShellCommand("locksettings clear --old 12345");
                getDevice().executeShellCommand("locksettings set-disabled true");
                getDevice().executeShellCommand(
                        "settings delete global require_password_to_decrypt");
            } finally {
                // Get ourselves back into a known-good state
                if (isFbeModeEmulated()) {
                    getDevice().executeShellCommand("sm set-emulate-fbe false");
                    getDevice().waitForDeviceNotAvailable(30000);
                    getDevice().waitForDeviceOnline();
                } else {
                    getDevice().rebootUntilOnline();
                }
                getDevice().waitForDeviceAvailable();
            }
        }
    }

    @Test
    public void testAppNotAbleToAccessItsDataAfterReinstall() throws Exception {
        // Install AppA create CE DE data
        new InstallMultiple().addFile(APPA_APK).run();
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CREATE_CE_DE_DATA);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CREATE_EXTERNAL_DIRS);

        // Reinstall AppA
        getDevice().uninstallPackage(APPA_PKG);
        new InstallMultiple().addFile(APPA_APK).run();

        // Verify CE, DE and external data are removed
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_CE_DATA_DOES_NOT_EXIST);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_DE_DATA_DOES_NOT_EXIST);
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CHECK_EXTERNAL_DIRS_DO_NOT_EXIST);
    }

    @Test
    public void testNormalProcessCannotAccessOtherAppDataDir() throws Exception {
        new InstallMultiple().addFile(APPA_APK).run();
        new InstallMultiple().addFile(APPB_APK).run();

        runDeviceTests(APPB_PKG, APPB_CLASS, APPB_METHOD_CAN_NOT_ACCESS_APPA_DIR);
    }

    @Test
    public void testSharedAppAbleToAccessOtherAppDataDir() throws Exception {
        new InstallMultiple().addFile(APP_SHARED_A_APK).run();
        new InstallMultiple().addFile(APP_SHARED_B_APK).run();

        runDeviceTests(APPB_PKG, APPB_CLASS, APPB_METHOD_CAN_ACCESS_APPA_DIR);
    }

    @Test
    public void testNormalProcessCannotAccessOtherAppExternalDataDir() throws Exception {
        assumeThatFuseDataIsolationIsEnabled(getDevice());

        new InstallMultiple().addFile(APPA_APK).run();
        new InstallMultiple().addFile(APPB_APK).run();

        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CREATE_EXTERNAL_DIRS);
        runDeviceTests(APPB_PKG, APPB_CLASS, APPB_METHOD_CAN_NOT_ACCESS_APPA_EXTERNAL_DIRS);
    }

    @Test
    public void testSharedAppAbleToAccessOtherAppExternalDataDir() throws Exception {
        new InstallMultiple().addFile(APP_SHARED_A_APK).run();
        new InstallMultiple().addFile(APP_SHARED_B_APK).run();

        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_CREATE_EXTERNAL_DIRS);
        runDeviceTests(APPB_PKG, APPB_CLASS, APPB_METHOD_CAN_ACCESS_APPA_EXTERNAL_DIRS);
    }

    private static void assumeThatFuseDataIsolationIsEnabled(ITestDevice device)
            throws DeviceNotAvailableException {
        Assume.assumeThat(device.executeShellCommand(
                CHECK_IF_FUSE_DATA_ISOLATION_IS_ENABLED_COMMANDLINE).trim(),
                is("true"));
    }

    @Test
    public void testIsolatedProcess() throws Exception {
        new InstallMultiple().addFile(APPA_APK).run();
        new InstallMultiple().addFile(APPB_APK).run();
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_TEST_ISOLATED_PROCESS);
    }

    @Test
    public void testAppZygoteIsolatedProcess() throws Exception {
        new InstallMultiple().addFile(APPA_APK).run();
        new InstallMultiple().addFile(APPB_APK).run();
        runDeviceTests(APPA_PKG, APPA_CLASS, APPA_METHOD_TEST_APP_ZYGOTE_ISOLATED_PROCESS);
    }
}
