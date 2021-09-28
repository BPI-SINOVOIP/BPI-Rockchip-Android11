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

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;

import android.platform.test.annotations.AppModeFull;

import com.android.compatibility.common.util.BackupUtils;
import com.android.compatibility.common.util.CommonTestUtils;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.util.Optional;
import java.util.concurrent.TimeUnit;

/** Base class for CTS multi-user backup/restore host-side tests. */
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull
public abstract class BaseMultiUserBackupHostSideTest extends BaseBackupHostSideTest {
    private static final String USER_SETUP_COMPLETE_SETTING = "user_setup_complete";
    private static final long TRANSPORT_INITIALIZATION_TIMEOUT_SECS = TimeUnit.MINUTES.toSeconds(2);

    // Key-value test package.
    static final String KEY_VALUE_APK = "CtsProfileKeyValueApp.apk";
    static final String KEY_VALUE_TEST_PACKAGE = "android.cts.backup.profilekeyvalueapp";
    static final String KEY_VALUE_DEVICE_TEST_NAME =
            KEY_VALUE_TEST_PACKAGE + ".ProfileKeyValueBackupRestoreTest";

    // Full backup test package.
    static final String FULL_BACKUP_APK = "CtsProfileFullBackupApp.apk";
    static final String FULL_BACKUP_TEST_PACKAGE = "android.cts.backup.profilefullbackupapp";
    static final String FULL_BACKUP_DEVICE_TEST_NAME =
            FULL_BACKUP_TEST_PACKAGE + ".ProfileFullBackupRestoreTest";

    protected final BackupUtils mBackupUtils = getBackupUtils();
    private ITestDevice mDevice;

    // Store initial device state as Optional as tearDown() will execute even if we have assumption
    // failures in setUp().
    private Optional<Integer> mInitialUser = Optional.empty();

    /** Check device features and keep track of pre-test device state. */
    @Before
    public void setUp() throws Exception {
        mDevice = getDevice();
        super.setUp();

        // Check that backup and multi-user features are both supported.
        assumeTrue("Backup feature not supported", mIsBackupSupported);
        assumeTrue("Multi-user feature not supported", mDevice.isMultiUserSupported());

        // Keep track of initial user state to restore in tearDown.
        int currentUserId = mDevice.getCurrentUser();
        mInitialUser = Optional.of(currentUserId);

        // Switch to primary user.
        int primaryUserId = mDevice.getPrimaryUserId();
        if (currentUserId != primaryUserId) {
            mDevice.switchUser(primaryUserId);
        }
    }

    /** Restore pre-test device state. */
    @After
    public void tearDown() throws Exception {
        if (mInitialUser.isPresent()) {
            mDevice.switchUser(mInitialUser.get());
            mInitialUser = Optional.empty();
        }
    }

    /**
     * Attempts to create a profile tied to the parent user {@code parentId}. Returns the user id of
     * the profile if successful, otherwise throws a {@link RuntimeException}.
     */
    int createProfileUser(int parentId, String profileName) throws IOException {
        String output =
                mBackupUtils.executeShellCommandAndReturnOutput(
                        String.format("pm create-user --profileOf %d %s", parentId, profileName));
        try {
            // Expected output is "Success: created user id <id>"
            String userId = output.substring(output.lastIndexOf(" ")).trim();
            return Integer.parseInt(userId);
        } catch (NumberFormatException e) {
            CLog.d("Failed to parse user id when creating a profile user");
            throw new RuntimeException(output, e);
        }
    }

    /** Start the user. */
    void startUser(int userId) throws DeviceNotAvailableException {
        boolean startSuccessful = mDevice.startUser(userId, /* wait for RUNNING_UNLOCKED */ true);
        assertThat(startSuccessful).isTrue();

        mDevice.setSetting(userId, "secure", USER_SETUP_COMPLETE_SETTING, "1");
    }

