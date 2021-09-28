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

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobService;
import android.content.SharedPreferences;
import android.util.Log;

import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;

import com.android.nn.benchmark.core.BenchmarkResult;
import com.android.nn.benchmark.core.Processor;
import com.android.nn.benchmark.core.TestModels;

import java.util.List;
import java.util.Random;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Regularly runs a random selection of the NN API benchmark models */
public class BenchmarkJobService extends JobService implements Processor.Callback {

    private static final String TAG = "NN_BENCHMARK";
    private static final String CHANNEL_ID = "default";
    private static final int NOTIFICATION_ID = 999;
    public static final int JOB_ID = 1;
    private NotificationManagerCompat mNotificationManager;
    private NotificationCompat.Builder mNotification;
    private boolean mJobStopped = false;
    private static final int NUM_RUNS = 10;
    private Processor mProcessor;
    private JobParameters mJobParameters;
    private static final String NN_API_DOGFOOD_PREF = "nn_api_dogfood";

    private static int DOGFOOD_MODELS_PER_RUN = 20;
    private BenchmarkResult mTestResults[];
    private final ExecutorService processorRunner = Executors.newSingleThreadExecutor();


    @Override
    public boolean onStartJob(JobParameters jobParameters) {
        mJobParameters = jobParameters;
        incrementNumRuns();
        Log.d(TAG, String.format("NN API Benchmarking job %d/%d started", getNumRuns(), NUM_RUNS));
        showNotification();
        doBenchmark();

        return true;
    }

    @Override
    public boolean onStopJob(JobParameters jobParameters) {
        Log.d(TAG, String.format("NN API Benchmarking job %d/%d stopped", getNumRuns(), NUM_RUNS));
        mJobStopped = true;

        return false;
    }

    public void doBenchmark() {
        mProcessor = new Processor(this, this, randomModelList());
        mProcessor.setUseNNApi(true);
        mProcessor.setToggleLong(true);
        processorRunner.submit(mProcessor);
    }

    public void onBenchmarkFinish(boolean ok) {
        mProcessor.exit();
        if (getNumRuns() >= NUM_RUNS) {
            mNotification
                    .setProgress(0, 0, false)
                    .setContentText(
                            "Benchmarking done! please upload a bug report via BetterBug under"
                                + " Android > Android OS & > Apps Runtime > Machine Learning")
                    .setOngoing(false);
            mNotificationManager.notify(NOTIFICATION_ID, mNotification.build());
            JobScheduler jobScheduler = getSystemService(JobScheduler.class);
            jobScheduler.cancel(JOB_ID);
            resetNumRuns();
        } else {
            mNotification
                    .setProgress(0, 0, false)
                    .setContentText(
                            String.format(
                                    "Background test %d of %d is complete", getNumRuns(), NUM_RUNS))
                    .setOngoing(false);
            mNotificationManager.notify(NOTIFICATION_ID, mNotification.build());
        }

        Log.d(TAG, "NN API Benchmarking job finished");
        jobFinished(mJobParameters, false);
    }

    public void onStatusUpdate(int testNumber, int numTests, String modelName) {
        Log.d(
                TAG,
                String.format("Benchmark progress %d of %d - %s", testNumber, numTests, modelName));
        mNotification.setProgress(numTests, testNumber, false);
        mNotificationManager.notify(NOTIFICATION_ID, mNotification.build());
    }

    private void showNotification() {
        mNotificationManager = NotificationManagerCompat.from(this);
        NotificationChannel channel =
                new NotificationChannel(CHANNEL_ID, "Default", NotificationManager.IMPORTANCE_LOW);
        mNotificationManager.createNotificationChannel(channel);
        mNotificationManager = NotificationManagerCompat.from(this);
        String title = "NN API Dogfood";
        String msg = String.format("Background test %d of %d is running", getNumRuns(), NUM_RUNS);

        mNotification =
                new NotificationCompat.Builder(this, CHANNEL_ID)
                        .setSmallIcon(R.mipmap.ic_launcher)
                        .setContentTitle(title)
                        .setContentText("NN API Benchmarking Job")
                        .setPriority(NotificationCompat.PRIORITY_MAX)
                        .setOngoing(true);
        mNotificationManager.notify(NOTIFICATION_ID, mNotification.build());
    }

    private int[] randomModelList() {
        long seed = System.currentTimeMillis();
        List<TestModels.TestModelEntry> testList = TestModels.modelsList();

        Log.v(TAG, "Dogfood run seed " + seed);
        Random random = new Random(seed);
        int numModelsToSelect = Math.min(DOGFOOD_MODELS_PER_RUN, testList.size());
        int[] randomModelIndices = new int[numModelsToSelect];

        for (int i = 0; i < numModelsToSelect; i++) {
            randomModelIndices[i] = random.nextInt(testList.size());
        }

        return randomModelIndices;
    }

    private void incrementNumRuns() {
        SharedPreferences.Editor editor =
                getSharedPreferences(NN_API_DOGFOOD_PREF, MODE_PRIVATE).edit();
        editor.putInt("num_runs", getNumRuns() + 1);
        editor.apply();
    }

    private void resetNumRuns() {
        SharedPreferences.Editor editor =
                getSharedPreferences(NN_API_DOGFOOD_PREF, MODE_PRIVATE).edit();
        editor.putInt("num_runs", 0);
        editor.apply();
    }

    private int getNumRuns() {
        SharedPreferences prefs = getSharedPreferences(NN_API_DOGFOOD_PREF, MODE_PRIVATE);
        return prefs.getInt("num_runs", 0);
    }
}
