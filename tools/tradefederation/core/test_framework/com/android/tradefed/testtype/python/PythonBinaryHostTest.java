/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.tradefed.testtype.python;

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.ResultForwarder;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.ITestFilterReceiver;
import com.android.tradefed.testtype.PythonUnitTestResultParser;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.SubprocessTestResultsParser;

import com.google.common.base.Joiner;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Formatter;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/**
 * Host test meant to run a python binary file from the Android Build system (Soong)
 *
 * <p>The test runner supports include-filter and exclude-filter. Note that exclude-filter works by
 * ignoring the test result, instead of skipping the actual test. The tests specified in the
 * exclude-filter will still be executed.
 */
@OptionClass(alias = "python-host")
public class PythonBinaryHostTest implements IRemoteTest, ITestFilterReceiver {

    protected static final String ANDROID_SERIAL_VAR = "ANDROID_SERIAL";
    protected static final String LD_LIBRARY_PATH = "LD_LIBRARY_PATH";
    protected static final String PATH_VAR = "PATH";
    protected static final long PATH_TIMEOUT_MS = 60000L;

    @VisibleForTesting static final String USE_TEST_OUTPUT_FILE_OPTION = "use-test-output-file";
    static final String TEST_OUTPUT_FILE_FLAG = "test-output-file";

    private static final String PYTHON_LOG_STDOUT_FORMAT = "%s-stdout";
    private static final String PYTHON_LOG_STDERR_FORMAT = "%s-stderr";
    private static final String PYTHON_LOG_TEST_OUTPUT_FORMAT = "%s-test-output";

    private Set<String> mIncludeFilters = new LinkedHashSet<>();
    private Set<String> mExcludeFilters = new LinkedHashSet<>();
    private String mLdLibraryPath = null;

    @Option(name = "par-file-name", description = "The binary names inside the build info to run.")
    private Set<String> mBinaryNames = new HashSet<>();

    @Option(
        name = "python-binaries",
        description = "The full path to a runnable python binary. Can be repeated."
    )
    private Set<File> mBinaries = new HashSet<>();

    @Option(
        name = "test-timeout",
        description = "Timeout for a single par file to terminate.",
        isTimeVal = true
    )
    private long mTestTimeout = 20 * 1000L;

    @Option(
            name = "inject-serial-option",
            description = "Whether or not to pass a -s <serialnumber> option to the binary")
    private boolean mInjectSerial = false;

    @Option(
            name = "inject-android-serial",
            description = "Whether or not to pass a ANDROID_SERIAL variable to the process.")
    private boolean mInjectAndroidSerialVar = true;

    @Option(
        name = "python-options",
        description = "Option string to be passed to the binary when running"
    )
    private List<String> mTestOptions = new ArrayList<>();

    @Option(
            name = USE_TEST_OUTPUT_FILE_OPTION,
            description =
                    "Whether the test should write results to the file specified via the --"
                            + TEST_OUTPUT_FILE_FLAG
                            + " flag instead of stderr which could contain spurious messages that "
                            + "break result parsing. Using this option requires that the Python "
                            + "test have the necessary logic to accept the flag and write results "
                            + "in the expected format.")
    private boolean mUseTestOutputFile = false;

    private TestInformation mTestInfo;
    private IRunUtil mRunUtil;

    /** {@inheritDoc} */
    @Override
    public void addIncludeFilter(String filter) {
        mIncludeFilters.add(filter);
    }

    /** {@inheritDoc} */
    @Override
    public void addExcludeFilter(String filter) {
        mExcludeFilters.add(filter);
    }

    /** {@inheritDoc} */
    @Override
    public void addAllIncludeFilters(Set<String> filters) {
        mIncludeFilters.addAll(filters);
    }

