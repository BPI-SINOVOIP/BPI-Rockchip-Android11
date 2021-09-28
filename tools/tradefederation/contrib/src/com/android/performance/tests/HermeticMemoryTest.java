/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.performance.tests;

import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.util.ProcessInfo;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.StreamUtil;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import org.junit.Assert;

import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Test to gather post launch memory details after launching app that include app memory usage and
 * system memory usage
 */
public class HermeticMemoryTest implements IDeviceTest, IRemoteTest {

    private static final String AM_START = "am start -n %s";
    private static final String AM_BROADCAST = "am broadcast -a %s -n %s %s";
    private static final String PROC_MEMINFO = "cat /proc/meminfo";
    private static final String CACHED_PROCESSES =
            "dumpsys meminfo|awk '/Total PSS by category:"
                    + "/{found=0} {if(found) print} /: Cached/{found=1}'|tr -d ' '";
    private static final Pattern PID_PATTERN = Pattern.compile("^.*pid(?<processid>[0-9]*).*$");
    private static final String DUMPSYS_PROCESS = "dumpsys meminfo %s";
    private static final String DUMPSYS_MEMINFO = "dumpsys meminfo -a ";
    private static final String MAPS_INFO = "cat /proc/%d/maps";
    private static final String SMAPS_INFO = "cat /proc/%d/smaps";
    private static final String STATUS_INFO = "cat /proc/%d/status";
    private static final String NATIVE_HEAP = "Native";
    private static final String DALVIK_HEAP = "Dalvik";
    private static final String HEAP = "Heap";
    private static final String MEMTOTAL = "MemTotal";
    private static final String MEMFREE = "MemFree";
    private static final String CACHED = "Cached";
    private static final int NO_PROCESS_ID = -1;
    private static final String DROP_CACHE = "echo 3 > /proc/sys/vm/drop_caches";
    private static final String SEPARATOR = "\\s+";
    private static final String LINE_SEPARATOR = "\\n";
    private static final String MEM_AVAIL_PATTERN = "^MemAvailable.*";
    private static final String MEM_TOTAL = "^\\s+TOTAL\\s+.*";
    private static final String FLOAT_DATA = "^([+-]?(\\d+\\.)?\\d+)$";

    @Option(
            name = "post-app-launch-delay",
            description = "The delay, between the app launch and the meminfo dump",
            isTimeVal = true)
    private long mPostAppLaunchDelay = 60;

    @Option(name = "component-name", description = "package/activity name to launch the activity")
    private String mComponentName = new String();

    @Option(name = "intent-action", description = "intent action to broadcast")
    private String mIntentAction = new String();

    @Option(name = "intent-params", description = "intent parameters")
    private String mIntentParams = new String();

    @Option(name = "total-memory-kb", description = "Built in total memory of the device")
    private long mTotalMemory = 0;

    @Option(
            name = "reporting-key",
            description =
                    "Reporting key is the unique identifier"
                            + "used to report data in the dashboard.")
    private String mRuKey = "";

    private ITestDevice mTestDevice = null;
    private ITestInvocationListener mlistener = null;
    private Map<String, String> mMetrics = new HashMap<>();

