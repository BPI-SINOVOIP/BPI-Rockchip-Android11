/*
 * Copyright (C) 2018 The Android Open Source Project
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

import android.icu.text.NumberFormat;
import android.support.test.uiautomator.UiDevice;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;

import java.io.IOException;
import java.text.ParseException;
import java.util.HashMap;
import java.util.InputMismatchException;
import java.util.Map;
import java.util.Scanner;

/**
 * ProcessShowmapHelper is a helper used to sample memory metrics (PSS, RSS, VSS) from
 * showmap before and after a test and return metrics of interest.
 *
 * Example Usage:
 * processShowmapHelper.setUp("com.android.systemui");
 * processShowmapHelper.startCollecting();
 * // Test runs
 * metrics = processShowmapHelper.getMetrics();
 * processShowmapHelper.stopCollecting();
 *
 * TODO(b/119684651) Add support for writing showmap output to file
 */
public class ProcessShowmapHelper implements ICollectorHelper<Long> {
    private static final String TAG = ProcessShowmapHelper.class.getSimpleName();
    // Command to get the showmap for a process
    private static final String SHOWMAP_CMD = "showmap %d";
    // Command to get the process id from the process name
    private static final String PIDOF_CMD = "pidof %s";
    private static final String PSS = "pss";
    private static final String RSS = "rss";
    private static final String VSS = "vss";
    private static final String DELTA = "delta";

    private String[] mProcessNames;
    private ShowmapMetrics[] mTestStartMetrics;
    private ShowmapMetrics[] mTestEndMetrics;
    private UiDevice mUiDevice;

    private static final class ShowmapMetrics {
        long pss;
        long rss;
        long vss;
    }

    /**
     * Sets up the helper before it starts sampling.
     *
     * @param processNames process names to sample
     */
    public void setUp(String... processNames) {
        mProcessNames = processNames;
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
    }

    @Override
    public boolean startCollecting() {
        mTestStartMetrics = sampleMemoryOfProcesses(mProcessNames);
        return mTestStartMetrics != null;
    }

    @Override
    public Map<String, Long> getMetrics() {
        // Collect end sample.
        HashMap<String, Long> showmapFinalMap = new HashMap<>();
        mTestEndMetrics = sampleMemoryOfProcesses(mProcessNames);
        if (mTestEndMetrics == null) {
            Log.e(TAG, "Unable to collect any showmap metrics at end. Returning empty metrics");
            return showmapFinalMap;
        }

        // Iterate over each process and collate start and end sample to build final metrics.
        for (int i = 0; i < mTestEndMetrics.length; i++) {
            String processName = mProcessNames[i];
            ShowmapMetrics endMetrics = mTestEndMetrics[i];
            if (endMetrics == null) {
                // Failed to get end metrics for this process. Continue.
                continue;
            }
            // Calculate and determine final metrics.
            showmapFinalMap.put(constructKey(processName, PSS), endMetrics.pss);
            showmapFinalMap.put(constructKey(processName, RSS), endMetrics.rss);
            showmapFinalMap.put(constructKey(processName, VSS), endMetrics.vss);

            if (mTestStartMetrics == null || mTestStartMetrics[i] == null) {
                // Failed to get start metrics for this process. Continue.
                continue;
            }
            ShowmapMetrics startMetrics = mTestStartMetrics[i];
            showmapFinalMap.put(
                    constructKey(processName, PSS, DELTA), endMetrics.pss - startMetrics.pss);
            showmapFinalMap.put(
                    constructKey(processName, RSS, DELTA), endMetrics.rss - startMetrics.rss);
            showmapFinalMap.put(
                    constructKey(processName, VSS, DELTA), endMetrics.vss - startMetrics.vss);
        }
        return showmapFinalMap;
    }

    @Override
    public boolean stopCollecting() {
        reset();
        return true;
    }

    /**
     * Sample the current memory for a set of processes using showmap.
     *
     * @param processNames the process names to sample
     * @return a list of showmap metrics for each process given in order. May be null if it is not
     *     properly set up.
     */
    private @Nullable ShowmapMetrics[] sampleMemoryOfProcesses(String... processNames) {
        if (processNames == null || mUiDevice == null) {
            Log.e(TAG, "Process names or UI device is null. Make sure you've called setup.");
            return null;
        }
        ShowmapMetrics[] metrics = new ShowmapMetrics[processNames.length];
        for (int i = 0; i < processNames.length; i++) {
            metrics[i] = sampleMemory(processNames[i]);
        }
        return metrics;
    }

    /**
     * Samples the current memory use of the process using showmap. Gets PSS, RSS, and VSS.
     *
     * @return metrics object with pss, rss, and vss
     */
    private @Nullable ShowmapMetrics sampleMemory(@NonNull String processName) {
        // Get pid
        int pid;
        try {
            // Note that only the first pid returned by "pidof" will be used.
            String pidofOutput = mUiDevice.executeShellCommand(
                String.format(PIDOF_CMD, processName));
            pid = NumberFormat.getInstance().parse(pidofOutput).intValue();
        } catch (IOException | ParseException e) {
            Log.e(TAG, String.format("Unable to get pid of %s ", processName), e);
            return null;
        }

        // Read showmap for process
        String showmapOutput;
        try {
            showmapOutput = mUiDevice.executeShellCommand(String.format(SHOWMAP_CMD, pid));
        } catch (IOException e) {
            Log.e(TAG, String.format("Failed to get showmap output for %s ", processName) , e);
            return null;
        }

        ShowmapMetrics metrics = new ShowmapMetrics();

        // Extract VSS, PSS and RSS from the showmap and output them as metrics.
        // The last lines of the showmap output looks something like:
        // CHECKSTYLE:OFF Generated code
        // virtual                     shared   shared  private  private
        //    size      RSS      PSS    clean    dirty    clean    dirty     swap  swapPSS   # object
        //-------- -------- -------- -------- -------- -------- -------- -------- -------- ---- ------------------------------
        //  928480   113016    24860    87348     7916     3632    14120     1968     1968 1900 TOTAL
        // CHECKSTYLE:ON Generated code
        try {
            int pos = showmapOutput.lastIndexOf("----");
            Scanner sc = new Scanner(showmapOutput.substring(pos));
            sc.next();
            metrics.vss = sc.nextLong();
            metrics.rss = sc.nextLong();
            metrics.pss = sc.nextLong();
        } catch (IndexOutOfBoundsException | InputMismatchException e) {
            Log.e(TAG, String.format("Unexpected showmap format for %s ", processName), e);
            return null;
        }
        return metrics;
    }

    /**
     * Resets any intermediate state in the helper for reuse.
     */
    private void reset() {
        mTestStartMetrics = null;
        mTestEndMetrics = null;
    }
}
