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

package android.cts.backup;

import android.platform.test.annotations.AppModeFull;

import com.android.compatibility.common.util.BackupUtils;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Optional;

/** Test the backup and restore flow for a key-value app in a profile. */
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull
public class ProfileKeyValueBackupRestoreHostSideTest extends BaseMultiUserBackupHostSideTest {
    private static final String TAG = "ProfileKeyValueBackupRestoreHostSideTest";
    private static final String LOGCAT_FILTER = "BackupManagerService:* " + TAG + ":* *:S";
    private static final String RESTOREATINSTALL_LOG =
            "restoreAtInstall pkg=" + KEY_VALUE_TEST_PACKAGE;
    private static final String RESTORECOMPLETE_LOG = "Restore complete";
    private static final int TIMEOUT_FOR_RESTOREATINSTALL_SECONDS = 30;

    private final BackupUtils mBackupUtils = getBackupUtils();
    private ITestDevice mDevice;
    private int mParentUserId;
    private Optional<Integer> mProfileUserId = Optional.empty();
    private String mTransport;

    /** Create the profile, switch to the local transport and setup the test package. */
    @Before
    @Override
    public void setUp() throws Exception {
        mDevice = getDevice();
        super.setUp();

        // Create profile user.
        mParentUserId = mDevice.getCurrentUser();
        int profileUserId = createProfileUser(mParentUserId, "Profile-KV");
        mProfileUserId = Optional.of(profileUserId);
        startUserAndInitializeForBackup(profileUserId);

        // Switch to local transport.
        mTransport = switchUserToLocalTransportAndAssertSuccess(profileUserId);

        // Setup test package.
        installPackageAsUser(KEY_VALUE_APK, profileUserId);
        clearPackageDataAsUser(KEY_VALUE_TEST_PACKAGE, profileUserId);
    }

    /** Uninstall the test package and remove the profile. */
    @After
    @Override
    public void tearDown() throws Exception {
        if (mProfileUserId.isPresent()) {
            int profileUserId = mProfileUserId.get();
            if (mTransport != null) {
                clearBackupDataInTransportForUser(
                        KEY_VALUE_TEST_PACKAGE, mTransport, profileUserId);
            }
            uninstallPackageAsUser(KEY_VALUE_TEST_PACKAGE, profileUserId);
            mDevice.removeUser(profileUserId);
            mProfileUserId = Optional.empty();
        }
        super.tearDown();
    }

    /**
     * Tests key-value app backup and restore in the profile user.
     *
     * <ol>
     *   <li>App writes shared preferences.
     *   <li>Force a backup.
     *   <li>Uninstall the app.
     *   <li>Install the app to perform a restore-at-install operation.
     *   <li>Check that the shared preferences are restored.
     * </ol>
     */
    @Test
    public void testKeyValueBackupAndRestore() throws Exception {
        int profileUserId = mProfileUserId.get();
        checkDeviceTest("assertSharedPrefsIsEmpty");
        checkDeviceTest("writeSharedPrefsAndAssertSuccess");

        mBackupUtils.backupNowAndAssertSuccessForUser(KEY_VALUE_TEST_PACKAGE, profileUserId);

        uninstallPackageAsUser(KEY_VALUE_TEST_PACKAGE, profileUserId);

        installPackageAsUser(KEY_VALUE_APK, profileUserId);

        checkDeviceTest("assertSharedPrefsRestored");
    }

    /**
     * Tests key-value app backup and restore in the profile user, when the app is already installed
     * for another user. This test parses logcat to assert that the asynchronous restore-at-install
     * operation has completed.
     *
     * <ol>
     *   <li>App writes shared preferences.
     *   <li>Force a backup.
     *   <li>Uninstall the app.
     *   <li>Install the app for parent user.
     *   <li>Install the app, which now already exists in parent user, for profile user to perform a
     *       restore-at-install operation. This fires off asynchronous restore and returns before
     *       the restore operation has finished.
     *   <li>Assert that restore has finished via logcat.
     *   <li>Check that the shared preferences are restored.
     * </ol>
     */
    @Test
    public void testKeyValueBackupAndRestoreForInstallExistingPackage() throws Exception {
        int profileUserId = mProfileUserId.get();
        checkDeviceTest("assertSharedPrefsIsEmpty");
        checkDeviceTest("writeSharedPrefsAndAssertSuccess");

        mBackupUtils.backupNowAndAssertSuccessForUser(KEY_VALUE_TEST_PACKAGE, profileUserId);

        uninstallPackageAsUser(KEY_VALUE_TEST_PACKAGE, profileUserId);

        installPackageAsUser(KEY_VALUE_APK, mParentUserId);
        String mark = mLogcatInspector.mark(TAG);
        installExistingPackageAsUser(KEY_VALUE_TEST_PACKAGE, profileUserId);

        // Check logs for success.
        mLogcatInspector.assertLogcatContainsInOrder(
                LOGCAT_FILTER,
                TIMEOUT_FOR_RESTOREATINSTALL_SECONDS,
                mark,
                RESTOREATINSTALL_LOG,
                RESTORECOMPLETE_LOG);

        checkDeviceTest("assertSharedPrefsRestored");
        uninstallPackageAsUser(KEY_VALUE_TEST_PACKAGE, mParentUserId);
    }

    /**
     * Tests key-value app backup and restore in the profile user, when the app is already installed
     * for another user. This test waits for an intent to be sent from the asynchronous restore
     * operation to signal that the restore-at-install operation has completed.
     *
     * <ol>
     *   <li>App writes shared preferences.
     *   <li>Force a backup.
     *   <li>Uninstall the app.
     *   <li>Install the app for parent user.
     *   <li>Install the app, which now already exists in parent user, for profile user to perform a
     *   restore-at-install operation. This fires off asynchronous restore and waits for an intent
     *   which is sent when the restore operation has completed.
     *   <li>Check that the shared preferences are restored.
     * </ol>
     */
    @Test
    public void testKeyValueBackupAndRestoreForInstallExistingPackageWaitTillComplete()
        throws Exception {
        int profileUserId = mProfileUserId.get();
        checkDeviceTest("assertSharedPrefsIsEmpty");
        checkDeviceTest("writeSharedPrefsAndAssertSuccess");

        mBackupUtils.backupNowAndAssertSuccessForUser(KEY_VALUE_TEST_PACKAGE, profileUserId);

        uninstallPackageAsUser(KEY_VALUE_TEST_PACKAGE, profileUserId);

        installPackageAsUser(KEY_VALUE_APK, mParentUserId);
        installExistingPackageAsUserWaitTillComplete(KEY_VALUE_TEST_PACKAGE, profileUserId);

        checkDeviceTest("assertSharedPrefsRestored");
        uninstallPackageAsUser(KEY_VALUE_TEST_PACKAGE, mParentUserId);
    }

    private void checkDeviceTest(String methodName) throws DeviceNotAvailableException {
        checkDeviceTestAsUser(
                KEY_VALUE_TEST_PACKAGE,
                KEY_VALUE_DEVICE_TEST_NAME,
                methodName,
                mProfileUserId.get());
    }
}