    /** Start the user and set necessary conditions for backup to be enabled in the user. */
    void startUserAndInitializeForBackup(int userId)
            throws IOException, DeviceNotAvailableException, InterruptedException {
        // Turn on multi-user feature for this user.
        mBackupUtils.activateBackupForUser(true, userId);

        startUser(userId);

        mBackupUtils.waitUntilBackupServiceIsRunning(userId);

        mBackupUtils.enableBackupForUser(true, userId);
        assertThat(mBackupUtils.isBackupEnabledForUser(userId)).isTrue();
    }

    /**
     * Selects the local transport as the current transport for user {@code userId}. Returns the
     * {@link String} name of the local transport.
     */
    String switchUserToLocalTransportAndAssertSuccess(int userId)
            throws Exception {
        // Make sure the user has the local transport.
        String localTransport = mBackupUtils.getLocalTransportName();

        // TODO (b/121198010): Update dumpsys or add shell command to query status of transport
        // initialization. Transports won't be available until they are initialized/registered.
        CommonTestUtils.waitUntil("wait for user to have local transport",
                TRANSPORT_INITIALIZATION_TIMEOUT_SECS,
                () -> userHasBackupTransport(localTransport, userId));

        // Switch to the local transport and assert success.
        mBackupUtils.setBackupTransportForUser(localTransport, userId);
        assertThat(mBackupUtils.isLocalTransportSelectedForUser(userId)).isTrue();

        return localTransport;
    }

    // TODO(b/139652329): Move to backup utils.
    private boolean userHasBackupTransport(
            String transport, int userId) throws DeviceNotAvailableException {
        String output = mDevice.executeShellCommand("bmgr --user " + userId + " list transports");
        for (String t : output.split(" ")) {
            if (transport.equals(t.replace("*", "").trim())) {
                return true;
            }
        }
        return false;
    }

    /** Runs "bmgr --user <id> wipe <transport> <package>" to clear the backup data. */
    void clearBackupDataInTransportForUser(String packageName, String transport, int userId)
            throws DeviceNotAvailableException {
        mDevice.executeShellCommand(
                String.format("bmgr --user %d wipe %s %s", userId, transport, packageName));
    }

    /** Clears data of {@code packageName} for user {@code userId}. */
    void clearPackageDataAsUser(String packageName, int userId) throws DeviceNotAvailableException {
        mDevice.executeShellCommand(String.format("pm clear --user %d %s", userId, packageName));
    }

    /** Installs {@code apk} for user {@code userId} and allows replacements and downgrades. */
    void installPackageAsUser(String apk, int userId)
            throws DeviceNotAvailableException, TargetSetupError {
        installPackageAsUser(apk, false, userId, "-r", "-d");
    }

    /** Uninstalls {@code packageName} for user {@code userId}. */
    void uninstallPackageAsUser(String packageName, int userId) throws DeviceNotAvailableException {
        mDevice.executeShellCommand(
                String.format("pm uninstall --user %d %s", userId, packageName));
    }

    /**
     * Installs existing {@code packageName} for user {@code userId}. This fires off asynchronous
     * restore and returns before the restore operation has finished.
     */
    void installExistingPackageAsUser(String packageName, int userId)
            throws DeviceNotAvailableException {
        mDevice.executeShellCommand(
                String.format("pm install-existing --user %d %s", userId, packageName));
    }

    /**
     * Installs existing {@code packageName} for user {@code userId}. This fires off asynchronous
     * restore and returns after it receives an intent which is sent when the restore operation has
     * completed.
     */
    void installExistingPackageAsUserWaitTillComplete(String packageName, int userId)
            throws DeviceNotAvailableException {
        mDevice.executeShellCommand(
                String.format("pm install-existing --user %d --wait %s", userId, packageName));
    }

    /** Run device side test as user {@code userId}. */
    void checkDeviceTestAsUser(String packageName, String className, String testName, int userId)
            throws DeviceNotAvailableException {
        boolean result = runDeviceTests(mDevice, packageName, className, testName, userId, null);
        assertThat(result).isTrue();
    }
}
