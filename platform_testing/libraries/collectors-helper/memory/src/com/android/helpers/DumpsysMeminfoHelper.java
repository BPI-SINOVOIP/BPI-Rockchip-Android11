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
 * limitations under the License
 */

package com.android.helpers;

import android.app.UiAutomation;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.android.server.am.nano.MemInfoDumpProto;

import com.google.protobuf.nano.InvalidProtocolBufferNanoException;

import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;

/**
 * This is a collector helper to use adb "dumpsys meminfo -a --proto" command to get important
 * memory metrics like PSS, Shared Dirty, Private Dirty, e.t.c. for the specified packages.
 */
public class DumpsysMeminfoHelper implements ICollectorHelper<Long> {

    private static final String TAG = DumpsysMeminfoHelper.class.getSimpleName();

    private static final String DUMPSYS_MEMINFO_CMD = "dumpsys meminfo -a --proto %s";

    private static final String METRIC_SOURCE = "dumpsys";
    private static final String METRIC_UNIT = "kb";

    // The metric names corresponding to the columns in the output of "dumpsys meminfo -a" command
    private static final String PSS_TOTAL = "pss_total";
    private static final String SHARED_DIRTY = "shared_dirty";
    private static final String PRIVATE_DIRTY = "private_dirty";
    private static final String HEAP_SIZE = "heap_size";
    private static final String HEAP_ALLOC = "heap_alloc";

    // Metric category names, which are the names of the heaps
    private static final String NATIVE_HEAP = "native";
    private static final String DALVIK_HEAP = "dalvik";
    private static final String TOTAL_HEAP = "total";

    private String[] mProcessNames = {};
    private UiAutomation mUiAutomation;

    public void setUp(String... processNames) {
        if (processNames == null) {
            return;
        }
        mProcessNames = processNames;
    }

    @Override
    public boolean startCollecting() {
        mUiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        return true;
    }

    @Override
    public Map<String, Long> getMetrics() {
        Map<String, Long> metrics = new HashMap<>();
        for (String processName : mProcessNames) {
            byte[] rawOutput = getRawDumpsysMeminfo(processName);
            if (rawOutput == null) {
                Log.e(TAG, "Missing meminfo output for process " + processName);
                continue;
            }
            metrics.putAll(parseMetrics(processName, rawOutput));
        }
        return metrics;
    }

    @Override
    public boolean stopCollecting() {
        return true;
    }

    private byte[] getRawDumpsysMeminfo(String processName) {
        if (processName == null || processName.isEmpty()) {
            return null;
        }
        final String cmd = String.format(DUMPSYS_MEMINFO_CMD, processName);
        ParcelFileDescriptor pfd = mUiAutomation.executeShellCommand(cmd);
        try (FileInputStream fis = new ParcelFileDescriptor.AutoCloseInputStream(pfd)) {
            return readInputStreamFully(fis);
        } catch (IOException e) {
            Log.e(TAG, "Failed to execute command. " + cmd, e);
            return null;
        }
    }

    private Map<String, Long> parseMetrics(String processName, byte[] rawOutput) {
        Map<String, Long> metrics = new HashMap<>();
        try {
            MemInfoDumpProto memInfo = MemInfoDumpProto.parseFrom(rawOutput);
            MemInfoDumpProto.ProcessMemory processMemory = findProcessMemory(memInfo, processName);
            if (processMemory != null) {
                putHeapMetrics(metrics, processMemory.nativeHeap, NATIVE_HEAP, processName);
                putHeapMetrics(metrics, processMemory.dalvikHeap, DALVIK_HEAP, processName);
                putHeapMetric(
                        metrics,
                        processMemory.totalHeap.memInfo.totalPssKb,
                        TOTAL_HEAP,
                        PSS_TOTAL,
                        processName);
            }
        } catch (InvalidProtocolBufferNanoException ex) {
            Log.e(TAG, "Invalid protobuf obtained from `dumpsys meminfo --proto`", ex);
        }
        return metrics;
    }

    /** Find ProcessMemory by name. Looks in app and native process. Returns null on failure. */
    private static MemInfoDumpProto.ProcessMemory findProcessMemory(
            MemInfoDumpProto memInfo, String processName) {
        // Look in app processes first.
        for (MemInfoDumpProto.AppData appData : memInfo.appProcesses) {
            if (appData.processMemory == null) {
                continue;
            }
            if (processName.equals(appData.processMemory.processName)) {
                return appData.processMemory;
            }
        }
        // If not found yet, then look in native processes.
        for (MemInfoDumpProto.ProcessMemory procMem : memInfo.nativeProcesses) {
            if (processName.equals(procMem.processName)) {
                return procMem;
            }
        }
        return null;
    }

    private static void putHeapMetrics(
            Map<String, Long> metrics,
            MemInfoDumpProto.ProcessMemory.HeapInfo heapInfo,
            String heapName,
            String processName) {
        if (heapInfo == null || heapInfo.memInfo == null) {
            return;
        }
        putHeapMetric(metrics, heapInfo.memInfo.totalPssKb, heapName, PSS_TOTAL, processName);
        putHeapMetric(metrics, heapInfo.memInfo.sharedDirtyKb, heapName, SHARED_DIRTY, processName);
        putHeapMetric(
                metrics, heapInfo.memInfo.privateDirtyKb, heapName, PRIVATE_DIRTY, processName);
        putHeapMetric(metrics, heapInfo.heapSizeKb, heapName, HEAP_SIZE, processName);
        putHeapMetric(metrics, heapInfo.heapAllocKb, heapName, HEAP_ALLOC, processName);
    }

    private static void putHeapMetric(
            Map<String, Long> metrics,
            long value,
            String heapName,
            String metricName,
            String processName) {
        metrics.put(
                MetricUtility.constructKey(
                        METRIC_SOURCE, heapName, metricName, METRIC_UNIT, processName),
                value);
    }

    /** Copied from {@link com.android.compatibility.common.util.FileUtils#readInputStreamFully}. */
    private static byte[] readInputStreamFully(InputStream is) {
        ByteArrayOutputStream os = new ByteArrayOutputStream();
        byte[] buffer = new byte[32768];
        int count;
        try {
            while ((count = is.read(buffer)) != -1) {
                os.write(buffer, 0, count);
            }
            is.close();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
        return os.toByteArray();
    }
}
