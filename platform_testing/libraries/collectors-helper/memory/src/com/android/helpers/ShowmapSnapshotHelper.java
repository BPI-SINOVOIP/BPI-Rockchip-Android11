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

import android.support.test.uiautomator.UiDevice;
import android.util.Log;
import androidx.test.InstrumentationRegistry;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.InputMismatchException;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.stream.Collectors;

/**
 * Helper to collect memory information for a list of processes from showmap.
 */
public class ShowmapSnapshotHelper implements ICollectorHelper<String> {
    private static final String TAG = ShowmapSnapshotHelper.class.getSimpleName();

    private static final String DROP_CACHES_CMD = "echo %d > /proc/sys/vm/drop_caches";
    private static final String PIDOF_CMD = "pidof %s";
    public static final String ALL_PROCESSES_CMD = "ps -A";
    private static final String SHOWMAP_CMD = "showmap -v %d";
    private static final String CHILD_PROCESSES_CMD = "ps -A --ppid %d";

    public static final String OUTPUT_METRIC_PATTERN = "showmap_%s_bytes";
    public static final String OUTPUT_FILE_PATH_KEY = "showmap_output_file";
    public static final String PROCESS_COUNT = "process_count";
    public static final String CHILD_PROCESS_COUNT_PREFIX = "child_processes_count";
    public static final String OUTPUT_CHILD_PROCESS_COUNT_KEY = CHILD_PROCESS_COUNT_PREFIX + "_%s";
    public static final String PROCESS_WITH_CHILD_PROCESS_COUNT =
            "process_with_child_process_count";

    private String[] mProcessNames = null;
    private String mTestOutputDir = null;
    private String mTestOutputFile = null;

    private int mDropCacheOption;
    private boolean mCollectForAllProcesses = false;
    private UiDevice mUiDevice;

    // Map to maintain per-process memory info
    private Map<String, String> mMemoryMap = new HashMap<>();

    // Maintain metric name and the index it corresponds to in the showmap output
    // summary
    private Map<Integer, String> mMetricNameIndexMap = new HashMap<>();

    public void setUp(String testOutputDir, String... processNames) {
        mProcessNames = processNames;
        mTestOutputDir = testOutputDir;
        mDropCacheOption = 0;
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
    }

    @Override
    public boolean startCollecting() {
        if (mTestOutputDir == null) {
            Log.e(TAG, String.format("Invalid test setup"));
            return false;
        }

        File directory = new File(mTestOutputDir);
        String filePath = String.format("%s/showmap_snapshot%d.txt", mTestOutputDir,
                UUID.randomUUID().hashCode());
        File file = new File(filePath);

        // Make sure directory exists and file does not
        if (directory.exists()) {
            if (file.exists() && !file.delete()) {
                Log.e(TAG, String.format("Failed to delete result output file %s", filePath));
                return false;
            }
        } else {
            if (!directory.mkdirs()) {
                Log.e(TAG, String.format("Failed to create result output directory %s",
                        mTestOutputDir));
                return false;
            }
        }

        // Create an empty file to fail early in case there are no write permissions
        try {
            if (!file.createNewFile()) {
                // This should not happen unless someone created the file right after we deleted it
                Log.e(TAG,
                        String.format("Race with another user of result output file %s", filePath));
                return false;
            }
        } catch (IOException e) {
            Log.e(TAG, String.format("Failed to create result output file %s", filePath), e);
            return false;
        }

        mTestOutputFile = filePath;
        return true;
    }

    @Override
    public Map<String, String> getMetrics() {
        try {
            // Drop cache if requested
            if (mDropCacheOption > 0) {
                dropCache(mDropCacheOption);
            }

            if (mCollectForAllProcesses) {
                Log.i(TAG, "Collecting memory metrics for all processes.");
                mProcessNames = getAllProcessNames();
            } else if (mProcessNames.length > 0) {
                Log.i(TAG, "Collecting memory only for given list of process");
            } else if (mProcessNames.length == 0) {
                // No processes specified, just return empty map
                return mMemoryMap;
            }

            FileWriter writer = new FileWriter(new File(mTestOutputFile), true);
            for (String processName : mProcessNames) {
                List<Integer> pids = new ArrayList<>();

                // Collect required data
                try {
                    pids = getPids(processName);
                    for (Integer pid : pids) {
                        String showmapOutput = execShowMap(processName, pid);
                        parseAndUpdateMemoryInfo(processName, showmapOutput);
                        // Store showmap output into file. If there are more than one process
                        // with same name write the individual showmap associated with pid.
                        storeToFile(mTestOutputFile, processName, pid, showmapOutput, writer);
                        // Parse number of child processes for the given pid and update the
                        // total number of child process count for the process name that pid
                        // is associated with.
                        updateChildProcessesCount(processName, pid);
                    }
                } catch (RuntimeException e) {
                    Log.e(TAG, e.getMessage(), e.getCause());
                    // Skip this process and continue with the next one
                    continue;
                }
            }
            // To track total number of process with child processes.
            if (mMemoryMap.size() != 0) {
                Set<String> parentWithChildProcessSet = mMemoryMap.keySet()
                        .stream()
                        .filter(s -> s.startsWith(CHILD_PROCESS_COUNT_PREFIX))
                        .collect(Collectors.toSet());
                mMemoryMap.put(PROCESS_WITH_CHILD_PROCESS_COUNT,
                        Long.toString(parentWithChildProcessSet.size()));
            }
            // Store the unique process count. -1 to exclude the "ps" process name.
            mMemoryMap.put(PROCESS_COUNT, Integer.toString(mProcessNames.length - 1));
            writer.close();
            mMemoryMap.put(OUTPUT_FILE_PATH_KEY, mTestOutputFile);
        } catch (RuntimeException e) {
            Log.e(TAG, e.getMessage(), e.getCause());
        } catch (IOException e) {
            Log.e(TAG, String.format("Failed to write output file %s", mTestOutputFile), e);
        }

        return mMemoryMap;
    }

