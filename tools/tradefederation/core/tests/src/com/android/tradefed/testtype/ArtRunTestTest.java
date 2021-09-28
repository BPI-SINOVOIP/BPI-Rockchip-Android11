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

import static org.junit.Assert.fail;

import com.android.ddmlib.CollectingOutputReceiver;

import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.FileUtil;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.HashMap;
import java.util.concurrent.TimeUnit;

import org.easymock.EasyMock;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link ArtRunTest}. */
@RunWith(JUnit4.class)
public class ArtRunTestTest {

    // Default run-test name.
    private static final String RUN_TEST_NAME = "run-test";

    private ITestInvocationListener mMockInvocationListener;
    private IAbi mMockAbi;
    private ITestDevice mMockITestDevice;

    private CollectingOutputReceiver mOutputReceiver;
    private ArtRunTest mArtRunTest;
    private OptionSetter mSetter;
    private TestInformation mTestInfo;
    // Target tests directory.
    private File mTmpTargetTestsDir;
    // Expected output file (under the target tests directory).
    private File mTmpExpectedFile;

    @Before
    public void setUp() throws ConfigurationException, IOException {
        mMockInvocationListener = EasyMock.createMock(ITestInvocationListener.class);
        mMockAbi = EasyMock.createMock(IAbi.class);
        mMockITestDevice = EasyMock.createMock(ITestDevice.class);
        mOutputReceiver = new CollectingOutputReceiver();
        mArtRunTest =
                new ArtRunTest() {
                    @Override
                    protected CollectingOutputReceiver createTestOutputReceiver() {
                        return mOutputReceiver;
                    }
                };
        mArtRunTest.setAbi(mMockAbi);
        mArtRunTest.setDevice(mMockITestDevice);
        mSetter = new OptionSetter(mArtRunTest);

        // Set up target tests directory and expected output file.
        mTmpTargetTestsDir = FileUtil.createTempDir("target_testcases");
        File runTestDir = new File(mTmpTargetTestsDir, RUN_TEST_NAME);
        runTestDir.mkdir();
        mTmpExpectedFile = new File(runTestDir, "expected.txt");
        FileWriter fw = new FileWriter(mTmpExpectedFile);
        fw.write("output\n");
        fw.close();

        // Set the target tests directory in test information object.
        mTestInfo = TestInformation.newBuilder().build();
        mTestInfo.executionFiles().put(FilesKey.TARGET_TESTS_DIRECTORY, mTmpTargetTestsDir);
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(mTmpTargetTestsDir);
    }

    /** Helper mocking writing the output of a test command. */
    private void mockTestOutputWrite(String output) {
        mOutputReceiver.addOutput(output.getBytes(), 0, output.length());
    }

    /** Helper that replays all mocks. */
    private void replayMocks() {
        EasyMock.replay(mMockInvocationListener, mMockAbi, mMockITestDevice);
    }

    /** Helper that verifies all mocks. */
    private void verifyMocks() {
        EasyMock.verify(mMockInvocationListener, mMockAbi, mMockITestDevice);
    }

    /** Test run when no device is set should throw an exception. */
    @Test
    public void testRun_noDevice() throws DeviceNotAvailableException {
        mArtRunTest.setDevice(null);
        replayMocks();
        try {
            mArtRunTest.run(mTestInfo, mMockInvocationListener);
            fail("An exception should have been thrown.");
        } catch (IllegalArgumentException e) {
            // Expected.
        }
        verifyMocks();
    }

    /** Test the behavior of the run method when the `run-test-name` option is not set. */
    @Test
    public void testRunSingleTest_unsetRunTestNameOption()
            throws ConfigurationException, DeviceNotAvailableException, IOException {
        final String classpath = "/data/local/tmp/test/test.jar";
        mSetter.setOptionValue("classpath", classpath);

        replayMocks();
        try {
            mArtRunTest.run(mTestInfo, mMockInvocationListener);
            fail("An exception should have been thrown.");
        } catch (IllegalArgumentException e) {
            // Expected.
        }
        verifyMocks();
    }