    @Override
    public void run(ITestInvocationListener listener) throws DeviceNotAvailableException {
        mlistener = listener;

        calculateFreeMem();

        String preMemInfo = mTestDevice.executeShellCommand(PROC_MEMINFO);

        if (!preMemInfo.isEmpty()) {

            uploadLogFile(preMemInfo, "BeforeLaunchProcMemInfo");
        } else {
            CLog.e("Not able to collect the /proc/meminfo before launching app");
        }

        Assert.assertTrue(
                "Device built in memory in kb is mandatory.Use --total-memory-kb value"
                        + "command line parameter",
                mTotalMemory != 0);
        RunUtil.getDefault().sleep(5000);
        mTestDevice.executeShellCommand(DROP_CACHE);
        RunUtil.getDefault().sleep(5000);
        Assert.assertTrue(
                "Not a valid component name to start the activity",
                (mComponentName.split("/").length == 2));
        if (mIntentAction.isEmpty()) {
            mTestDevice.executeShellCommand(String.format(AM_START, mComponentName));
        } else {
            mTestDevice.executeShellCommand(
                    String.format(AM_BROADCAST, mIntentAction, mComponentName, mIntentParams));
        }

        RunUtil.getDefault().sleep(mPostAppLaunchDelay);
        String postMemInfo = mTestDevice.executeShellCommand(PROC_MEMINFO);
        int processId = getProcessId();
        String dumpsysMemInfo =
                mTestDevice.executeShellCommand(String.format("%s %d", DUMPSYS_MEMINFO, processId));
        String mapsInfo = mTestDevice.executeShellCommand(String.format(MAPS_INFO, processId));
        String sMapsInfo = mTestDevice.executeShellCommand(String.format(SMAPS_INFO, processId));
        String statusInfo = mTestDevice.executeShellCommand(String.format(STATUS_INFO, processId));

        if (!postMemInfo.isEmpty()) {
            uploadLogFile(postMemInfo, "AfterLaunchProcMemInfo");
            parseProcInfo(postMemInfo);
        } else {
            CLog.e("Not able to collect the proc/meminfo after launching app");
        }

        if (NO_PROCESS_ID == processId) {
            CLog.e("Process Id not found for the activity launched");
        } else {
            if (!dumpsysMemInfo.isEmpty()) {
                uploadLogFile(dumpsysMemInfo, String.format("DumpsysMemInfo_%s", mComponentName));
                parseDumpsysInfo(dumpsysMemInfo);
            } else {
                CLog.e("Not able to collect the Dumpsys meminfo after launching app");
            }
            if (!mapsInfo.isEmpty()) {
                uploadLogFile(mapsInfo, "mapsInfo");
            } else {
                CLog.e("Not able to collect maps info after launching app");
            }
            if (!sMapsInfo.isEmpty()) {
                uploadLogFile(sMapsInfo, "smapsInfo");
            } else {
                CLog.e("Not able to collect smaps info after launching app");
            }
            if (!statusInfo.isEmpty()) {
                uploadLogFile(statusInfo, "statusInfo");
            } else {
                CLog.e("Not able to collect status info after launching app");
            }
        }

        reportMetrics(listener, mRuKey, mMetrics);
    }

    /**
     * Method to get the process id of the target package/activity name
     *
     * @return processId of the activity launched
     * @throws DeviceNotAvailableException
     */
    private int getProcessId() throws DeviceNotAvailableException {
        String pkgActivitySplit[] = mComponentName.split("/");
        if (pkgActivitySplit[0] != null) {
            ProcessInfo processData = mTestDevice.getProcessByName(pkgActivitySplit[0]);
            if (null != processData) {
                return processData.getPid();
            }
        }
        return NO_PROCESS_ID;
    }

    /**
     * Method to write the data to test logs.
     *
     * @param data
     * @param fileName
     */
    private void uploadLogFile(String data, String fileName) {
        ByteArrayInputStreamSource inputStreamSrc = null;
        try {
            inputStreamSrc = new ByteArrayInputStreamSource(data.getBytes());
            mlistener.testLog(fileName, LogDataType.TEXT, inputStreamSrc);
        } finally {
            StreamUtil.cancel(inputStreamSrc);
        }
    }

    /** Method to parse dalvik and heap info for launched app */
    private void parseDumpsysInfo(String dumpInfo) {
        String line[] = dumpInfo.split(LINE_SEPARATOR);
        for (int lineCount = 0; lineCount < line.length; lineCount++) {
            String dataSplit[] = line[lineCount].trim().split(SEPARATOR);
            if ((dataSplit[0].equalsIgnoreCase(NATIVE_HEAP) && dataSplit[1].equalsIgnoreCase(HEAP))
                    || (dataSplit[0].equalsIgnoreCase(DALVIK_HEAP)
                            && dataSplit[1].equalsIgnoreCase(HEAP))
                    || dataSplit[0].equalsIgnoreCase("Total")) {
                if (dataSplit.length > 10) {
                    if (dataSplit[0].contains(NATIVE_HEAP) || dataSplit[0].contains(DALVIK_HEAP)) {
                        mMetrics.put(dataSplit[0] + ":PSS_TOTAL", dataSplit[2]);
                        mMetrics.put(dataSplit[0] + ":SHARED_DIRTY", dataSplit[4]);
                        mMetrics.put(dataSplit[0] + ":PRIVATE_DIRTY", dataSplit[5]);
                        mMetrics.put(dataSplit[0] + ":HEAP_TOTAL", dataSplit[10]);
                        mMetrics.put(dataSplit[0] + ":HEAP_ALLOC", dataSplit[11]);
                    } else if (dataSplit[1].matches(FLOAT_DATA)) {
                        mMetrics.put(dataSplit[0] + ":PSS", dataSplit[1]);
                    }
                }
            }
        }
    }