    @Override
    public boolean stopCollecting() {
        return true;
    }

    /**
     * Set drop cache option.
     *
     * @param dropCacheOption drop pagecache (1), slab (2) or all (3) cache
     * @return true on success, false if input option is invalid
     */
    public boolean setDropCacheOption(int dropCacheOption) {
        // Valid values are 1..3
        if (dropCacheOption < 1 || dropCacheOption > 3) {
            return false;
        }

        mDropCacheOption = dropCacheOption;
        return true;
    }

    /**
     * Drops kernel memory cache.
     *
     * @param cacheOption drop pagecache (1), slab (2) or all (3) caches
     */
    private void dropCache(int cacheOption) throws RuntimeException {
        try {
            mUiDevice.executeShellCommand(String.format(DROP_CACHES_CMD, cacheOption));
        } catch (IOException e) {
            throw new RuntimeException("Unable to drop caches", e);
        }
    }

    /**
     * Get pid's of the process with {@code processName} name.
     *
     * @param processName name of the process to get pid
     * @return pid's of the specified process
     */
    private List<Integer> getPids(String processName) throws RuntimeException {
        try {
            String pidofOutput = mUiDevice
                    .executeShellCommand(String.format(PIDOF_CMD, processName));

            // Sample output for the process with more than 1 pid.
            // Sample command : "pidof init"
            // Sample output : 1 559
            String[] pids = pidofOutput.split("\\s+");
            List<Integer> pidList = new ArrayList<>();
            for (String pid : pids) {
                pidList.add(Integer.parseInt(pid.trim()));
            }
            return pidList;
        } catch (IOException e) {
            throw new RuntimeException(String.format("Unable to get pid of %s ", processName), e);
        }
    }

    /**
     * Executes showmap command for the process with {@code processName} name and {@code pid} pid.
     *
     * @param processName name of the process to run showmap for
     * @param pid pid of the process to run showmap for
     * @return the output of showmap command
     */
    private String execShowMap(String processName, long pid) throws IOException {
        try {
            return mUiDevice.executeShellCommand(String.format(SHOWMAP_CMD, pid));
        } catch (IOException e) {
            throw new RuntimeException(
                    String.format("Unable to execute showmap command for %s ", processName), e);
        }
    }

    /**
     * Extract memory metrics from showmap command output for the process with {@code processName}
     * name.
     *
     * @param processName name of the process to extract memory info for
     * @param showmapOutput showmap command output
     */
    private void parseAndUpdateMemoryInfo(String processName, String showmapOutput)
            throws RuntimeException {
        try {

            // -------- -------- -------- -------- -------- -------- -------- -------- ----- ------
            // ----
            // virtual shared shared private private
            // size RSS PSS clean dirty clean dirty swap swapPSS flags object
            // ------- -------- -------- -------- -------- -------- -------- -------- ------ -----
            // ----
            // 10810272 5400 1585 3800 168 264 1168 0 0 TOTAL

            int pos = showmapOutput.lastIndexOf("----");
            String summarySplit[] = showmapOutput.substring(pos).trim().split("\\s+");

            for (Map.Entry<Integer, String> entry : mMetricNameIndexMap.entrySet()) {
                String metricKey = constructKey(
                        String.format(OUTPUT_METRIC_PATTERN, entry.getValue()),
                        processName);
                // If there are multiple pids associated with the process name then update the
                // existing entry in the map otherwise add new entry in the map.
                if (mMemoryMap.containsKey(metricKey)) {
                    long currValue = Long.parseLong(mMemoryMap.get(metricKey));
                    mMemoryMap.put(metricKey, Long.toString(currValue +
                            (Long.parseLong(summarySplit[entry.getKey() + 1]) * 1024)));
                } else {
                    mMemoryMap.put(metricKey, Long.toString(Long.parseLong(
                            summarySplit[entry.getKey() + 1]) * 1024));
                }
            }
        } catch (IndexOutOfBoundsException | InputMismatchException e) {
            throw new RuntimeException(
                    String.format("Unexpected showmap format for %s ", processName), e);
        }
    }