    /** Test the behavior of the run method when the `classpath` option is not set. */
    @Test
    public void testRunSingleTest_unsetClasspathOption()
            throws ConfigurationException, DeviceNotAvailableException, IOException {
        mSetter.setOptionValue("run-test-name", RUN_TEST_NAME);

        replayMocks();
        try {
            mArtRunTest.run(mTestInfo, mMockInvocationListener);
            fail("An exception should have been thrown.");
        } catch (IllegalArgumentException e) {
            // Expected.
        }
        verifyMocks();
    }

    /** Test the run method for a (single) test. */
    @Test
    public void testRunSingleTest()
            throws ConfigurationException, DeviceNotAvailableException, IOException {
        mSetter.setOptionValue("run-test-name", RUN_TEST_NAME);
        final String classpath = "/data/local/tmp/test/test.jar";
        mSetter.setOptionValue("classpath", classpath);

        // Pre-test checks.
        EasyMock.expect(mMockITestDevice.getSerialNumber()).andReturn("");
        EasyMock.expect(mMockAbi.getName()).andReturn("abi");
        String runName = "ArtRunTest_abi";
        // Beginning of test.
        mMockInvocationListener.testRunStarted(runName, 1);
        TestDescription testId = new TestDescription(runName, RUN_TEST_NAME);
        mMockInvocationListener.testStarted(testId);
        String cmd = String.format("dalvikvm64 -classpath %s Main", classpath);
        // Test execution.
        mMockITestDevice.executeShellCommand(
                cmd, mOutputReceiver, 60000L, TimeUnit.MILLISECONDS, 0);
        mockTestOutputWrite("output\n");
        EasyMock.expect(mMockITestDevice.getSerialNumber()).andReturn("");
        // End of test.
        mMockInvocationListener.testEnded(
                EasyMock.eq(testId), (HashMap<String, Metric>) EasyMock.anyObject());
        mMockInvocationListener.testRunEnded(
                EasyMock.anyLong(), (HashMap<String, Metric>) EasyMock.anyObject());

        replayMocks();

        mArtRunTest.run(mTestInfo, mMockInvocationListener);

        verifyMocks();
    }

    /**
     * Test the behavior of the run method when the output produced by the shell command on device
     * differs from the expected output.
     */
    @Test
    public void testRunSingleTest_unexpectedOutput()
            throws ConfigurationException, DeviceNotAvailableException, IOException {
        mSetter.setOptionValue("run-test-name", RUN_TEST_NAME);
        final String classpath = "/data/local/tmp/test/test.jar";
        mSetter.setOptionValue("classpath", classpath);

        // Pre-test checks.
        EasyMock.expect(mMockITestDevice.getSerialNumber()).andReturn("");
        EasyMock.expect(mMockAbi.getName()).andReturn("abi");
        String runName = "ArtRunTest_abi";
        // Beginning of test.
        mMockInvocationListener.testRunStarted(runName, 1);
        TestDescription testId = new TestDescription(runName, RUN_TEST_NAME);
        mMockInvocationListener.testStarted(testId);
        String cmd = String.format("dalvikvm64 -classpath %s Main", classpath);
        // Test execution.
        mMockITestDevice.executeShellCommand(
                cmd, mOutputReceiver, 60000L, TimeUnit.MILLISECONDS, 0);
        mockTestOutputWrite("unexpected\n");
        EasyMock.expect(mMockITestDevice.getSerialNumber()).andReturn("");
        // End of test.
        mMockInvocationListener.testFailed(testId, "'unexpected\n' instead of 'output\n'");
        mMockInvocationListener.testEnded(
                EasyMock.eq(testId), (HashMap<String, Metric>) EasyMock.anyObject());
        mMockInvocationListener.testRunEnded(
                EasyMock.anyLong(), (HashMap<String, Metric>) EasyMock.anyObject());

        replayMocks();

        mArtRunTest.run(mTestInfo, mMockInvocationListener);

        verifyMocks();
    }
}
