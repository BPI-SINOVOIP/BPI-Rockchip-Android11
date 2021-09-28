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

package android.net.wifi.cts.app;

import android.app.Activity;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.os.Bundle;
import android.os.Parcel;
import android.os.ResultReceiver;
import android.util.Log;

/**
 * An activity that can be used to schedule a job inside the app.
 *
 * Note: This trampoline is needed since we cannot schedule a job to an app from outside the app.
 */
public class ScheduleJobActivity extends Activity {
    private static final String TAG = "ScheduleJobActivity";
    public static final int JOB_ID = 1;
    private static final String RESULT_RECEIVER_EXTRA =
            "android.net.wifi.cts.app.extra.RESULT_RECEIVER";
    private static final String SERVICE_COMPONENT_EXTRA =
            "android.net.wifi.cts.app.extra.SERVICE_COMPONENT";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ComponentName serviceComponentName =
                getIntent().getParcelableExtra(SERVICE_COMPONENT_EXTRA);
        ResultReceiver resultReceiver = getIntent().getParcelableExtra(RESULT_RECEIVER_EXTRA);

        Bundle bundle = new Bundle();
        bundle.putParcelable(RESULT_RECEIVER_EXTRA, resultReceiver);
        JobInfo jobInfo = new JobInfo.Builder(JOB_ID, serviceComponentName)
                .setTransientExtras(bundle)
                .setMinimumLatency(1)
                .setOverrideDeadline(1)
                .build();
        JobScheduler jobScheduler = getSystemService(JobScheduler.class);
        jobScheduler.schedule(jobInfo);

        Log.v(TAG,"Job scheduled: " + jobInfo);

        finish();
    }
}
