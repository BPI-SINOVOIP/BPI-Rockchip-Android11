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

import static com.android.helpers.MetricUtility.constructKey;

import android.app.ActivityManager;
import android.app.ActivityManager.RunningAppProcessInfo;
import android.content.Context;
import android.os.Debug.MemoryInfo;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Helper to collect totalpss memory usage per process tracked by the ActivityManager
 * memoryinfo.
 */
public class TotalPssHelper implements ICollectorHelper<Long> {

    private static final String TAG = TotalPssHelper.class.getSimpleName();

    private static final int DEFAULT_THRESHOLD = 1024;
    private static final int DEFAULT_MIN_ITERATIONS = 6;
    private static final int DEFAULT_MAX_ITERATIONS = 20;
    private static final int DEFAULT_SLEEP_TIME = 1000;
    private static final String PSS_METRIC_PREFIX = "am_totalpss_bytes";

    private String[] mProcessNames;
    // Minimum number of iterations needed before deciding on the memory usage.
    private int mMinIterations;
    // Maximum number of iterations needed waiting for memory usage to be stabilized.
    private int mMaxIterations;
    // Sleep time in between the iterations.
    private int mSleepTime;
    // Threshold in kb to use whether the data is stabilized.
    private int mThreshold;
    // Map to maintain the pss memory size.
    private Map<String, Long> mPssFinalMap = new HashMap<>();

    public void setUp(String... processNames) {
        mProcessNames = processNames;
        // Minimum iterations should be atleast 3 to check for the
        // stabilization of the memory usage.
        mMinIterations = DEFAULT_MIN_ITERATIONS;
        mMaxIterations = DEFAULT_MAX_ITERATIONS;
        mSleepTime = DEFAULT_SLEEP_TIME;
        mThreshold = DEFAULT_THRESHOLD;
    }

    @Override
    public boolean startCollecting() {
        return true;
    }

    @Override
    public Map<String, Long> getMetrics() {
        if (mMinIterations < 3) {
            Log.w(TAG, "Need atleast 3 iterations to check memory usage stabilization.");
            return mPssFinalMap;
        }
        if (mProcessNames != null) {
            for (String processName : mProcessNames) {
                if (!processName.isEmpty()) {
                    measureMemory(processName);
                }
            }
        }
        return mPssFinalMap;
    }

    @Override
    public boolean stopCollecting() {
        return true;
    }

    /**
     * Measure memory info of the given process name tracked by the activity manager
     * MemoryInfo(i.e getTotalPss).
     *
     * @param processName to calculate the memory info.
     */
    private void measureMemory(String processName) {
        Log.i(TAG, "Tracking memory usage of the process - " + processName);
        List<Long> pssData = new ArrayList<Long>();
        long pss = 0;
        int iteration = 0;
        while (iteration < mMaxIterations) {
            sleep(mSleepTime);
            pss = getPss(processName);
            pssData.add(pss);
            if (iteration >= mMinIterations && stabilized(pssData)) {
                Log.i(TAG, "Memory usage stabilized at iteration count = " + iteration);
                // Final metric reported in bytes.
                mPssFinalMap.put(constructKey(PSS_METRIC_PREFIX, processName), pss * 1024);
                return;
            }
            iteration++;
        }

        Log.i(TAG, processName + " memory usage did not stabilize."
                + " Returning the average of the pss data collected.");
        // Final metric reported in bytes.
        mPssFinalMap.put(constructKey(PSS_METRIC_PREFIX, processName), average(pssData) * 1024);
    }

    /**
     * Time to sleep in between the iterations.
     *
     * @param time in ms to sleep.
     */
    private void sleep(int time) {
        try {
            Thread.sleep(time);
        } catch (InterruptedException e) {
            // ignore
        }
    }

    /**
     * Get the total pss memory of the given process name.
     *
     * @param processName of the process to measure the memory.
     * @return the memory in KB.
     */
    private long getPss(String processName) {
        ActivityManager am = (ActivityManager) InstrumentationRegistry.getInstrumentation()
                .getContext().getSystemService(Context.ACTIVITY_SERVICE);
        List<RunningAppProcessInfo> apps = am.getRunningAppProcesses();
        for (RunningAppProcessInfo proc : apps) {
            if (!proc.processName.equals(processName)) {
                continue;
            }
            MemoryInfo meminfo = am.getProcessMemoryInfo(new int[] {
                proc.pid
            })[0];
            Log.i(TAG,
                    String.format("Memory usage of process - %s is %d", processName,
                            meminfo.getTotalPss()));
            return meminfo.getTotalPss();
        }
        Log.w(TAG, "Not able to find the process id for the process = " + processName);
        return 0;
    }

    /**
     * Checks whether the memory usage is stabilized by calculating the sum of the difference
     * between the last 3 values and comparing that to the threshold.
     *
     * @param pssData list of pssData of the given process name.
     * @return true if the memory is stabilized.
     */
    private boolean stabilized(List<Long> pssData) {
        long diff1 = Math.abs(pssData.get(pssData.size() - 1) - pssData.get(pssData.size() - 2));
        long diff2 = Math.abs(pssData.get(pssData.size() - 2) - pssData.get(pssData.size() - 3));
        Log.i(TAG, "diff1=" + diff1 + " diff2=" + diff2);
        return (diff1 + diff2) < mThreshold;
    }

    /**
     * Returns the average of the pssData collected for the maxIterations.
     *
     * @param pssData list of pssData.
     * @return
     */
    private long average(List<Long> pssData) {
        long sum = 0;
        for (long sample : pssData) {
            sum += sample;
        }
        return sum / pssData.size();
    }

    /**
     * @param minIterations before starting to check for memory is stabilized.
     */
    public void setMinIterations(int minIterations) {
        mMinIterations = minIterations;
    }

    /**
     * @param maxIterations to wait for memory to be stabilized.
     */
    public void setMaxIterations(int maxIterations) {
        mMaxIterations = maxIterations;
    }

    /**
     * @param sleepTime in between the iterations.
     */
    public void setSleepTime(int sleepTime) {
        mSleepTime = sleepTime;
    }

    /**
     * @param threshold for difference in memory usage between two successive iterations in kb
     */
    public void setThreshold(int threshold) {
        mThreshold = threshold;
    }
}
