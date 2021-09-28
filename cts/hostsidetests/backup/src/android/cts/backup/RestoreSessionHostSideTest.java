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
 * limitations under the License
 */

package android.cts.backup;

import static org.junit.Assert.assertTrue;

import android.platform.test.annotations.AppModeFull;

import com.android.compatibility.common.util.BackupUtils;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import java.io.IOException;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Optional;

/**
 * Tests for system APIs in {@link RestoreSession}
 *
 * <p>These tests use the local transport.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull
public class RestoreSessionHostSideTest extends BaseBackupHostSideTest {
    private static final int USER_SYSTEM = 0;
    private static final String MAIN_TEST_APP_PKG = "android.cts.backup.restoresessionapp";
    private static final String DEVICE_MAIN_TEST_CLASS_NAME =
            MAIN_TEST_APP_PKG + ".RestoreSessionTest";
    private static final String MAIN_TEST_APK = "CtsRestoreSessionApp.apk";

    private static final String TEST_APP_PKG_PREFIX = "android.cts.backup.restoresessionapp";
    private static final String TEST_APP_APK_PREFIX = "CtsRestoreSessionApp";
    private static final int TEST_APPS_COUNT = 3;

    private Optional<String> mOldTransport = Optional.empty();
    private BackupUtils mBackupUtils;

    /** Switch to local transport. */
    @Before
    public void setUp() throws Exception {
        mBackupUtils = getBackupUtils();
        mOldTransport = Optional.of(setBackupTransport(mBackupUtils.getLocalTransportName()));
        installPackage(MAIN_TEST_APK);
    }

    /** Restore transport settings to original values. */
    @After
    public void tearDown() throws Exception {
        if (mOldTransport.isPresent()) {
            setBackupTransport(mOldTransport.get());
            mOldTransport = Optional.empty();

            uninstallPackage(MAIN_TEST_APK);
        }
    }

    /** Test {@link RestoreSession#restorePackage(RestoreObserver, String)} */
    @Test
    public void testRestorePackage() throws Exception {
        testRestorePackagesInternal("testRestorePackage", /* packagesToRestore */ 1);
    }

    /**
     * Test {@link RestoreSession#restorePackage(RestoreObserver, String, BackupManagerMonitor)}
     */
    @Test
    public void testRestorePackageWithMonitorParam() throws Exception {
        testRestorePackagesInternal("testRestorePackageWithMonitorParam",
                /* packagesToRestore */ 1);
    }

    /** Test {@link RestoreSession#restorePackages(long, RestoreObserver, Set)} */
    @Test
    public void testRestorePackages() throws Exception {
        testRestorePackagesInternal("testRestorePackages", /* packagesToRestore */ 2);
    }

    /**
     * Test {@link RestoreSession#restorePackages(long, RestoreObserver, Set, BackupManagerMonitor)}
     */
    @Test
    public void testRestorePackagesWithMonitorParam() throws Exception {
        testRestorePackagesInternal("testRestorePackagesWithMonitorParam",
                /* packagesToRestore */ 2);
    }

    /**
     *
     *
     * <ol>
     *   <li>Install 3 test packages on the device
     *   <li>Write dummy values to shared preferences for each package
     *   <li>Backup each package (adb shell bmgr backupnow)
     *   <li>Clear shared preferences for each package
     *   <li>Run restore for the first {@code numPackagesToRestore}, verify only those are restored
     *   <li>Verify that shared preferences for the restored packages are restored correctly
     * </ol>
     */
    private void testRestorePackagesInternal(String deviceTestName, int numPackagesToRestore)
            throws Exception {
        installPackage(getApkNameForTestApp(1));
        installPackage(getApkNameForTestApp(2));
        installPackage(getApkNameForTestApp(3));

        // Write dummy value to shared preferences for all test packages.
        checkRestoreSessionDeviceTestForAllApps("testSaveValuesToSharedPrefs");
        checkRestoreSessionDeviceTestForAllApps("testCheckSharedPrefsExist");

        // Backup all test packages.
        mBackupUtils.backupNowAndAssertSuccess(getPackageNameForTestApp(1));
        mBackupUtils.backupNowAndAssertSuccess(getPackageNameForTestApp(2));
        mBackupUtils.backupNowAndAssertSuccess(getPackageNameForTestApp(3));

        // Clear shared preferences for all test packages.
        checkRestoreSessionDeviceTestForAllApps("testClearSharedPrefs");
        checkRestoreSessionDeviceTestForAllApps("testCheckSharedPrefsDontExist");

        runRestoreSessionDeviceTestAndAssertSuccess(
                MAIN_TEST_APP_PKG, DEVICE_MAIN_TEST_CLASS_NAME, deviceTestName);

        // Check that shared prefs are only restored (and restored correctly) for the packages
        // that need to be restored.
        for (int i = 1; i <= TEST_APPS_COUNT; i++) {
            if (i <= numPackagesToRestore) {
                checkRestoreSessionDeviceTest(i, "testCheckSharedPrefsExist");
            } else {
                checkRestoreSessionDeviceTest(i, "testCheckSharedPrefsDontExist");
            }
        }

        uninstallPackage(getPackageNameForTestApp(1));
        uninstallPackage(getPackageNameForTestApp(2));
        uninstallPackage(getPackageNameForTestApp(3));
    }

    /** Run the given device test for all test apps. */
    private void checkRestoreSessionDeviceTestForAllApps(String testName)
            throws DeviceNotAvailableException {
        for (int appNumber = 1; appNumber <= TEST_APPS_COUNT; appNumber++) {
            checkRestoreSessionDeviceTest(appNumber, testName);
        }
    }

    /** Run device test with the given test name and test app number. */
    private void checkRestoreSessionDeviceTest(int testAppNumber, String testName)
            throws DeviceNotAvailableException {
        String packageName = getPackageNameForTestApp(testAppNumber);
        runRestoreSessionDeviceTestAndAssertSuccess(
                packageName, packageName + ".RestoreSessionAppTest", testName);
    }

    private void runRestoreSessionDeviceTestAndAssertSuccess(
            String packageName, String fullClassName, String testName)
            throws DeviceNotAvailableException {
        boolean result = runDeviceTests(packageName, fullClassName, testName);
        assertTrue("Device test failed: " + testName, result);
    }

    private String getPackageNameForTestApp(int appNumber) {
        return TEST_APP_PKG_PREFIX + appNumber;
    }

    private String getApkNameForTestApp(int appNumber) {
        return TEST_APP_APK_PREFIX + appNumber + ".apk";
    }

    private String setBackupTransport(String transport) throws IOException {
        return mBackupUtils.setBackupTransportForUser(transport, USER_SYSTEM);
    }
}