    /** {@inheritDoc} */
    @Override
    public void addAllExcludeFilters(Set<String> filters) {
        mExcludeFilters.addAll(filters);
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

    @Override
    public final void run(TestInformation testInfo, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        mTestInfo = testInfo;
        File testDir = mTestInfo.executionFiles().get(FilesKey.HOST_TESTS_DIRECTORY);
        if (testDir == null || !testDir.exists()) {
            testDir = mTestInfo.executionFiles().get(FilesKey.TESTS_DIRECTORY);
        }
        if (testDir != null && testDir.exists()) {
            File libDir = new File(testDir, "lib");
            List<String> ldLibraryPath = new ArrayList<>();
            if (libDir.exists()) {
                ldLibraryPath.add(libDir.getAbsolutePath());
            }

            File lib64Dir = new File(testDir, "lib64");
            if (lib64Dir.exists()) {
                ldLibraryPath.add(lib64Dir.getAbsolutePath());
            }
            if (!ldLibraryPath.isEmpty()) {
                mLdLibraryPath = Joiner.on(":").join(ldLibraryPath);
            }
        }
        List<File> pythonFilesList = findParFiles();
        for (File pyFile : pythonFilesList) {
            if (!pyFile.exists()) {
                CLog.d(
                        "ignoring %s which doesn't look like a test file.",
                        pyFile.getAbsolutePath());
                continue;
            }
            pyFile.setExecutable(true);
            runSinglePythonFile(listener, testInfo, pyFile);
        }
    }

    private List<File> findParFiles() {
        File testsDir = null;
        if (mTestInfo.getBuildInfo() instanceof IDeviceBuildInfo) {
            testsDir = ((IDeviceBuildInfo) mTestInfo.getBuildInfo()).getTestsDir();
        }
        List<File> files = new ArrayList<>();
        for (String parFileName : mBinaryNames) {
            File res = null;
            // search tests dir
            if (testsDir != null) {
                res = FileUtil.findFile(testsDir, parFileName);
            }

            // TODO: is there other places to search?
            if (res == null) {
                throw new RuntimeException(
                        String.format("Couldn't find a par file %s", parFileName));
            }
            files.add(res);
        }
        files.addAll(mBinaries);
        return files;
    }

    private void runSinglePythonFile(
            ITestInvocationListener listener, TestInformation testInfo, File pyFile) {
        List<String> commandLine = new ArrayList<>();
        commandLine.add(pyFile.getAbsolutePath());
        // If we have a physical device, pass it to the python test by serial
        if (!(mTestInfo.getDevice().getIDevice() instanceof StubDevice) && mInjectSerial) {
            // TODO: support multi-device python tests?
            commandLine.add("-s");
            commandLine.add(mTestInfo.getDevice().getSerialNumber());
        }

        if (mLdLibraryPath != null) {
            getRunUtil().setEnvVariable(LD_LIBRARY_PATH, mLdLibraryPath);
        }
        if (mInjectAndroidSerialVar) {
            getRunUtil()
                    .setEnvVariable(ANDROID_SERIAL_VAR, mTestInfo.getDevice().getSerialNumber());
        }

        File tempTestOutputFile = null;
        if (mUseTestOutputFile) {
            try {
                tempTestOutputFile = FileUtil.createTempFile("python-test-output", ".txt");
            } catch (IOException e) {
                throw new RuntimeException(e);
            }

            commandLine.add("--" + TEST_OUTPUT_FILE_FLAG);
            commandLine.add(tempTestOutputFile.getAbsolutePath());
        }

        File updatedAdb = testInfo.executionFiles().get(FilesKey.ADB_BINARY);
        if (updatedAdb == null) {
            String adbPath = getAdbPath();
            // Don't check if it's the adb on the $PATH
            if (!adbPath.equals("adb")) {
                updatedAdb = new File(adbPath);
                if (!updatedAdb.exists()) {
                    updatedAdb = null;
                }
            }
        }
        if (updatedAdb != null) {
            CLog.d("Testing with adb binary at: %s", updatedAdb);
            // If a special adb version is used, pass it to the PATH
            CommandResult pathResult =
                    getRunUtil()
                            .runTimedCmd(PATH_TIMEOUT_MS, "/bin/bash", "-c", "echo $" + PATH_VAR);
            if (!CommandStatus.SUCCESS.equals(pathResult.getStatus())) {
                throw new RuntimeException(
                        String.format(
                                "Failed to get the $PATH. status: %s, stdout: %s, stderr: %s",
                                pathResult.getStatus(),
                                pathResult.getStdout(),
                                pathResult.getStderr()));
            }
            // Include the directory of the adb on the PATH to be used.
            String path =
                    String.format(
                            "%s:%s",
                            updatedAdb.getParentFile().getAbsolutePath(),
                            pathResult.getStdout().trim());
            CLog.d("Using $PATH with updated adb: %s", path);
            getRunUtil().setEnvVariable(PATH_VAR, path);
            // Log the version of adb seen
            CommandResult versionRes = getRunUtil().runTimedCmd(PATH_TIMEOUT_MS, "adb", "version");
            CLog.d("%s", versionRes.getStdout());
            CLog.d("%s", versionRes.getStderr());
        }
        // Add all the other options
        commandLine.addAll(mTestOptions);

        CommandResult result =
                getRunUtil().runTimedCmd(mTestTimeout, commandLine.toArray(new String[0]));
        String runName = pyFile.getName();
        PythonForwarder forwarder = new PythonForwarder(listener, runName);
        if (!CommandStatus.SUCCESS.equals(result.getStatus())) {
            CLog.e(
                    "Something went wrong when running the python binary:\nstdout: "
                            + "%s\nstderr:%s",
                    result.getStdout(), result.getStderr());
        }
        if (result.getStdout() != null) {
            try (InputStreamSource data =
                    new ByteArrayInputStreamSource(result.getStdout().getBytes())) {
                listener.testLog(
                        String.format(PYTHON_LOG_STDOUT_FORMAT, runName), LogDataType.TEXT, data);
            }
        }
        File stderrFile = null;
        try {
            // Note that we still log stderr when parsing results from a test-written output file
            // since it most likely contains useful debugging information.
            stderrFile = FileUtil.createTempFile("python-res", ".txt");
            FileUtil.writeToFile(result.getStderr(), stderrFile);
            testLogFile(listener, String.format(PYTHON_LOG_STDERR_FORMAT, runName), stderrFile);

            File testOutputFile = stderrFile;
            String testOutput = result.getStderr();

            if (mUseTestOutputFile) {
                testOutputFile = tempTestOutputFile;
                // This assumes that the output file is encoded using the same charset as the
                // currently configured default.
                testOutput = FileUtil.readStringFromFile(testOutputFile);
                testLogFile(
                        listener,
                        String.format(PYTHON_LOG_TEST_OUTPUT_FORMAT, runName),
                        testOutputFile);
            }

            // If it doesn't have the std output TEST_RUN_STARTED, use regular parser.
            if (!testOutput.contains("TEST_RUN_STARTED")) {
                // Attempt to parse the pure python output
                PythonUnitTestResultParser pythonParser =
                        new PythonUnitTestResultParser(
                                Arrays.asList(forwarder),
                                "python-run",
                                mIncludeFilters,
                                mExcludeFilters);
                pythonParser.processNewLines(testOutput.split("\n"));
            } else {
                if (!mIncludeFilters.isEmpty() || !mExcludeFilters.isEmpty()) {
                    throw new RuntimeException(
                            "Non-unittest python test does not support using filters in "
                                    + "PythonBinaryHostTest. Please use test runner "
                                    + "ExecutableHostTest instead.");
                }
                try (SubprocessTestResultsParser parser =
                        new SubprocessTestResultsParser(forwarder, mTestInfo.getContext())) {
                    parser.parseFile(testOutputFile);
                }
            }
        } catch (RuntimeException e) {
            StringBuilder message = new StringBuilder();
            Formatter formatter = new Formatter(message);

            formatter.format(
                    "Failed to parse the python logs: %s. Please ensure that verbosity of "
                            + "output is high enough to be parsed.",
                    e.getMessage());

            if (mUseTestOutputFile) {
                formatter.format(
                        " Make sure that your test writes its output to the file specified "
                                + "by the --%s flag and that its contents (%s) are in the format "
                                + "expected by the test runner.",
                        TEST_OUTPUT_FILE_FLAG,
                        String.format(PYTHON_LOG_TEST_OUTPUT_FORMAT, runName));
            }

            reportFailure(listener, runName, message.toString());
            CLog.e(e);
        } catch (IOException e) {
            throw new RuntimeException(e);
        } finally {
            FileUtil.deleteFile(stderrFile);
            FileUtil.deleteFile(tempTestOutputFile);
        }
    }

    @VisibleForTesting
    IRunUtil getRunUtil() {
        if (mRunUtil == null) {
            mRunUtil = new RunUtil();
        }
        return mRunUtil;
    }

    @VisibleForTesting
    String getAdbPath() {
        return GlobalConfiguration.getDeviceManagerInstance().getAdbPath();
    }

    private void reportFailure(
            ITestInvocationListener listener, String runName, String errorMessage) {
        listener.testRunStarted(runName, 0);
        FailureDescription description =
                FailureDescription.create(errorMessage, FailureStatus.TEST_FAILURE);
        listener.testRunFailed(description);
        listener.testRunEnded(0L, new HashMap<String, Metric>());
    }

    private static void testLogFile(ITestInvocationListener listener, String dataName, File f) {
        try (FileInputStreamSource data = new FileInputStreamSource(f)) {
            listener.testLog(dataName, LogDataType.TEXT, data);
        }
    }

    /** Result forwarder to replace the run name by the binary name. */
    public static class PythonForwarder extends ResultForwarder {

        private String mRunName;

        /** Ctor with the run name using the binary name. */
        public PythonForwarder(ITestInvocationListener listener, String name) {
            super(listener);
            mRunName = name;
        }

        @Override
        public void testRunStarted(String runName, int testCount) {
            // Replace run name
            testRunStarted(runName, testCount, 0);
        }

        @Override
        public void testRunStarted(String runName, int testCount, int attempt) {
            // Replace run name
            testRunStarted(runName, testCount, attempt, System.currentTimeMillis());
        }

        @Override
        public void testRunStarted(String runName, int testCount, int attempt, long startTime) {
            // Replace run name
            super.testRunStarted(mRunName, testCount, attempt, startTime);
        }
    }
}
