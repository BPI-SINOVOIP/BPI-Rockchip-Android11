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

import android.app.UiAutomation;
import android.support.test.uiautomator.UiDevice;
import android.util.Log;

import androidx.annotation.VisibleForTesting;
import androidx.test.InstrumentationRegistry;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.regex.Pattern;

/** An {@link ICollectorHelper} for collecting binder metrics for all or a list of processes. */
public class BinderCollectionHelper implements ICollectorHelper<Integer> {

    /** Prefix for all output metrics that come from the trace-ipc dump. */
    @VisibleForTesting static final String BINDER_METRICS_PREFIX = "trace-ipc";
    /** Shell commands to start and stop collecting the binder metrics. */
    @VisibleForTesting static final String TRACE_IPC_COMMAND_START = "am trace-ipc start";

    @VisibleForTesting
    static final String TRACE_IPC_COMMAND_STOP = "am trace-ipc stop --dump-file %s";
    /** Path to the ipc metrics file that will be dumped after trace stopped. */
    @VisibleForTesting static final String TRACE_FILE_PATH = "/data/local/tmp/ipc-trace.txt";

    @VisibleForTesting static final String COPY_COMMAND = "cp %s %s";
    @VisibleForTesting static final String GET_RETURN_CODE_COMMAND = "echo $?";
    @VisibleForTesting static final String SUCCESS_RETURN_CODE = "0";
    /** Prefix of process name line in trace file. */
    @VisibleForTesting static final String TRACE_FOR_PROCESS_PREFIX = "Traces for process: ";
    /** Pattern of the process name line in trace file. */
    @VisibleForTesting
    static final Pattern TRACE_FOR_PROCESS_PATTERN = Pattern.compile("Traces for process: .*");
    /** Prefix of count line in trace file. */
    @VisibleForTesting static final String COUNT_PREFIX = "Count: ";
    /** Pattern of the count line in trace file. */
    @VisibleForTesting static final Pattern COUNT_PATTERN = Pattern.compile("Count: (\\d+).*");

    private static final String LOG_TAG = BinderCollectionHelper.class.getSimpleName();
    private Set<String> mTrackedProcesses = new HashSet<>();
    private UiDevice mDevice;
    private UiAutomation uiAutomation;

    /** Start to trace IPC by executing {@link TRACE_IPC_COMMAND_START}. */
    @Override
    public boolean startCollecting() {
        try {
            getUiAutomation().executeShellCommand(TRACE_IPC_COMMAND_START).checkError();
        } catch (IOException e) {
            throw new RuntimeException("Start IPC tracing failed.", e);
        }
        Log.i(LOG_TAG, "IPC tracing starts.");
        return true;
    }

    /** Collect the binder metrics for tracked processes (or all, if unspecified). */
    @Override
    public Map<String, Integer> getMetrics() {
        Map<String, Integer> result = new HashMap<>();
        try {
            getUiAutomation()
                    .executeShellCommand(String.format(TRACE_IPC_COMMAND_STOP, TRACE_FILE_PATH))
                    .checkError();

            File file = File.createTempFile("temp-ipc-tracing-dump", ".txt");
            String cpOutput =
                    getDevice()
                            .executeShellCommand(
                                    String.format(
                                            COPY_COMMAND, TRACE_FILE_PATH, file.getAbsolutePath()));
            Log.i(
                    LOG_TAG,
                    String.format(
                            "Temp tracing file: %s, size is %d",
                            file.getAbsolutePath(), file.length()));
            BufferedReader reader = new BufferedReader(new FileReader(file));
            parseMetrics(reader, result);
            file.delete();
        } catch (FileNotFoundException e) {
            throw new RuntimeException(
                    String.format("Trace file %s not found.", TRACE_FILE_PATH), e);
        } catch (IOException e) {
            throw new RuntimeException("Get Binder metrics failed.", e);
        }
        return result;
    }

    /** Do nothing, because already been stopped in {@code getMetrics}. */
    @Override
    public boolean stopCollecting() {
        return true;
    }

    /** Add a process or list of processes to be tracked. */
    public void addTrackedProcesses(String... processes) {
        Collections.addAll(mTrackedProcesses, processes);
    }

    /** Returns the {@link UiDevice} under test. */
    @VisibleForTesting
    protected UiDevice getDevice() {
        if (mDevice == null) {
            mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        }
        return mDevice;
    }

    protected UiAutomation getUiAutomation() {
        if (uiAutomation == null) {
            uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        }
        return uiAutomation;
    }

    /** Find the next counter line and return the count. */
    private int getNextCounter(BufferedReader reader) throws IOException {
        String line;
        while ((line = reader.readLine()) != null) {
            if (TRACE_FOR_PROCESS_PATTERN.matcher(line).matches()) {
                break;
            }
            if (COUNT_PATTERN.matcher(line).matches()) {
                return Integer.valueOf(
                        line.substring(line.indexOf(COUNT_PREFIX) + COUNT_PREFIX.length()));
            }
        }
        return -1;
    }

    /** Parse binder metrics, process name to count pair, into {@code result} map. */
    @VisibleForTesting
    void parseMetrics(BufferedReader reader, Map<String, Integer> result) throws IOException {
        String line;
        while ((line = reader.readLine()) != null) {
            if (TRACE_FOR_PROCESS_PATTERN.matcher(line).matches()) {
                String currentProcess =
                        line.substring(
                                line.indexOf(TRACE_FOR_PROCESS_PREFIX)
                                        + TRACE_FOR_PROCESS_PREFIX.length());
                if (mTrackedProcesses.isEmpty() || mTrackedProcesses.contains(currentProcess)) {
                    Log.i(LOG_TAG, String.format("Found tracking process: %s", currentProcess));
                    int nextCount = 0;
                    int totalCount = 0;
                    while ((nextCount = getNextCounter(reader)) > 0) {
                        totalCount += nextCount;
                    }
                    result.put(currentProcess, totalCount);
                }
            }
        }
    }
}
