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

import static org.junit.Assert.fail;

import android.platform.test.annotations.AppModeFull;

import com.android.compatibility.common.util.BackupUtils;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.util.Optional;
import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;

/** Test that backup and restore can run in parallel across users. */
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull
public class ParallelUserBackupRestoreHostSideTest extends BaseMultiUserBackupHostSideTest {
    private static final long TIMEOUT_BACKUP = TimeUnit.MINUTES.toMillis(5);
    private static final long TIMEOUT_RESTORE = TimeUnit.MINUTES.toMillis(5);
    private static final long TIMEOUT_RESULT = TimeUnit.SECONDS.toMillis(30);
    private static final String BACKUP_SUCCESS_OUTPUT = "Backup finished with result: Success";
    private static final String RESTORE_SUCCESS_OUTPUT = "restoreFinished: 0";

    private final BackupUtils mBackupUtils = getBackupUtils();
    private ITestDevice mDevice;
    private ExecutorService mExecutorService;

    private int mParentUserId;
    private Optional<Integer> mProfileUserId = Optional.empty();

    /** Setup the parent and profile user for backup. */
    @Before
    @Override
    public void setUp() throws Exception {
        mDevice = getDevice();
        super.setUp();

        mParentUserId = mDevice.getCurrentUser();
        startUserAndInitializeForBackup(mParentUserId);
        switchUserToLocalTransportAndAssertSuccess(mParentUserId);

        int profileUserId = createProfileUser(mParentUserId, "Profile-Parallel");
        mProfileUserId = Optional.of(profileUserId);
        startUserAndInitializeForBackup(profileUserId);
        switchUserToLocalTransportAndAssertSuccess(profileUserId);

        mExecutorService = Executors.newFixedThreadPool(2);
    }

    /** Remove the profile user. */
    @After
    @Override
    public void tearDown() throws Exception {
        if (mExecutorService != null) {
            mExecutorService.shutdownNow();
        }
        if (mProfileUserId.isPresent()) {
            mDevice.removeUser(mProfileUserId.get());
            mProfileUserId = Optional.empty();
        }
    }

    /**
     * Test that backup and restore can happen concurrently between the parent and profile user.
     *
     * <p>For backup:
     *
     * <ol>
     *   <li>Execute "adb shell bmgr backupnow --all" for both users concurrently.
     *   <li>Verify that both backups complete within the timeout.
     *   <li>Verify that the shell result is "Backup finished with result: Success" for each backup.
     * </ol>
     *
     * For restore:
     *
     * <ol>
     *   <li>Execute "adb shell bmgr restore [local transport token]" for both users concurrently.
     *   <li>Verify that both restores complete within the timeout.
     *   <li>Verify that the shell result is "restoreFinished: 0" for each restore.
     * </ol>
     */
    @Test
    public void testParallelBackupAndRestore() throws Exception {
        int profileUserId = mProfileUserId.get();
        // Test parallel backups.
        CountDownLatch backupLatch = new CountDownLatch(2);
        Future<String> parentBackupOutput =
                mExecutorService.submit(backupCallableForUser(mParentUserId, backupLatch));
        Future<String> profileBackupOutput =
                mExecutorService.submit(backupCallableForUser(profileUserId, backupLatch));

        // Wait for both backups to complete.
        boolean backupsCompleted = backupLatch.await(TIMEOUT_BACKUP, TimeUnit.MILLISECONDS);
        assertThat(backupsCompleted).isTrue();

        // Check that both backups succeeded.
        assertThat(parentBackupOutput.get(TIMEOUT_RESULT, TimeUnit.SECONDS))
                .contains(BACKUP_SUCCESS_OUTPUT);
        assertThat(profileBackupOutput.get(TIMEOUT_RESULT, TimeUnit.SECONDS))
                .contains(BACKUP_SUCCESS_OUTPUT);

        // Test parallel restores.
        CountDownLatch restoreLatch = new CountDownLatch(2);
        Future<String> parentRestoreOutput =
                mExecutorService.submit(restoreCallableForUser(mParentUserId, restoreLatch));
        Future<String> profileRestoreOutput =
                mExecutorService.submit(restoreCallableForUser(profileUserId, restoreLatch));

        // Wait for both restores to complete.
        boolean restoresCompleted = restoreLatch.await(TIMEOUT_RESTORE, TimeUnit.MILLISECONDS);
        assertThat(restoresCompleted).isTrue();

        // Check that both restores succeeded.
        assertThat(parentRestoreOutput.get(TIMEOUT_RESULT, TimeUnit.SECONDS))
                .contains(RESTORE_SUCCESS_OUTPUT);
        assertThat(profileRestoreOutput.get(TIMEOUT_RESULT, TimeUnit.SECONDS))
                .contains(RESTORE_SUCCESS_OUTPUT);
    }

    /**
     * Executes "adb shell bmgr backupnow --all" for user {@code userId}, counts down {@code
     * backupLatch} when complete, and returns the shell output.
     */
    private Callable<String> backupCallableForUser(int userId, CountDownLatch backupLatch) {
        return () -> {
            try {
                String output =
                        mBackupUtils.getShellCommandOutput(
                                String.format("bmgr --user %d backupnow --all", userId));
                backupLatch.countDown();
                return output;
            } catch (IOException e) {
                e.printStackTrace();
                fail("Couldn't perform backup for user " + userId);
            }
            return null;
        };
    }

    /**
     * Executes "adb shell bmgr restore [local transport token]" for user {@code userId}, counts
     * down {@code restoreLatch} when complete, and returns the shell output.
     */
    private Callable<String> restoreCallableForUser(int userId, CountDownLatch restoreLatch) {
        return () -> {
            try {
                String output =
                        mBackupUtils.getShellCommandOutput(
                                String.format(
                                        "bmgr --user %d restore %s",
                                        userId, BackupUtils.LOCAL_TRANSPORT_TOKEN));
                restoreLatch.countDown();
                return output;
            } catch (IOException e) {
                e.printStackTrace();
                fail("Couldn't perform restore for user " + userId);
            }
            return null;
        };
    }
}
