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

import android.os.SystemClock;
import android.support.test.uiautomator.UiDevice;
import android.util.Log;
import androidx.test.InstrumentationRegistry;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** An {@link ProcLoadHelper} to check for cpu load in last minute is lesser or equal
 *  to given threshold for a given timeout and collect the cpu load as metric.
 */
public class ProcLoadHelper implements ICollectorHelper<Double> {

    private static final String LOG_TAG = ProcLoadHelper.class.getSimpleName();
    private static final String LOAD_CMD = "cat /proc/loadavg";
    public static final String LAST_MINUTE_LOAD_METRIC_KEY = "proc_loadavg_last_minute";

    private static final Pattern LOAD_OUTPUT_PATTERN = Pattern.compile(
            "(?<LASTMINUTELOAD>.*)\\s.*\\s.*\\s.*\\s.*");

    private double mProcLoadThreshold = 0;
    private long mProcLoadWaitTimeInMs = 0;
    // Default to 500 msecs timeout.
    private long mProcLoadIntervalInMs = 500;
    private double mRecentLoad = 0;
    private UiDevice mDevice;

    /** Wait untill the proc/load reaches below the threshold or timeout expires */
    @Override
    public boolean startCollecting() {
        mRecentLoad = 0;
        long remainingWaitTime = mProcLoadWaitTimeInMs;
        while (true) {
            mRecentLoad = getProcLoadInLastMinute();
            Log.i(LOG_TAG, String.format("Average cpu load in last minute is : %s", mRecentLoad));
            if (mRecentLoad <= mProcLoadThreshold) {
                break;
            } else {
                if (remainingWaitTime <= 0) {
                    Log.i(LOG_TAG, "Timeout because proc/loadavg never went below the threshold.");
                    return false;
                }
                long currWaitTime = (mProcLoadIntervalInMs < remainingWaitTime)
                        ? mProcLoadIntervalInMs : remainingWaitTime;
                Log.d(LOG_TAG, String.format("Waiting for %s msecs", currWaitTime));
                SystemClock.sleep(currWaitTime);
                remainingWaitTime = remainingWaitTime - mProcLoadIntervalInMs;
            }
        }
        return true;
    }

    /** Collect the proc/load_avg last minute cpu load average metric. */
    @Override
    public Map<String, Double> getMetrics() {
        // Adding the last recorded load in the metric that will be reported.
        Map<String, Double> result = new HashMap<>();
        Log.i(LOG_TAG, String.format("proc/loadavg in last minute before test is : %s",
                mRecentLoad));
        result.put(LAST_MINUTE_LOAD_METRIC_KEY, mRecentLoad);
        return result;
    }

    /** Do nothing, because nothing is needed to disable cpuy load avg. */
    @Override
    public boolean stopCollecting() {
        return true;
    }

    /**
     * Parse the last minute cpu load from proc/loadavg
     *
     * @return cpu load in last minute. Returns -1 in case if it is failed to parse.
     */
    private double getProcLoadInLastMinute() {
        try {
            String output = getDevice().executeShellCommand(LOAD_CMD);
            Log.i(LOG_TAG, String.format("Output of proc_loadavg is : %s", output));
            // Output of the load command
            // 1.39 1.10 1.21 2/2679 6380
            // 1.39 is the proc load in the last minute.

            Matcher match = null;
            if ((match = matches(LOAD_OUTPUT_PATTERN, output.trim())) != null) {
                Log.i(LOG_TAG, String.format("Current load is : %s",
                        match.group("LASTMINUTELOAD")));
                return Double.parseDouble(match.group("LASTMINUTELOAD"));
            } else {
                Log.w(LOG_TAG, "Not able to parse the proc/loadavg");
            }
        } catch (IOException e) {
            throw new RuntimeException("Failed to get proc/loadavg.", e);
        }

        return -1;
    }

    /** Returns the {@link UiDevice} under test. */
    private UiDevice getDevice() {
        if (mDevice == null) {
            mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        }
        return mDevice;
    }

    /**
     * Checks whether {@code line} matches the given {@link Pattern}.
     *
     * @return The resulting {@link Matcher} obtained by matching the {@code line} against
     *         {@code pattern}, or null if the {@code line} does not match.
     */
    private static Matcher matches(Pattern pattern, String line) {
        Matcher ret = pattern.matcher(line);
        return ret.matches() ? ret : null;
    }

    /**
     * Sets the threshold value which the device cpu load average should be lesser than or equal.
     */
    public void setProcLoadThreshold(double procLoadThreshold) {
        mProcLoadThreshold = procLoadThreshold;
    }

    /**
     * Sets the timeout in msecs checking for threshold before proceeding with the testing.
     */
    public void setProcLoadWaitTimeInMs(long procLoadWaitTimeInMs) {
        mProcLoadWaitTimeInMs = procLoadWaitTimeInMs;
    }

    /**
     * Sets the interval time in msecs to check continuosly untill the timeout expires.
     */
    public void setProcLoadIntervalInMs(long procLoadIntervalInMs) {
        mProcLoadIntervalInMs = procLoadIntervalInMs;
    }
}
