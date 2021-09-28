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

/** Test the backup and restore flow for a full-backup app in a profile. */
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull
public class ProfileFullBackupRestoreHostSideTest extends BaseMultiUserBackupHostSideTest {
    private final BackupUtils mBackupUtils = getBackupUtils();
    private ITestDevice mDevice;
    private Optional<Integer> mProfileUserId = Optional.empty();
    private String mTransport;

    /** Create the profile, switch to the local transport and setup the test package. */
    @Before
    @Override
    public void setUp() throws Exception {
        mDevice = getDevice();
        super.setUp();

        // Create profile user.
        int parentUserId = mDevice.getCurrentUser();
        int profileUserId = createProfileUser(parentUserId, "Profile-Full");
        mProfileUserId = Optional.of(profileUserId);
        startUserAndInitializeForBackup(profileUserId);

        // Switch to local transport.
        mTransport = switchUserToLocalTransportAndAssertSuccess(profileUserId);

        // Setup test package.
        installPackageAsUser(FULL_BACKUP_APK, profileUserId);
        clearPackageDataAsUser(FULL_BACKUP_TEST_PACKAGE, profileUserId);
    }

    /** Uninstall the test package and remove the profile. */
    @After
    @Override
    public void tearDown() throws Exception {
        if (mProfileUserId.isPresent()) {
            int profileUserId = mProfileUserId.get();
            if (mTransport != null) {
                clearBackupDataInTransportForUser(
                        FULL_BACKUP_TEST_PACKAGE, mTransport, profileUserId);
            }
            uninstallPackageAsUser(FULL_BACKUP_TEST_PACKAGE, profileUserId);
            mDevice.removeUser(profileUserId);
            mProfileUserId = Optional.empty();
        }
        super.tearDown();
    }

    /**
     * Tests full-backup app backup and restore in the profile user using system restore.
     *
     * <ol>
     *   <li>App writes files to its directory.
     *   <li>Force a backup.
     *   <li>Clear the app's files.
     *   <li>Force a restore.
     *   <li>Check that the files and its contents are restored.
     * </ol>
     */
    @Test
    public void testFullBackupAndSystemRestore() throws Exception {
        int profileUserId = mProfileUserId.get();
        checkDeviceTest("assertFilesDontExist");
        checkDeviceTest("writeFilesAndAssertSuccess");

        mBackupUtils.backupNowAndAssertSuccessForUser(FULL_BACKUP_TEST_PACKAGE, profileUserId);

        checkDeviceTest("clearFilesAndAssertSuccess");

        mBackupUtils.restoreAndAssertSuccessForUser(
                BackupUtils.LOCAL_TRANSPORT_TOKEN, FULL_BACKUP_TEST_PACKAGE, profileUserId);

        checkDeviceTest("assertFilesRestored");
    }

    /**
     * Tests full-backup app backup and restore in the profile user using restore at install.
     *
     * <ol>
     *   <li>App writes files to its directory.
     *   <li>Force a backup.
     *   <li>Uninstall the app.
     *   <li>Install the app to perform a restore-at-install operation.
     *   <li>Check that the files and its contents are restored.
     * </ol>
     */
    @Test
    public void testFullBackupAndRestoreAtInstall() throws Exception {
        int profileUserId = mProfileUserId.get();
        checkDeviceTest("assertFilesDontExist");
        checkDeviceTest("writeFilesAndAssertSuccess");

        mBackupUtils.backupNowAndAssertSuccessForUser(FULL_BACKUP_TEST_PACKAGE, profileUserId);

        checkDeviceTest("clearFilesAndAssertSuccess");

        uninstallPackageAsUser(FULL_BACKUP_TEST_PACKAGE, profileUserId);

        installPackageAsUser(FULL_BACKUP_APK, profileUserId);

        checkDeviceTest("assertFilesRestored");
    }

    private void checkDeviceTest(String methodName) throws DeviceNotAvailableException {
        checkDeviceTestAsUser(
                FULL_BACKUP_TEST_PACKAGE,
                FULL_BACKUP_DEVICE_TEST_NAME,
                methodName,
                mProfileUserId.get());
    }
}