    /**
     * Store test results for one process into file.
     *
     * @param fileName name of the file being written
     * @param processName name of the process
     * @param pid pid of the process
     * @param showmapOutput showmap command output
     * @param writer file writer to write the data
     */
    private void storeToFile(String fileName, String processName, long pid, String showmapOutput,
            FileWriter writer) throws RuntimeException {
        try {
            writer.write(String.format(">>> %s (%d) <<<\n", processName, pid));
            writer.write(showmapOutput);
            writer.write('\n');
        } catch (IOException e) {
            throw new RuntimeException(String.format("Unable to write file %s ", fileName), e);
        }
    }

    /**
     * Set the memory metric name and corresponding index to parse from the showmap output summary.
     *
     * @param metricNameIndexStr comma separated metric_name:index TODO: Pre-process the string into
     *            map and pass the map to this method.
     */
    public void setMetricNameIndex(String metricNameIndexStr) {
        Log.i(TAG, String.format("Metric Name index %s", metricNameIndexStr));
        String metricDetails[] = metricNameIndexStr.split(",");
        for (String metricDetail : metricDetails) {
            String metricDetailsSplit[] = metricDetail.split(":");
            if (metricDetailsSplit.length == 2) {
                mMetricNameIndexMap.put(Integer.parseInt(
                        metricDetailsSplit[1]), metricDetailsSplit[0]);
            }
        }
        Log.i(TAG, String.format("Metric Name index map size %s", mMetricNameIndexMap.size()));
    }

    /**
     * Retrieves the number of child processes for the given process id and updates the total
     * process count for the process name that pid is associated with.
     *
     * @param processName
     * @param pid
     */
    private void updateChildProcessesCount(String processName, long pid) {
        try {
            Log.i(TAG,
                    String.format("Retrieving child processes count for process name: %s with"
                            + " process id %d.", processName, pid));
            String childProcessesStr = mUiDevice
                    .executeShellCommand(String.format(CHILD_PROCESSES_CMD, pid));
            Log.i(TAG, String.format("Child processes cmd output: %s", childProcessesStr));
            String[] childProcessStrSplit = childProcessesStr.split("\\n");
            // To discard the header line in the command output.
            int childProcessCount = childProcessStrSplit.length - 1;
            String childCountMetricKey = String.format(OUTPUT_CHILD_PROCESS_COUNT_KEY, processName);

            if (childProcessCount > 0) {
                mMemoryMap.put(childCountMetricKey,
                        Long.toString(
                                Long.parseLong(mMemoryMap.getOrDefault(childCountMetricKey, "0"))
                                        + childProcessCount));
            }
        } catch (IOException e) {
            throw new RuntimeException("Unable to run child process count command.", e);
        }
    }

    /**
     * Enables memory collection for all processes.
     */
    public void setAllProcesses() {
        mCollectForAllProcesses = true;
    }

    /**
     * Get all process names running in the system.
     */
    private String[] getAllProcessNames() {
        Set<String> allProcessNames = new LinkedHashSet<>();
        try {
            String psOutput = mUiDevice.executeShellCommand(ALL_PROCESSES_CMD);
            // Split the lines
            String allProcesses[] = psOutput.split("\\n");
            for (String invidualProcessDetails : allProcesses) {
                Log.i(TAG, String.format("Process detail: %s", invidualProcessDetails));
                // Sample process detail line
                // system 603 1 41532 5396 SyS_epoll+ 0 S servicemanager
                String processSplit[] = invidualProcessDetails.split("\\s+");
                // Parse process name
                String processName = processSplit[processSplit.length - 1].trim();
                // Include the process name which are not enclosed in [].
                if (!processName.startsWith("[") && !processName.endsWith("]")) {
                    // Skip the first (i.e header) line from "ps -A" output.
                    if (processName.equalsIgnoreCase("NAME")) {
                        continue;
                    }
                    Log.i(TAG, String.format("Including the process %s", processName));
                    allProcessNames.add(processName);
                }
            }
        } catch (IOException ioe) {
            throw new RuntimeException(
                    String.format("Unable execute all processes command %s ", ALL_PROCESSES_CMD),
                    ioe);
        }
        return allProcessNames.toArray(new String[0]);
    }
}
