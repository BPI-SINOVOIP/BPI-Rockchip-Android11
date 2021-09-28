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
package com.android.tradefed.testtype;

import com.android.ddmlib.FileListingService;
import com.android.ddmlib.IShellOutputReceiver;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.CollectingOutputReceiver;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import com.google.common.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/** A Test that runs a Google benchmark test package on given device. */
@OptionClass(alias = "gbenchmark")
public class GoogleBenchmarkTest implements IDeviceTest, IRemoteTest, ITestFilterReceiver {

    static final String DEFAULT_TEST_PATH = "/data/benchmarktest";
    static final String GBENCHMARK_FILTER_OPTION = "--benchmark_filter";
    static final int ADB_CMD_CHAR_LIMIT = 4000; // May apply to some old device models.

    private static final String GBENCHMARK_JSON_OUTPUT_FORMAT = "--benchmark_format=json";
    private static final String GBENCHMARK_LIST_TESTS_OPTION = "--benchmark_list_tests=true";
    private static final List<String> DEFAULT_FILE_EXCLUDE_FILTERS = new ArrayList<>();

    static {
        // Exclude .config by default as they are not runnable.
        DEFAULT_FILE_EXCLUDE_FILTERS.add(".*\\.config$");
    }

    @Option(
        name = "file-exclusion-filter-regex",
        description = "Regex to exclude certain files from executing. Can be repeated"
    )
    private List<String> mFileExclusionFilterRegex = new ArrayList<>(DEFAULT_FILE_EXCLUDE_FILTERS);

    @Option(name = "native-benchmark-device-path",
            description="The path on the device where native stress tests are located.")
    private String mDeviceTestPath = DEFAULT_TEST_PATH;

    @Option(name = "benchmark-module-name",
          description="The name of the native benchmark test module to run. " +
          "If not specified all tests in --native-benchmark-device-path will be run.")
    private String mTestModule = null;

    @Option(name = "benchmark-run-name",
          description="Optional name to pass to test reporters. If unspecified, will use " +
          "test binary as run name.")
    private String mReportRunName = null;

    @Option(name = "max-run-time", description =
            "The maximum time to allow for each benchmark run in ms.", isTimeVal=true)
    private long mMaxRunTime = 15 * 60 * 1000;

    @Option(
            name = "include-filter",
            description = "The benchmark (regex) filters used to match benchmarks to include.")
    private Set<String> mIncludeFilters = new LinkedHashSet<>();

    @Option(
            name = "exclude-filter",
            description = "The benchmark (regex) filters used to match benchmarks to exclude.")
    private Set<String> mExcludeFilters = new LinkedHashSet<>();

    private ITestDevice mDevice = null;

    /**
     * {@inheritDoc}
     */
    @Override
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public ITestDevice getDevice() {
        return mDevice;
    }

    /**
     * Set the Android native benchmark test module to run.
     *
     * @param moduleName The name of the native test module to run
     */
    public void setModuleName(String moduleName) {
        mTestModule = moduleName;
    }

    /**
     * Get the Android native benchmark test module to run.
     *
     * @return the name of the native test module to run, or null if not set
     */
    public String getModuleName() {
        return mTestModule;
    }

    public void setReportRunName(String reportRunName) {
        mReportRunName = reportRunName;
    }

    /**
     * Adds an exclusion file filter regex.
     * <p/>
     * Exposed for unit testing
     *
     * @param regex to exclude file.
     */
    void addFileExclusionFilterRegex(String regex) {
        mFileExclusionFilterRegex.add(regex);
    }

    /**
     * Gets the path where native benchmark tests live on the device.
     *
     * @return The path on the device where the native tests live.
     */
    private String getTestPath() {
        StringBuilder testPath = new StringBuilder(mDeviceTestPath);
        if (mTestModule != null) {
            testPath.append(FileListingService.FILE_SEPARATOR);
            testPath.append(mTestModule);
        }
        return testPath.toString();
    }