    /** Method to parse the system memory details */
    private void parseProcInfo(String memInfo) {
        String lineSplit[] = memInfo.split(LINE_SEPARATOR);
        long memTotal = 0;
        long memFree = 0;
        long cached = 0;
        for (int lineCount = 0; lineCount < lineSplit.length; lineCount++) {
            String line = lineSplit[lineCount].replace(":", "").trim();
            String dataSplit[] = line.split(SEPARATOR);
            if (dataSplit[0].equalsIgnoreCase(MEMTOTAL)
                    || dataSplit[0].equalsIgnoreCase(MEMFREE)
                    || dataSplit[0].equalsIgnoreCase(CACHED)) {
                if (dataSplit[0].equalsIgnoreCase(MEMTOTAL)) {
                    memTotal = Long.parseLong(dataSplit[1]);
                }
                if (dataSplit[0].equalsIgnoreCase(MEMFREE)) {
                    memFree = Long.parseLong(dataSplit[1]);
                }
                if (dataSplit[0].equalsIgnoreCase(CACHED)) {
                    cached = Long.parseLong(dataSplit[1]);
                }
                mMetrics.put("System_" + dataSplit[0], dataSplit[1]);
            }
        }
        mMetrics.put("System_Kernel_Firmware", String.valueOf((mTotalMemory - memTotal)));
        mMetrics.put("System_Framework_Apps", String.valueOf((memTotal - (memFree + cached))));
    }

    /**
     * Method to parse the free memory based on total memory available from proc/meminfo and private
     * dirty and private clean information of the cached processes from dumpsys meminfo.
     */
    private void calculateFreeMem() throws DeviceNotAvailableException {
        String memInfo = mTestDevice.executeShellCommand(PROC_MEMINFO);
        uploadLogFile(memInfo, "proc_meminfo_In_CacheProcDirty");
        Pattern p = Pattern.compile(MEM_AVAIL_PATTERN, Pattern.MULTILINE);
        Matcher m = p.matcher(memInfo);
        String memAvailable[] = null;
        if (m.find()) {
            memAvailable = m.group(0).split(SEPARATOR);
        }
        int cacheProcDirty = Integer.parseInt(memAvailable[1]);

        String cachedProcesses = mTestDevice.executeShellCommand(CACHED_PROCESSES);
        String processes[] = cachedProcesses.split("\\n{2}")[0].split(LINE_SEPARATOR);
        StringBuilder processesDumpsysInfo = new StringBuilder();
        for (String process : processes) {
            Matcher match = null;
            if ((match = matches(PID_PATTERN, process)) != null) {
                String processId = match.group("processid");
                processesDumpsysInfo.append(
                        String.format("Process Name : %s - PID : %s", process, processId));
                processesDumpsysInfo.append("\n");
                String processInfoStr =
                        mTestDevice.executeShellCommand(String.format(DUMPSYS_PROCESS, processId));
                processesDumpsysInfo.append(processInfoStr);
                processesDumpsysInfo.append("\n");
                Pattern p1 = Pattern.compile(MEM_TOTAL, Pattern.MULTILINE);
                Matcher m1 = p1.matcher(processInfoStr);
                String processInfo[] = null;
                if (m1.find()) {
                    processInfo = m1.group(0).split(LINE_SEPARATOR);
                }
                if (null != processInfo && processInfo.length > 0) {
                    String procDetails[] = processInfo[0].trim().split(SEPARATOR);
                    cacheProcDirty =
                            cacheProcDirty
                                    + Integer.parseInt(procDetails[2].trim())
                                    + Integer.parseInt(procDetails[3]);
                }
            }
        }
        uploadLogFile(processesDumpsysInfo.toString(), "ProcessesDumpsysInfo_In_CacheProcDirty");
        mMetrics.put("MemAvailable_CacheProcDirty", String.valueOf(cacheProcDirty));
    }

    /**
     * Report run metrics by creating an empty test run to stick them in
     *
     * @param listener the {@link ITestInvocationListener} of test results
     * @param runName the test name
     * @param metrics the {@link Map} that contains metrics for the given test
     */
    void reportMetrics(
            ITestInvocationListener listener, String runName, Map<String, String> metrics) {
        // Create an empty testRun to report the parsed runMetrics
        CLog.d("About to report metrics: %s", metrics);
        listener.testRunStarted(runName, 0);
        listener.testRunEnded(0, TfMetricProtoUtil.upgradeConvert(metrics));
    }

    /**
     * Checks whether {@code line} matches the given {@link Pattern}.
     *
     * @return The resulting {@link Matcher} obtained by matching the {@code line} against {@code
     *     pattern}, or null if the {@code line} does not match.
     */
    private static Matcher matches(Pattern pattern, String line) {
        Matcher ret = pattern.matcher(line);
        return ret.matches() ? ret : null;
    }

    @Override
    public void setDevice(ITestDevice device) {
        mTestDevice = device;
    }

    @Override
    public ITestDevice getDevice() {
        return mTestDevice;
    }
}
