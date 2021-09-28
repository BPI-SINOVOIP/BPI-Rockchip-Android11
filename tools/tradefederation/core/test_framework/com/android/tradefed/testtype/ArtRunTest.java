/*
 * Copyright (C) 2020 The Android Open Source Project
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

import com.android.ddmlib.CollectingOutputReceiver;

import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.AbiUtils;
import com.android.tradefed.util.ArrayUtil;
import com.android.tradefed.util.FileUtil;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** A test runner to run ART run-tests. */
public class ArtRunTest implements IDeviceTest, IRemoteTest, IAbiReceiver {

    private static final String RUNTEST_TAG = "ArtRunTest";

    private static final String DALVIKVM_CMD =
            "dalvikvm|#BITNESS#| -classpath |#CLASSPATH#| |#MAINCLASS#|";

    @Option(
            name = "test-timeout",
            description =
                    "The max time in ms for an art run-test to "
                            + "run. Test run will be aborted if any test takes longer.",
            isTimeVal = true)
    private long mMaxTestTimeMs = 1 * 60 * 1000;

    @Option(name = "run-test-name", description = "The name to use when reporting results.")
    private String mRunTestName;

    @Option(name = "classpath", description = "Holds the paths to search when loading tests.")
    private List<String> mClasspath = new ArrayList<>();

    private ITestDevice mDevice = null;
    private IAbi mAbi = null;

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

    @Override
    public IAbi getAbi() {
        return mAbi;
    }

    /** {@inheritDoc} */
    @Override
    public void run(TestInformation testInfo, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        if (mDevice == null) {
            throw new IllegalArgumentException("Device has not been set.");
        }
        if (mAbi == null) {
            throw new IllegalArgumentException("ABI has not been set.");
        }
        if (mRunTestName == null) {
            throw new IllegalArgumentException("Run-test name has not been set.");
        }
        if (mClasspath.isEmpty()) {
            throw new IllegalArgumentException("Classpath is empty.");
        }

        runArtTest(testInfo, listener);
    }

    /**
     * Run a single ART run-test (on device).
     *
     * @param listener {@link ITestInvocationListener} listener for test
     * @throws DeviceNotAvailableException
     */
    void runArtTest(TestInformation testInfo, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        CLog.i("Running ArtRunTest %s on %s", mRunTestName, mDevice.getSerialNumber());

        String cmd = DALVIKVM_CMD;
        String abi = mAbi.getName();
        cmd = cmd.replace("|#BITNESS#|", AbiUtils.getBitness(abi));
        cmd = cmd.replace("|#CLASSPATH#|", ArrayUtil.join(File.pathSeparator, mClasspath));
        // TODO: Turn this into an an option of the `ArtRunTest` class?
        cmd = cmd.replace("|#MAINCLASS#|", "Main");

        CLog.d("About to run run-test command: %s", cmd);
        String runName = String.format("%s_%s", RUNTEST_TAG, abi);
        // Note: We only run one test at the moment.
        int testCount = 1;
        TestDescription testId = new TestDescription(runName, mRunTestName);
        listener.testRunStarted(runName, testCount);
        listener.testStarted(testId);

        try {
            // Execute the test on device.
            CollectingOutputReceiver receiver = createTestOutputReceiver();
            mDevice.executeShellCommand(
                    cmd, receiver, mMaxTestTimeMs, TimeUnit.MILLISECONDS, /* retryAttempts */ 0);
            String output = receiver.getOutput();
            CLog.v("%s on %s returned %s", cmd, mDevice.getSerialNumber(), output);

            // Check the output producted by the test.
            if (output != null) {
                try {
                    File expectedFile = getDependencyFileFromRunTestDir(testInfo, "expected.txt");
                    CLog.i("Found expected output for run-test %s: %s", mRunTestName, expectedFile);
                    String expected = FileUtil.readStringFromFile(expectedFile);
                    if (!output.equals(expected)) {
                        String error = String.format("'%s' instead of '%s'", output, expected);
                        // TODO: Implement better reporting, e.g. using a diff output.
                        // Also, the "check" step should be configurable, as this is the case in
                        // current ART run-test scripts).
                        CLog.i("%s FAILED: %s", mRunTestName, error);
                        listener.testFailed(testId, error);
                    }
                } catch (IOException ioe) {
                    CLog.e(
                            "I/O error while accessing expected output file for test %s: %s",
                            mRunTestName, ioe);
                    listener.testFailed(testId, "I/O error while accessing expected output file.");
                }
            } else {
                listener.testFailed(testId, "No output received to compare to.");
            }
        } finally {
            HashMap<String, Metric> emptyTestMetrics = new HashMap();
            listener.testEnded(testId, emptyTestMetrics);
            HashMap<String, Metric> emptyTestRunMetrics = new HashMap();
            // TODO: Pass an actual value as `elapsedTimeMillis` argument.
            listener.testRunEnded(/* elapsedTimeMillis*/ 0, emptyTestRunMetrics);
        }
    }

    /** Create an output receiver for the test command executed on the device. */
    protected CollectingOutputReceiver createTestOutputReceiver() {
        return new CollectingOutputReceiver();
    }

    /** Search for a dependency/artifact file in the run-test's directory. */
    protected File getDependencyFileFromRunTestDir(TestInformation testInfo, String fileName)
            throws FileNotFoundException {
        File testsDir = testInfo.executionFiles().get(FilesKey.TARGET_TESTS_DIRECTORY);
        if (testsDir == null || !testsDir.exists()) {
            throw new FileNotFoundException(
                    String.format(
                            "Could not find target tests directory for test %s.", mRunTestName));
        }
        File runTestDir = new File(testsDir, mRunTestName);
        File file = FileUtil.findFile(runTestDir, fileName);
        if (file == null) {
            throw new FileNotFoundException(
                    String.format(
                            "Could not find an artifact file associated with %s in directory %s.",
                            fileName, runTestDir));
        }
        return file;
    }
}