    /**
     * Executes all native benchmark tests in a folder as well as in all subfolders recursively.
     *
     * @param root The root folder to begin searching for native tests
     * @param testDevice The device to run tests on
     * @param listener the run listener
     * @throws DeviceNotAvailableException
     */
    private void doRunAllTestsInSubdirectory(String root, ITestDevice testDevice,
            ITestInvocationListener listener) throws DeviceNotAvailableException {
        if (testDevice.isDirectory(root)) {
            // recursively run tests in all subdirectories
            for (String child : testDevice.getChildren(root)) {
                doRunAllTestsInSubdirectory(root + "/" + child, testDevice, listener);
            }
        } else {
            // assume every file is a valid benchmark test binary.
            // use name of file as run name
            String rootEntry = root.substring(root.lastIndexOf("/") + 1);
            String runName = (mReportRunName == null ? rootEntry : mReportRunName);

            // force file to be executable
            testDevice.executeShellCommand(String.format("chmod 755 %s", root));
            if (shouldSkipFile(root)) {
                return;
            }
            long startTime = System.currentTimeMillis();

            Set<String> filteredTests = getFilteredTests(testDevice, root);
            CLog.d("List that will be used: %s", Arrays.asList(filteredTests));

            // Count expected number of tests
            int numTests = filteredTests.size();
            if (numTests == 0) {
                CLog.d("No tests to run.");
                return;
            }

            Map<String, String> metricMap = new HashMap<String, String>();
            CollectingOutputReceiver outputCollector = createOutputCollector();
            GoogleBenchmarkResultParser resultParser = createResultParser(runName, listener);
            listener.testRunStarted(runName, numTests);
            try {
                String cmd =
                        String.format(
                                "%s%s %s",
                                root,
                                getFilterFlagForTests(filteredTests),
                                GBENCHMARK_JSON_OUTPUT_FORMAT);
                CLog.i(String.format("Running google benchmark test on %s: %s",
                        mDevice.getSerialNumber(), cmd));
                executeCommand(testDevice, cmd, outputCollector);
                metricMap = resultParser.parse(outputCollector);
            } catch (DeviceNotAvailableException e) {
                listener.testRunFailed(e.getMessage());
                throw e;
            } finally {
                final long elapsedTime = System.currentTimeMillis() - startTime;
                listener.testRunEnded(elapsedTime, TfMetricProtoUtil.upgradeConvert(metricMap));
            }
        }
    }

    /**
     * Returns benchmark tests matching current filters.
     *
     * @param testDevice the device on which to run the command
     * @param fullBinaryPath the full binary path
     * @return matching benchmark tests.
     * @throws DeviceNotAvailableException
     */
    private Set<String> getFilteredTests(ITestDevice testDevice, String fullBinaryPath)
            throws DeviceNotAvailableException {
        Set<String> filteredTests = getTestsForFilters(testDevice, fullBinaryPath, mIncludeFilters);
        if (!mExcludeFilters.isEmpty()) {
            filteredTests.removeAll(
                    getTestsForFilters(testDevice, fullBinaryPath, mExcludeFilters));
        }
        return filteredTests;
    }

    /**
     * Returns benchmark tests matching the filters.
     *
     * @param testDevice the device on which to run the command
     * @param fullBinaryPath the full binary path
     * @param filters filters for matching benchmark tests
     * @return matching benchmark tests.
     * @throws DeviceNotAvailableException
     */
    private Set<String> getTestsForFilters(
            ITestDevice testDevice, String fullBinaryPath, Set<String> filters)
            throws DeviceNotAvailableException {
        String cmd =
                String.format(
                        "%s%s %s",
                        fullBinaryPath,
                        getFilterFlagForFilters(filters),
                        GBENCHMARK_LIST_TESTS_OPTION);
        String list_output = executeCommand(testDevice, cmd, null /* outputReceiver */);
        String[] list = list_output.trim().split("\n");
        if (noMatchesFound(list)) {
            list = new String[0];
        }
        return new LinkedHashSet<String>(Arrays.asList(list));
    }

    // ** Returns true if no matches found. */
    private boolean noMatchesFound(String[] list) {
        if (list.length == 0) {
            return true;
        }

        // A benchmark name is a single word.
        // A no-match output is "Failed to match any benchmarks ..."
        // Consider no matches found if the output line has multiple-words.
        return (list[0].indexOf(' ') >= 0);
    }

    /**
     * Helper method to determine if we should skip the execution of a given file.
     * @param fullPath the full path of the file in question
     * @return true if we should skip the said file.
     */
    protected boolean shouldSkipFile(String fullPath) {
        if (fullPath == null || fullPath.isEmpty()) {
            return true;
        }
        if (mFileExclusionFilterRegex == null || mFileExclusionFilterRegex.isEmpty()) {
            return false;
        }
        for (String regex : mFileExclusionFilterRegex) {
            if (fullPath.matches(regex)) {
                CLog.i(String.format("File %s matches exclusion file regex %s, skipping",
                        fullPath, regex));
                return true;
            }
        }
        return false;
    }

    /**
     * Exposed for testing
     */
    CollectingOutputReceiver createOutputCollector() {
        return new CollectingOutputReceiver();
    }

    /**
     * Exposed for testing
     */
    GoogleBenchmarkResultParser createResultParser(String runName,
            ITestInvocationListener listener) {
        return new GoogleBenchmarkResultParser(runName, listener);
    }

    /** {@inheritDoc} */
    @Override
    public void run(TestInformation testInfo, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        if (mDevice == null) {
            throw new IllegalArgumentException("Device has not been set");
        }
        String testPath = getTestPath();
        if (!mDevice.doesFileExist(testPath)) {
            CLog.w(String.format("Could not find native benchmark test directory %s in %s!",
                    testPath, mDevice.getSerialNumber()));
            throw new RuntimeException(
                    String.format("Could not find native benchmark test directory %s", testPath));
        }
        doRunAllTestsInSubdirectory(testPath, mDevice, listener);
    }

