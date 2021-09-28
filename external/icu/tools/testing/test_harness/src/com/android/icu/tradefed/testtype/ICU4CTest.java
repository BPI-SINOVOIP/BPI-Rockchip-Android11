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

package com.android.icu.tradefed.testtype;

import com.android.ddmlib.FileListingService;
import com.android.ddmlib.testrunner.ITestRunListener;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.testtype.IAbiReceiver;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.IRuntimeHintProvider;
import com.android.tradefed.testtype.ITestFilterReceiver;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.LinkedList;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.regex.Pattern;

/** A Test that runs a native test package on given device. */
@OptionClass(alias = "icu4c")
public class ICU4CTest
        implements IDeviceTest,
    ITestFilterReceiver,
    IRemoteTest,
    IAbiReceiver,
    IRuntimeHintProvider {

    static final String DEFAULT_NATIVETEST_PATH = "/data/local/tmp";

    private ITestDevice mDevice = null;
    private IAbi mAbi = null;

    @Option(name = "module-name", description = "The name of the native test module to run.")
    private String mTestModule = null;

    @Option(name = "command-filter-prefix",
        description = "The prefix required for each test filter when running the shell command")
    private String mCommandFilterPrefix = "";

    @Option(
        name = "native-test-timeout",
        description =
                "The max time in ms for the test to run. "
                        + "Test run will be aborted if any test takes longer."
    )
    private int mMaxTestTimeMs = 1 * 60 * 1000;

    @Option(name = "run-test-as", description = "User to execute test binary as.")
    private String mRunTestAs = null;

    @Option(
        name = "runtime-hint",
        description = "The hint about the test's runtime.",
        isTimeVal = true
    )
    private long mRuntimeHint = 60000; // 1 minute

    @Option(
        name = "no-fail-data-errors",
        description = "Treat data load failures as warnings, not errors."
    )
    private boolean mNoFailDataErrors = false;

    @Option(
        name = "include-filter",
        description = "The ICU-specific positive filter of the test names to run."
    )
    private Set<String> mIncludeFilters = new LinkedHashSet<>();


    @Option(
        name = "exclude-filter",
        description = "The ICU-specific negative filter of the test names to run."
    )
    private Set<String> mExcludeFilters = new LinkedHashSet<>();

    private static final String TEST_FLAG_NO_FAIL_DATA_ERRORS = "-w";
    private static final String TEST_FLAG_XML_OUTPUT = "-x";

    /** {@inheritDoc} */
    @Override
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    /** {@inheritDoc} */
    @Override
    public ITestDevice getDevice() {
        return mDevice;
    }

    /** {@inheritDoc} */
    @Override
    public void setAbi(IAbi abi) {
        mAbi = abi;
    }

    /** {@inheritDoc} */
    @Override
    public IAbi getAbi() {
        return mAbi;
    }

    /**
     * Set the Android native test module to run.
     *
     * @param moduleName The name of the native test module to run
     */
    public void setModuleName(String moduleName) {
        mTestModule = moduleName;
    }

    /**
     * Get the Android native test module to run.
     *
     * @return the name of the native test module to run, or null if not set
     */
    public String getModuleName() {
        return mTestModule;
    }

    /** Set the max time in ms for the test to run. */
    @VisibleForTesting
    void setMaxTestTimeMs(int timeout) {
        mMaxTestTimeMs = timeout;
    }

    /** {@inheritDoc} */
    @Override
    public long getRuntimeHint() {
        return mRuntimeHint;
    }

    /** {@inheritDoc} */
    @Override
    public void addIncludeFilter(String filter) {
        mIncludeFilters.add(filter);
    }

    /** {@inheritDoc} */
    @Override
    public void addAllIncludeFilters(Set<String> filters) {
        mIncludeFilters.addAll(filters);
    }

    /** {@inheritDoc} */
    @Override
    public void addExcludeFilter(String filter) {
        mExcludeFilters.add(filter);
    }

    /** {@inheritDoc} */
    @Override
    public void addAllExcludeFilters(Set<String> filters) {
        mExcludeFilters.addAll(filters);
    }

    @Override
    public Set<String> getIncludeFilters() {
        return mIncludeFilters;
    }

    @Override
    public Set<String> getExcludeFilters() {
        return mExcludeFilters;
    }

    /** {@inheritDoc} */
    @Override
    public void clearExcludeFilters() {
        mExcludeFilters.clear();
    }

    @Override
    public void clearIncludeFilters() {
        mIncludeFilters.clear();
    }

    public void setCommandFilterPrefix(String s) {
        if (s == null) {
            throw new NullPointerException("CommandFilterPrefix can't be null");
        }
        mCommandFilterPrefix = s;
    }

    public String getCommandFilterPrefix() {
        return mCommandFilterPrefix;
    }

    /**
     * Gets the path where native tests live on the device.
     *
     * @return The path on the device where the native tests live.
     */
    private String getTestPath() {
        StringBuilder testPath = new StringBuilder(DEFAULT_NATIVETEST_PATH);

        testPath.append(FileListingService.FILE_SEPARATOR);
        testPath.append(mTestModule);

        return testPath.toString();
    }

    protected boolean isDeviceFileExecutable(String fullPath) throws DeviceNotAvailableException {
        CommandResult commandResult = mDevice.executeShellV2Command(String.format("[ -x %s ]",
            fullPath));
        return commandResult.getExitCode() == 0;
    }

    /**
     * Run the given test binary.
     *
     * @param testDevice the {@link ITestDevice}
     * @param fullPath absolute file system path to test binary on device
     * @param xmlOutputPath absolute file system path to the XML result file
     * @throws DeviceNotAvailableException
     */
    private CommandResult doRunTest(
            final ITestDevice testDevice, final String fullPath, final String xmlOutputPath)
            throws DeviceNotAvailableException {
        String cmd = getTestCmdLine(fullPath, xmlOutputPath);
        CLog.i("Running ICU4C test %s on %s", cmd, testDevice.getSerialNumber());
        CommandResult commandResult =
                testDevice.executeShellV2Command(
                        cmd,
                        mMaxTestTimeMs /* maxTimeToShellOutputResponse */,
                        TimeUnit.MILLISECONDS,
                        0 /* retryAttempts */);
        CLog.d(
                "%s executed with an exit code of %d and status code  %s",
                fullPath, commandResult.getExitCode(), commandResult.getStatus().name());

        if (commandResult.getExitCode() != 0) {
            CLog.e("Command stdout:\n " + commandResult.getStdout());
            CLog.e("Command stderr:\n " + commandResult.getStderr());
            throw new IllegalStateException(
                    String.format(
                            "%s non-zero exit code %d", fullPath, commandResult.getExitCode()));
        }

        if (commandResult.getStatus() != CommandStatus.SUCCESS) {
            CLog.e("Command stdout:\n " + commandResult.getStdout());
            CLog.e("Command stderr:\n " + commandResult.getStderr());
            throw new IllegalStateException(
                    String.format(
                            "%s exits with status %s", fullPath, commandResult.getStatus().name()));
        }

        return commandResult;
    }

    /**
     * Run the given test binary and parse XML results
     *
     * <p>This methods typically requires the filter for .tff and .xml files, otherwise it will post
     * some unwanted results.
     *
     * @param testDevice the {@link ITestDevice}
     * @param fullPath absolute file system path to test binary on device
     * @param listener the {@link ITestRunListener}
     * @throws DeviceNotAvailableException
     */
    private void runTest(
            final ITestDevice testDevice, final String fullPath, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        CLog.i("Running runTest path: %s", fullPath);
        String xmlFullPath = fullPath + "_res.xml";

        try {
            CommandResult commandResult = doRunTest(testDevice, fullPath, xmlFullPath);

            // Pull the result file, may not exist if issue with the test.
            String testRunName = fullPath.substring(fullPath.lastIndexOf("/") + 1);
            File tmpOutput = FileUtil.createTempFile(testRunName, ".xml");
            testDevice.pullFile(xmlFullPath, tmpOutput);

            ICU4CXmlResultParser parser = new ICU4CXmlResultParser(mTestModule,
                testRunName, listener);

            parser.parseResult(tmpOutput, commandResult);
        } catch (IOException e) {
            throw new IllegalStateException(e);
        } finally {
            // Clean the file on the device
            testDevice.executeShellCommand("rm " + xmlFullPath);
        }
    }

    /**
     * Helper method to build the test command to run.
     *
     * @param fullPath absolute file system path to test binary on device
     * @param xmlPath absolute file system path to the XML reult file on device
     * @return the shell command line to run for the test
     */
    protected String getTestCmdLine(String fullPath, String xmlPath) {
        List<String> args = new LinkedList<String>();

        // su to requested user
        if (mRunTestAs != null) {
            args.add(String.format("su %s", mRunTestAs));
        }

        args.add(fullPath);

        if (mNoFailDataErrors) {
            args.add(TEST_FLAG_NO_FAIL_DATA_ERRORS);
        }

        args.add(TEST_FLAG_XML_OUTPUT);
        args.add(xmlPath);

        String cmd = String.join(" ", args);

        List<String> includeFilters = preprocessIncludeFilters();
        if (!includeFilters.isEmpty()) {
            cmd += " " + String.join(" ", includeFilters);
        }

        return cmd;
    }

    private List<String> preprocessIncludeFilters() {
        Set<String> includeFilters = mIncludeFilters;
        List<String> results = new ArrayList<>();
        for (String filter : includeFilters) {
            if (!filter.startsWith(mTestModule)) {
                CLog.i("Ignore positive filter which does not contain module prefix \"%s\":%s",
                    mTestModule, filter);
                continue;
            }
            String modifiedFilter = filter.substring(mTestModule.length());
            if (filter.length() == 0) {
                // Ignore because it intends to run all tests when the filter is the module name.
                continue;
            }
            // Android / tradefed uses '.' as package separator, but ICU4C tests use '/'.
            modifiedFilter = modifiedFilter.replace('.', '/');

            if (modifiedFilter.charAt(0) != '/' || modifiedFilter.length() == 1) {
                CLog.i("Ignore invalid filter:%s", filter);
                continue;
            }
            modifiedFilter = mCommandFilterPrefix + modifiedFilter.substring(1);
            results.add(modifiedFilter);
        }
        return results;
    }

    /** {@inheritDoc} */
    @Override
    public void run(ITestInvocationListener listener) throws DeviceNotAvailableException {
        if (mDevice == null) {
            throw new IllegalArgumentException("Device has not been set");
        }

        String testPath = getTestPath();
        if (!mDevice.doesFileExist(testPath)) {
            throw new IllegalStateException(
                    String.format(
                            "Could not find native test binary %s in %s!",
                            testPath, mDevice.getSerialNumber()));
        }
        if (!isDeviceFileExecutable(testPath)) {
            throw new IllegalStateException(
                    String.format(
                            "%s exists but is not executable in %s.",
                            testPath, mDevice.getSerialNumber()));
        }
        if (!mExcludeFilters.isEmpty()) {
            throw new IllegalStateException("ICU4C test suites do not support exclude filters");
        }
        runTest(mDevice, testPath, listener);
    }
}
