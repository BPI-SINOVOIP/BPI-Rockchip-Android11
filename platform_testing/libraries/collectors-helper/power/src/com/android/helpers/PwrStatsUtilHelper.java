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

package com.android.helpers;

import android.support.test.uiautomator.UiDevice;
import android.util.Log;

import androidx.annotation.VisibleForTesting;
import androidx.test.InstrumentationRegistry;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * PwrStatsUtilHelper runs and collects power stats HAL metrics from the Pixel-specific
 * pwrstats_util tool that is already included on the device in userdebug builds.
 */
public class PwrStatsUtilHelper implements ICollectorHelper<Long> {
    private static final String LOG_TAG = PwrStatsUtilHelper.class.getSimpleName();

    private UiDevice mDevice = null;

    @VisibleForTesting protected int mUtilPid = 0;
    @VisibleForTesting protected File mLogFile;

    @Override
    public boolean startCollecting() {
        Log.i(LOG_TAG, "Starting pwrstats_util collection...");
        // Create temp log file for pwrstats_util
        try {
            mLogFile = File.createTempFile("pwrstats_output", ".txt");
        } catch (IOException e) {
            Log.e(LOG_TAG, e.toString());
            return false;
        }
        mLogFile.delete();

        // Kick off pwrstats_util and get its PID
        final String pid_regex = "^pid = (\\d+)$";
        final Pattern pid_pattern = Pattern.compile(pid_regex);
        String output;
        try {
            output = executeShellCommand("pwrstats_util -d " + mLogFile.getAbsolutePath());
        } catch (IOException e) {
            Log.e(LOG_TAG, e.toString());
            return false;
        }
        Matcher m = pid_pattern.matcher(output);
        if (m.find()) {
            mUtilPid = Integer.parseInt(m.group(1));
        }
        return true;
    }

    @Override
    public boolean stopCollecting() {
        Log.i(LOG_TAG, "Ending pwrstats_util collection...");
        try {
            executeShellCommand("kill -INT " + mUtilPid);
        } catch (IOException e) {
            Log.e(LOG_TAG, e.toString());
            return false;
        }
        try {
            Thread.sleep(1000);
        } catch (InterruptedException e) {
            Log.e(LOG_TAG, e.toString());
        }
        return true;
    }

    @Override
    public Map<String, Long> getMetrics() {
        stopCollecting();

        Log.i(LOG_TAG, "Getting metrics...");
        return processMetricsFromLogFile();
    }

    @VisibleForTesting
    /* Execute a shell command and return its output. */
    protected String executeShellCommand(String command) throws IOException {
        if (mDevice == null) {
            mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        }

        Log.i(LOG_TAG, "Running '" + command + "'");
        return mDevice.executeShellCommand(command);
    }

    /* Parse each of the metrics into a key value pair and delete the log file. */
    private Map<String, Long> processMetricsFromLogFile() {
        final String PATTERN = "^(.*)=(\\d+)$";
        final Pattern r = Pattern.compile(PATTERN);
        Map<String, Long> metrics = new HashMap<>();
        BufferedReader br = null;
        try {
            br = new BufferedReader(new FileReader(mLogFile));
            // First line is elapsed time
            String line = br.readLine();
            Matcher m;
            while ((line = br.readLine()) != null) {
                m = r.matcher(line);
                if (m.find()) {
                    Log.i(LOG_TAG, m.group(1) + "=" + m.group(2));
                    metrics.put(m.group(1), Long.parseLong(m.group(2)));
                }
            }
            mLogFile.delete();
        } catch (IOException e) {
            Log.e(LOG_TAG, e.toString());
        }

        try {
            if (br != null) br.close();
        } catch (IOException e) {
            Log.e(LOG_TAG, e.toString());
        }
        return metrics;
    }
}