    /*
     * Conforms filters using a {@link TestDescription} format to be recognized by the
     * GoogleBenchmarkTest executable.
     */
    public String cleanFilter(String filter) {
        Integer index = filter.indexOf('#');
        if (index >= 0) {
            // For "class#method", takes the "method" part as the benchmark name.
            String benchmark = filter.substring(index + 1);
            if (benchmark.isEmpty()) {
                CLog.e("Invalid filter %s. Result could be unexpected.", filter);
            } else {
                return benchmark;
            }
        }
        return filter;
    }

    /** {@inheritDoc} */
    @Override
    public void addIncludeFilter(String filter) {
        mIncludeFilters.add(cleanFilter(filter));
    }

    /** {@inheritDoc} */
    @Override
    public void addAllIncludeFilters(Set<String> filters) {
        for (String filter : filters) {
            mIncludeFilters.add(cleanFilter(filter));
        }
    }

    /** {@inheritDoc} */
    @Override
    public void addExcludeFilter(String filter) {
        mExcludeFilters.add(cleanFilter(filter));
    }

    /** {@inheritDoc} */
    @Override
    public void addAllExcludeFilters(Set<String> filters) {
        for (String filter : filters) {
            mExcludeFilters.add(cleanFilter(filter));
        }
    }

    /** {@inheritDoc} */
    @Override
    public Set<String> getIncludeFilters() {
        return mIncludeFilters;
    }

    /** {@inheritDoc} */
    @Override
    public Set<String> getExcludeFilters() {
        return mExcludeFilters;
    }

    /** {@inheritDoc} */
    @Override
    public void clearIncludeFilters() {
        mIncludeFilters.clear();
    }

    /** {@inheritDoc} */
    @Override
    public void clearExcludeFilters() {
        mExcludeFilters.clear();
    }

    @VisibleForTesting
    protected String getFilterFlagForFilters(Set<String> filters) {
        // Use single filter as only the last "--benchmark_filter" is recognized.
        StringBuilder filterFlag = new StringBuilder();
        Iterator<String> iterator = filters.iterator();
        if (iterator.hasNext()) {
            filterFlag.append(String.format(" %s=%s", GBENCHMARK_FILTER_OPTION, iterator.next()));
            while (iterator.hasNext()) {
                filterFlag.append(String.format("\\|%s", iterator.next()));
            }
        }
        return filterFlag.toString();
    }

    @VisibleForTesting
    protected String getFilterFlagForTests(Set<String> fitlererTests) {
        // Pass the list of tests as filter since BenchmarkTest can't handle negative filters.
        StringBuilder filterFlag = new StringBuilder();
        Iterator<String> iterator = fitlererTests.iterator();
        if (iterator.hasNext()) {
            // Format benchmark as "^benchmark$" to avoid unintended regex partial matching.
            filterFlag.append(String.format(" %s=^%s$", GBENCHMARK_FILTER_OPTION, iterator.next()));
            while (iterator.hasNext()) {
                filterFlag.append(String.format("\\|^%s$", iterator.next()));
            }
        }
        return filterFlag.toString();
    }

    /**
     * Helper method to run a benchmarktest command. If the command is too long to be run directly
     * by adb, it runs from a temporary script.
     *
     * @param testDevice the device on which to run the command
     * @param cmd the command string to run
     * @param outputReceiver the output receiver for reading test results
     * @return shell output if outputReceiver is null
     */
    protected String executeCommand(
            final ITestDevice testDevice,
            final String cmd,
            final IShellOutputReceiver outputReceiver)
            throws DeviceNotAvailableException {
        // Ensure that command is not too long for adb
        if (cmd.length() < ADB_CMD_CHAR_LIMIT) {
            if (outputReceiver == null) {
                return testDevice.executeShellCommand(cmd);
            }

            testDevice.executeShellCommand(
                    cmd,
                    outputReceiver,
                    mMaxRunTime /* maxTimeToShellOutputResponse */,
                    TimeUnit.MILLISECONDS,
                    0 /* retryAttempts */);
            return null;
        }

        // Wrap adb shell command in script if command is too long for direct execution
        return executeCommandByScript(testDevice, cmd, outputReceiver);
    }

    /** Runs a command from a temporary script. */
    private String executeCommandByScript(
            final ITestDevice testDevice,
            final String cmd,
            final IShellOutputReceiver outputReceiver)
            throws DeviceNotAvailableException {
        String tmpFileDevice = "/data/local/tmp/gbenchmarktest_script.sh";
        testDevice.pushString(String.format("#!/bin/bash\n%s", cmd), tmpFileDevice);
        String shellOutput = null;
        try {
            if (outputReceiver == null) {
                shellOutput = testDevice.executeShellCommand(String.format("sh %s", tmpFileDevice));
            } else {
                testDevice.executeShellCommand(
                        String.format("sh %s", tmpFileDevice),
                        outputReceiver,
                        mMaxRunTime /* maxTimeToShellOutputResponse */,
                        TimeUnit.MILLISECONDS,
                        0 /* retry attempts */);
            }
        } finally {
            testDevice.deleteFile(tmpFileDevice);
        }
        return shellOutput;
    }
}
