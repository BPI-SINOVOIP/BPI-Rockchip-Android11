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

package com.android.nn.dogfood;

import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import com.android.nn.benchmark.core.TestModelsListLoader;
import com.android.nn.benchmark.util.TestExternalStorageActivity;

import java.io.IOException;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "NN_BENCHMARK";
    private static final int JOB_FREQUENCY_MILLIS = 15 * 60 * 1000; // 15 minutes
    private Button mStartStopButton;
    private TextView mMessage;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        TestExternalStorageActivity.testWriteExternalStorage(this, true);

        mStartStopButton = (Button) findViewById(R.id.start_stop_button);
        mMessage = (TextView) findViewById(R.id.message);
        try {
            TestModelsListLoader.parseFromAssets(getAssets());
        } catch (IOException e) {
            Log.e(TAG, "Could not load models", e);
        }
    }

    public void startStopTestClicked(View v) {

        JobScheduler jobScheduler = (JobScheduler) getSystemService(JOB_SCHEDULER_SERVICE);
        if (jobScheduler.getPendingJob(BenchmarkJobService.JOB_ID) == null) {
            // no job is currently scheduled
            ComponentName componentName = new ComponentName(this, BenchmarkJobService.class);
            JobInfo jobInfo =
                    new JobInfo.Builder(BenchmarkJobService.JOB_ID, componentName)
                            .setPeriodic(JOB_FREQUENCY_MILLIS)
                            .build();
            jobScheduler.schedule(jobInfo);
            mMessage.setText("Benchmark job scheduled, you can leave this app");
            mStartStopButton.setText(R.string.stop_button_text);
        } else {
            jobScheduler.cancel(BenchmarkJobService.JOB_ID);
            mMessage.setText("Benchmark job cancelled");
            mStartStopButton.setText(R.string.start_button_text);
        }
    }
}
