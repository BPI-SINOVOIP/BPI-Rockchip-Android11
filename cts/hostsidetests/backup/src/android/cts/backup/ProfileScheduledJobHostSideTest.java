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

import android.platform.test.annotations.AppModeFull;

import com.android.compatibility.common.util.BackupUtils;
import com.android.compatibility.common.util.ProtoUtils;
import com.android.server.job.JobSchedulerServiceDumpProto;
import com.android.server.job.JobStatusShortInfoProto;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Optional;

/** Test that key-value and full backup jobs are scheduled in a profile. */
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull
public class ProfileScheduledJobHostSideTest extends BaseMultiUserBackupHostSideTest {
    private static final String PACKAGE_MANAGER_SENTINEL = "@pm@";

    /** From {@link com.android.server.backup.KeyValueBackupJob}. */
    private static final int KEY_VALUE_MIN_JOB_ID = 52417896;

    private static final String KEY_VALUE_JOB_NAME =
            "android/com.android.server.backup.KeyValueBackupJob";

    /** From {@link com.android.server.backup.FullBackupJob}. */
    private static final int FULL_BACKUP_MIN_JOB_ID = 52418896;

    private static final String FULL_BACKUP_JOB_NAME =
            "android/com.android.server.backup.FullBackupJob";

    private static final String TAG = "ProfileScheduledJobHostSideTest";
    private static final String LOGCAT_FILTER =
            "BackupManagerService:* KeyValueBackupTask:* PFTBT:* " + TAG + ":* *:S";
    private static final String KEY_VALUE_SUCCESS_LOG = "K/V backup pass finished";
    private static final String FULL_BACKUP_SUCCESS_LOG = "Full data backup pass finished";
    private static final int TIMEOUT_FOR_KEY_VALUE_SECONDS = 5 * 60; // 5 minutes.
    private static final int TIMEOUT_FOR_FULL_BACKUP_SECONDS = 5 * 60; // 5 minutes.

    private static final String JOB_SCHEDULER_RUN_COMMAND = "cmd jobscheduler run -f android";

    private final BackupUtils mBackupUtils = getBackupUtils();
    private ITestDevice mDevice;
    private Optional<Integer> mProfileUserId = Optional.empty();

    /** Create a profile user and switch to the local transport. */
    @Before
    @Override
    public void setUp() throws Exception {
        mDevice = getDevice();
        super.setUp();

        int parentUserId = mDevice.getCurrentUser();
        int profileUserId = createProfileUser(parentUserId, "Profile-Jobs");
        mProfileUserId = Optional.of(profileUserId);
        startUserAndInitializeForBackup(profileUserId);

        switchUserToLocalTransportAndAssertSuccess(profileUserId);
    }

    /** Remove the profile. */
    @After
    @Override
    public void tearDown() throws Exception {
        if (mProfileUserId.isPresent()) {
            mDevice.removeUser(mProfileUserId.get());
            mProfileUserId = Optional.empty();
        }
        super.tearDown();
    }

    /** Assert that the key value backup job for the profile is scheduled. */
    @Test
    public void testKeyValueBackupJobScheduled() throws Exception {
        int jobId = getJobIdForUser(KEY_VALUE_MIN_JOB_ID, mProfileUserId.get());
        assertThat(isSystemJobScheduled(jobId, KEY_VALUE_JOB_NAME)).isTrue();
    }

    /**
     * Tests the flow of the key value backup job for the profile.
     *
     * <ol>
     *   <li>Install key value backup app to ensure eligibility.
     *   <li>Force run the key value backup job.
     *   <li>Assert success via logcat.
     *   <li>Check that the job was rescheduled.
     * </ol>
     */
    @Test
    public void testKeyValueBackupJobRunsSuccessfully() throws Exception {
        int profileUserId = mProfileUserId.get();
        // Install a new key value backup app.
        installPackageAsUser(KEY_VALUE_APK, profileUserId);

        // Force run k/v job.
        String startLog = mLogcatInspector.mark(TAG);
        int jobId = getJobIdForUser(KEY_VALUE_MIN_JOB_ID, profileUserId);
        mBackupUtils.executeShellCommandSync(JOB_SCHEDULER_RUN_COMMAND + " " + jobId);

        // Check logs for success.
        mLogcatInspector.assertLogcatContainsInOrder(
                LOGCAT_FILTER, TIMEOUT_FOR_KEY_VALUE_SECONDS, startLog, KEY_VALUE_SUCCESS_LOG);

        // Check job rescheduled.
        assertThat(isSystemJobScheduled(jobId, KEY_VALUE_JOB_NAME)).isTrue();
    }

