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
package android.jobscheduler.cts;

import static android.jobscheduler.cts.jobtestapp.TestJobService.ACTION_JOB_STARTED;
import static android.jobscheduler.cts.jobtestapp.TestJobService.ACTION_JOB_STOPPED;
import static android.jobscheduler.cts.jobtestapp.TestJobService.JOB_PARAMS_EXTRA_KEY;
import static android.server.wm.WindowManagerState.STATE_RESUMED;

import android.app.job.JobParameters;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.jobscheduler.cts.jobtestapp.TestActivity;
import android.jobscheduler.cts.jobtestapp.TestJobSchedulerReceiver;
import android.os.SystemClock;
import android.server.wm.WindowManagerStateHelper;
import android.util.Log;

/**
 * Common functions to interact with the test app.
 */
class TestAppInterface {
    private static final String TAG = TestAppInterface.class.getSimpleName();

    static final String TEST_APP_PACKAGE = "android.jobscheduler.cts.jobtestapp";
    private static final String TEST_APP_ACTIVITY = TEST_APP_PACKAGE + ".TestActivity";
    static final String TEST_APP_RECEIVER = TEST_APP_PACKAGE + ".TestJobSchedulerReceiver";

    private final Context mContext;
    private final int mJobId;

    /* accesses must be synchronized on itself */
    private final TestJobStatus mTestJobStatus = new TestJobStatus();

    TestAppInterface(Context ctx, int jobId) {
        mContext = ctx;
        mJobId = jobId;

        final IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(ACTION_JOB_STARTED);
        intentFilter.addAction(ACTION_JOB_STOPPED);
        mContext.registerReceiver(mReceiver, intentFilter);
    }

    void cleanup() {
        final Intent cancelJobsIntent = new Intent(TestJobSchedulerReceiver.ACTION_CANCEL_JOBS);
        cancelJobsIntent.setComponent(new ComponentName(TEST_APP_PACKAGE, TEST_APP_RECEIVER));
        cancelJobsIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.sendBroadcast(cancelJobsIntent);
        mContext.sendBroadcast(new Intent(TestActivity.ACTION_FINISH_ACTIVITY));
        mContext.unregisterReceiver(mReceiver);
        mTestJobStatus.reset();
    }

    void scheduleJob(boolean allowWhileIdle, boolean needNetwork) {
        final Intent scheduleJobIntent = new Intent(TestJobSchedulerReceiver.ACTION_SCHEDULE_JOB);
        scheduleJobIntent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        scheduleJobIntent.putExtra(TestJobSchedulerReceiver.EXTRA_JOB_ID_KEY, mJobId);
        scheduleJobIntent.putExtra(TestJobSchedulerReceiver.EXTRA_ALLOW_IN_IDLE, allowWhileIdle);
        scheduleJobIntent.putExtra(TestJobSchedulerReceiver.EXTRA_REQUIRE_NETWORK_ANY, needNetwork);
        scheduleJobIntent.setComponent(new ComponentName(TEST_APP_PACKAGE, TEST_APP_RECEIVER));
        mContext.sendBroadcast(scheduleJobIntent);
    }

    void startAndKeepTestActivity() {
        startAndKeepTestActivity(false);
    }

    void startAndKeepTestActivity(boolean waitForResume) {
        final Intent testActivity = new Intent();
        testActivity.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        ComponentName testComponentName = new ComponentName(TEST_APP_PACKAGE, TEST_APP_ACTIVITY);
        testActivity.setComponent(testComponentName);
        mContext.startActivity(testActivity);
        if (waitForResume) {
            new WindowManagerStateHelper().waitForActivityState(testComponentName, STATE_RESUMED);
        }
    }

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            Log.d(TAG, "Received action " + intent.getAction());
            switch (intent.getAction()) {
                case ACTION_JOB_STARTED:
                case ACTION_JOB_STOPPED:
                    final JobParameters params = intent.getParcelableExtra(JOB_PARAMS_EXTRA_KEY);
                    Log.d(TAG, "JobId: " + params.getJobId());
                    synchronized (mTestJobStatus) {
                        mTestJobStatus.running = ACTION_JOB_STARTED.equals(intent.getAction());
                        mTestJobStatus.jobId = params.getJobId();
                    }
                    break;
            }
        }
    };

    boolean awaitJobStart(long maxWait) throws Exception {
        return waitUntilTrue(maxWait, () -> {
            synchronized (mTestJobStatus) {
                return (mTestJobStatus.jobId == mJobId) && mTestJobStatus.running;
            }
        });
    }

    boolean awaitJobStop(long maxWait) throws Exception {
        return waitUntilTrue(maxWait, () -> {
            synchronized (mTestJobStatus) {
                return (mTestJobStatus.jobId == mJobId) && !mTestJobStatus.running;
            }
        });
    }

    private boolean waitUntilTrue(long maxWait, Condition condition) throws Exception {
        final long deadLine = SystemClock.uptimeMillis() + maxWait;
        do {
            Thread.sleep(500);
        } while (!condition.isTrue() && SystemClock.uptimeMillis() < deadLine);
        return condition.isTrue();
    }

    private static final class TestJobStatus {
        int jobId;
        boolean running;

        private void reset() {
            running = false;
        }
    }

    private interface Condition {
        boolean isTrue() throws Exception;
    }
}
