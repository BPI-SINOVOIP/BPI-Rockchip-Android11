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
package com.android.car.bugreport;

import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;
import android.util.Log;

/**
 * Utilities for scheduling upload jobs.
 */
class JobSchedulingUtils {
    private static final String TAG = JobSchedulingUtils.class.getSimpleName();

    private static final int UPLOAD_JOB_ID = 1;
    private static final int RETRY_DELAY_IN_MS = 5_000;

    /**
     * Schedules {@link UploadJob} under the current user.
     *
     * <p>Make sure this method is called under the primary user.
     *
     * <ul>
     *   <li>require network connectivity
     *   <li>good quality network (large upload size)
     *   <li>persist across reboots
     * </ul>
     */
    static void scheduleUploadJob(Context context) {
        JobScheduler jobScheduler = context.getSystemService(JobScheduler.class);
        if (jobScheduler == null) {
            Log.w(TAG, "Cannot get JobScheduler from context.");
            return;
        }

        JobInfo pendingJob = jobScheduler.getPendingJob(UPLOAD_JOB_ID);
        // if there is already a pending job, do not schedule a new one.
        if (pendingJob != null) {
            Log.d(TAG, "Upload job is already active, not re-scheduling");
            return;
        }

        // NOTE: Don't set estimated network bytes, because we want bug reports to be uploaded
        //       without any constraints.
        jobScheduler.schedule(new JobInfo.Builder(UPLOAD_JOB_ID,
                new ComponentName(context, UploadJob.class))
                .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                .setBackoffCriteria(RETRY_DELAY_IN_MS, JobInfo.BACKOFF_POLICY_LINEAR)
                .build());
    }
}