    /** Stop the profile user and assert that the key value job is no longer scheduled. */
    @Test
    public void testKeyValueBackupJobCancelled() throws Exception {
        int profileUserId = mProfileUserId.get();
        int jobId = getJobIdForUser(KEY_VALUE_MIN_JOB_ID, profileUserId);
        assertThat(isSystemJobScheduled(jobId, KEY_VALUE_JOB_NAME)).isTrue();

        mDevice.stopUser(profileUserId, /* waitFlag */ true, /* forceFlag */ true);

        assertThat(isSystemJobScheduled(jobId, KEY_VALUE_JOB_NAME)).isFalse();
    }

    /** Assert that the full backup job for the profile is scheduled. */
    @Test
    public void testFullBackupJobScheduled() throws Exception {
        int jobId = getJobIdForUser(FULL_BACKUP_MIN_JOB_ID, mProfileUserId.get());
        assertThat(isSystemJobScheduled(jobId, FULL_BACKUP_JOB_NAME)).isTrue();
    }

    /**
     * Tests the flow of the full backup job for the profile.
     *
     * <ol>
     *   <li>Install full backup app to ensure eligibility.
     *   <li>Force run the full backup job.
     *   <li>Assert success via logcat.
     *   <li>Check that the job was rescheduled.
     * </ol>
     */
    @Test
    public void testFullBackupJobRunsSuccessfully() throws Exception {
        int profileUserId = mProfileUserId.get();
        // Install a new eligible full backup app and run a backup pass for @pm@ as we cannot
        // perform a full backup pass before @pm@ is backed up.
        installPackageAsUser(FULL_BACKUP_APK, profileUserId);
        mBackupUtils.backupNowAndAssertSuccessForUser(PACKAGE_MANAGER_SENTINEL, profileUserId);

        // Force run full backup job.
        String startLog = mLogcatInspector.mark(TAG);
        int jobId = getJobIdForUser(FULL_BACKUP_MIN_JOB_ID, profileUserId);
        mBackupUtils.executeShellCommandSync(JOB_SCHEDULER_RUN_COMMAND + " " + jobId);

        // Check logs for success.
        mLogcatInspector.assertLogcatContainsInOrder(
                LOGCAT_FILTER, TIMEOUT_FOR_FULL_BACKUP_SECONDS, startLog, FULL_BACKUP_SUCCESS_LOG);

        // Check job rescheduled.
        assertThat(isSystemJobScheduled(jobId, FULL_BACKUP_JOB_NAME)).isTrue();
    }

    /** Stop the profile user and assert that the full backup job is no longer scheduled. */
    @Test
    public void testFullBackupJobCancelled() throws Exception {
        int profileUserId = mProfileUserId.get();
        int jobId = getJobIdForUser(FULL_BACKUP_MIN_JOB_ID, profileUserId);
        assertThat(isSystemJobScheduled(jobId, FULL_BACKUP_JOB_NAME)).isTrue();

        mDevice.stopUser(profileUserId, /* waitFlag */ true, /* forceFlag */ true);

        assertThat(isSystemJobScheduled(jobId, FULL_BACKUP_JOB_NAME)).isFalse();
    }

    private int getJobIdForUser(int minJobId, int userId) {
        return minJobId + userId;
    }

    /** Returns {@code true} if there is a system job scheduled with the specified parameters. */
    private boolean isSystemJobScheduled(int jobId, String jobName) throws Exception {
        // TODO: Look into making a higher level adb command or a system API for this instead.
        //  (e.g. "adb shell cmd jobscheduler is-job-scheduled system JOB-ID JOB-NAME")
        final JobSchedulerServiceDumpProto dump =
                ProtoUtils.getProto(mDevice, JobSchedulerServiceDumpProto.parser(),
                        ProtoUtils.DUMPSYS_JOB_SCHEDULER);
        for (JobSchedulerServiceDumpProto.RegisteredJob job : dump.getRegisteredJobsList()) {
            final JobStatusShortInfoProto info = job.getInfo();
            if (info.getCallingUid() == 1000 && info.getJobId() == jobId
                    && jobName.equals(info.getBatteryName())) {
                return true;
            }
        }
        return false;
    }
}
