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

package android.jobscheduler.cts;

import android.annotation.TargetApi;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.provider.Settings;

import com.android.compatibility.common.util.SystemUtil;

/**
 * Tests related to scheduling jobs.
 */
@TargetApi(30)
public class JobSchedulingTest extends BaseJobSchedulerTest {
    private static final int MIN_SCHEDULE_QUOTA = 250;
    private static final int JOB_ID = JobSchedulingTest.class.hashCode();

    private String originalJobSchedulerConstants;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        originalJobSchedulerConstants = Settings.Global.getString(getContext().getContentResolver(),
                Settings.Global.JOB_SCHEDULER_CONSTANTS);
    }

    @Override
    public void tearDown() throws Exception {
        mJobScheduler.cancel(JOB_ID);
        Settings.Global.putString(getContext().getContentResolver(),
                Settings.Global.JOB_SCHEDULER_CONSTANTS, originalJobSchedulerConstants);
        SystemUtil.runShellCommand(getInstrumentation(), "cmd jobscheduler reset-schedule-quota");

        // The super method should be called at the end.
        super.tearDown();
    }

    /**
     * Test that apps can call schedule at least the minimum amount of times without being
     * blocked.
     */
    public void testMinSuccessfulSchedulingQuota() {
        JobInfo jobInfo = new JobInfo.Builder(JOB_ID, kJobServiceComponent)
                .setMinimumLatency(60 * 60 * 1000L)
                .setPersisted(true)
                .build();

        for (int i = 0; i < MIN_SCHEDULE_QUOTA; ++i) {
            assertEquals(JobScheduler.RESULT_SUCCESS, mJobScheduler.schedule(jobInfo));
        }
    }

    /**
     * Test that scheduling fails once an app hits the schedule quota limit.
     */
    public void testFailingScheduleOnQuotaExceeded() {
        Settings.Global.putString(getContext().getContentResolver(),
                Settings.Global.JOB_SCHEDULER_CONSTANTS,
                "enable_api_quotas=true,aq_schedule_count=300,aq_schedule_window_ms=300000,"
                        + "aq_schedule_throw_exception=false,aq_schedule_return_failure=true");

        JobInfo jobInfo = new JobInfo.Builder(JOB_ID, kJobServiceComponent)
                .setMinimumLatency(60 * 60 * 1000L)
                .setPersisted(true)
                .build();

        for (int i = 0; i < 500; ++i) {
            final int expected =
                    i < 300 ? JobScheduler.RESULT_SUCCESS : JobScheduler.RESULT_FAILURE;
            assertEquals("Got unexpected result for schedule #" + (i + 1),
                    expected, mJobScheduler.schedule(jobInfo));
        }
    }

    /**
     * Test that scheduling succeeds even after an app hits the schedule quota limit.
     */
    public void testContinuingScheduleOnQuotaExceeded() {
        Settings.Global.putString(getContext().getContentResolver(),
                Settings.Global.JOB_SCHEDULER_CONSTANTS,
                "enable_api_quotas=true,aq_schedule_count=300,aq_schedule_window_ms=300000,"
                        + "aq_schedule_throw_exception=false,aq_schedule_return_failure=false");

        JobInfo jobInfo = new JobInfo.Builder(JOB_ID, kJobServiceComponent)
                .setMinimumLatency(60 * 60 * 1000L)
                .setPersisted(true)
                .build();

        for (int i = 0; i < 500; ++i) {
            assertEquals("Got unexpected result for schedule #" + (i + 1),
                    JobScheduler.RESULT_SUCCESS, mJobScheduler.schedule(jobInfo));
        }
    }

    /**
     * Test that non-persisted jobs aren't limited by quota.
     */
    public void testNonPersistedJobsNotLimited() {
        Settings.Global.putString(getContext().getContentResolver(),
                Settings.Global.JOB_SCHEDULER_CONSTANTS,
                "enable_api_quotas=true,aq_schedule_count=300,aq_schedule_window_ms=60000,"
                        + "aq_schedule_throw_exception=false,aq_schedule_return_failure=true");

        JobInfo jobInfo = new JobInfo.Builder(JOB_ID, kJobServiceComponent)
                .setMinimumLatency(60 * 60 * 1000L)
                .setPersisted(false)
                .build();

        for (int i = 0; i < 500; ++i) {
            assertEquals(JobScheduler.RESULT_SUCCESS, mJobScheduler.schedule(jobInfo));
        }
    }
}
