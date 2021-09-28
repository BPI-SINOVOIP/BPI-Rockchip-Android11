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
package com.android.tradefed.testtype.binary;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;

import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.metrics.proto.MetricMeasurement;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.error.InfraErrorIdentifier;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.util.CommandResult;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.util.HashMap;
import java.util.Collection;
import java.util.Map;

/** Unit tests for {@link com.android.tradefed.testtype.binary.ExecutableTargetTest}. */
@RunWith(JUnit4.class)
public class ExecutableTargetTestTest {
    private final String testName1 = "testName1";
    private final String testCmd1 = "cmd1";
    private final String testName2 = "testName2";
    private final String testCmd2 = "cmd2";
    private final String testName3 = "testName3";
    private final String testCmd3 = "cmd3";
    private static final String ERROR_MESSAGE = "binary returned non-zero exit code.";

    private ITestInvocationListener mListener = null;
    private ITestDevice mMockITestDevice = null;
    private ExecutableTargetTest mExecutableTargetTest;

    private TestInformation mTestInfo;

    /** Helper to initialize the various EasyMocks we'll need. */
    @Before
    public void setUp() throws Exception {
        mListener = Mockito.mock(ITestInvocationListener.class);
        mMockITestDevice = Mockito.mock(ITestDevice.class);
        InvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mMockITestDevice);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
        mTestInfo = TestInformation.newBuilder().build();
    }

    /** Test the run method for a couple commands and success */
    @Test
    public void testRun_cmdSuccess() throws DeviceNotAvailableException, ConfigurationException {
        mExecutableTargetTest =
                new ExecutableTargetTest() {
                    @Override
                    public String findBinary(String binary) {
                        return binary;
                    }

                    @Override
                    protected void checkCommandResult(
                            CommandResult result,
                            ITestInvocationListener listener,
                            TestDescription description) {}
                };
        mExecutableTargetTest.setDevice(mMockITestDevice);
        // Set test commands
        OptionSetter setter = new OptionSetter(mExecutableTargetTest);
        setter.setOptionValue("test-command-line", testName1, testCmd1);
        setter.setOptionValue("test-command-line", testName2, testCmd2);
        mExecutableTargetTest.run(mTestInfo, mListener);
        // run cmd1 test
        TestDescription testDescription = new TestDescription(testName1, testName1);
        Mockito.verify(mListener, Mockito.times(1)).testRunStarted(eq(testName1), eq(1));
        Mockito.verify(mListener, Mockito.times(1)).testStarted(Mockito.eq(testDescription));
        Mockito.verify(mListener, Mockito.times(1))
                .testEnded(
                        Mockito.eq(testDescription),
                        Mockito.eq(new HashMap<String, MetricMeasurement.Metric>()));
        // run cmd2 test
        TestDescription testDescription2 = new TestDescription(testName2, testName2);
        Mockito.verify(mListener, Mockito.times(1)).testRunStarted(eq(testName2), eq(1));
        Mockito.verify(mListener, Mockito.times(1)).testStarted(Mockito.eq(testDescription2));
        Mockito.verify(mListener, Mockito.times(1))
                .testEnded(
                        Mockito.eq(testDescription2),
                        Mockito.eq(new HashMap<String, MetricMeasurement.Metric>()));
        Mockito.verify(mListener, Mockito.times(2))
                .testRunEnded(
                        Mockito.anyLong(),
                        Mockito.<HashMap<String, MetricMeasurement.Metric>>any());
    }

    /** Test the run method for a couple commands but binary path not found. */
    @Test
    public void testRun_pathNotExist() throws DeviceNotAvailableException, ConfigurationException {
        mExecutableTargetTest =
                new ExecutableTargetTest() {
                    @Override
                    public String findBinary(String binary) {
                        return null;
                    }

                    @Override
                    protected void checkCommandResult(
                            CommandResult result,
                            ITestInvocationListener listener,
                            TestDescription description) {}
                };
        mExecutableTargetTest.setDevice(mMockITestDevice);
        // Set test commands
        OptionSetter setter = new OptionSetter(mExecutableTargetTest);
        setter.setOptionValue("test-command-line", testName1, testCmd1);
        setter.setOptionValue("test-command-line", testName2, testCmd2);
        mExecutableTargetTest.run(mTestInfo, mListener);
        // run cmd1 test
        Mockito.verify(mListener, Mockito.times(0)).testRunStarted(eq(testName1), eq(1));
        FailureDescription failure1 =
                FailureDescription.create(
                                String.format(ExecutableBaseTest.NO_BINARY_ERROR, testCmd1),
                                FailureStatus.TEST_FAILURE)
                        .setErrorIdentifier(InfraErrorIdentifier.ARTIFACT_NOT_FOUND);
        Mockito.verify(mListener, Mockito.times(1)).testRunFailed(failure1);
        // run cmd2 test
        Mockito.verify(mListener, Mockito.times(0)).testRunStarted(eq(testName2), eq(1));
        FailureDescription failure2 =
                FailureDescription.create(
                                String.format(ExecutableBaseTest.NO_BINARY_ERROR, testCmd2),
                                FailureStatus.TEST_FAILURE)
                        .setErrorIdentifier(InfraErrorIdentifier.ARTIFACT_NOT_FOUND);
        Mockito.verify(mListener, Mockito.times(1)).testRunFailed(failure2);
        Mockito.verify(mListener, Mockito.times(2))
                .testRunEnded(
                        Mockito.anyLong(),
                        Mockito.<HashMap<String, MetricMeasurement.Metric>>any());
    }

    /** Test the run method for a couple commands and commands failed. */
    @Test
    public void testRun_cmdFailed() throws DeviceNotAvailableException, ConfigurationException {
        mExecutableTargetTest =
                new ExecutableTargetTest() {
                    @Override
                    public String findBinary(String binary) {
                        return binary;
                    }

                    @Override
                    protected void checkCommandResult(
                            CommandResult result,
                            ITestInvocationListener listener,
                            TestDescription description) {
                        listener.testFailed(description, ERROR_MESSAGE);
                    }
                };
        mExecutableTargetTest.setDevice(mMockITestDevice);
        // Set test commands
        OptionSetter setter = new OptionSetter(mExecutableTargetTest);
        setter.setOptionValue("test-command-line", testName1, testCmd1);
        setter.setOptionValue("test-command-line", testName2, testCmd2);
        mExecutableTargetTest.run(mTestInfo, mListener);
        // run cmd1 test
        TestDescription testDescription = new TestDescription(testName1, testName1);
        Mockito.verify(mListener, Mockito.times(1)).testRunStarted(eq(testName1), eq(1));
        Mockito.verify(mListener, Mockito.times(1)).testStarted(Mockito.eq(testDescription));
        Mockito.verify(mListener, Mockito.times(1)).testFailed(testDescription, ERROR_MESSAGE);
        Mockito.verify(mListener, Mockito.times(1))
                .testEnded(
                        Mockito.eq(testDescription),
                        Mockito.eq(new HashMap<String, MetricMeasurement.Metric>()));
        // run cmd2 test
        TestDescription testDescription2 = new TestDescription(testName2, testName2);
        Mockito.verify(mListener, Mockito.times(1)).testRunStarted(eq(testName2), eq(1));
        Mockito.verify(mListener, Mockito.times(1)).testStarted(Mockito.eq(testDescription2));
        Mockito.verify(mListener, Mockito.times(1)).testFailed(testDescription2, ERROR_MESSAGE);
        Mockito.verify(mListener, Mockito.times(1))
                .testEnded(
                        Mockito.eq(testDescription2),
                        Mockito.eq(new HashMap<String, MetricMeasurement.Metric>()));
        Mockito.verify(mListener, Mockito.times(2))
                .testRunEnded(
                        Mockito.anyLong(),
                        Mockito.<HashMap<String, MetricMeasurement.Metric>>any());
    }

    /** Test the run method for a couple commands with ExcludeFilters */
    @Test
    public void testRun_addExcludeFilter()
            throws DeviceNotAvailableException, ConfigurationException {
        mExecutableTargetTest =
                new ExecutableTargetTest() {
                    @Override
                    public String findBinary(String binary) {
                        return binary;
                    }

                    @Override
                    protected void checkCommandResult(
                            CommandResult result,
                            ITestInvocationListener listener,
                            TestDescription description) {}
                };
        mExecutableTargetTest.setDevice(mMockITestDevice);
        // Set test commands
        OptionSetter setter = new OptionSetter(mExecutableTargetTest);
        setter.setOptionValue("test-command-line", testName1, testCmd1);
        setter.setOptionValue("test-command-line", testName2, testCmd2);
        setter.setOptionValue("test-command-line", testName3, testCmd3);
        TestDescription testDescription = new TestDescription(testName1, testName1);
        TestDescription testDescription2 = new TestDescription(testName2, testName2);
        TestDescription testDescription3 = new TestDescription(testName3, testName3);
        mExecutableTargetTest.addExcludeFilter(testName1);
        mExecutableTargetTest.addExcludeFilter(testDescription3.toString());
        mExecutableTargetTest.run(mTestInfo, mListener);
        // testName1 should NOT run.
        Mockito.verify(mListener, Mockito.never()).testRunStarted(eq(testName1), eq(1));
        Mockito.verify(mListener, Mockito.never()).testStarted(Mockito.eq(testDescription));
        Mockito.verify(mListener, Mockito.never())
                .testEnded(
                        Mockito.eq(testDescription),
                        Mockito.eq(new HashMap<String, MetricMeasurement.Metric>()));
        // run cmd2 test
        Mockito.verify(mListener, Mockito.times(1)).testRunStarted(eq(testName2), eq(1));
        Mockito.verify(mListener, Mockito.times(1)).testStarted(Mockito.eq(testDescription2));
        Mockito.verify(mListener, Mockito.times(1))
                .testEnded(
                        Mockito.eq(testDescription2),
                        Mockito.eq(new HashMap<String, MetricMeasurement.Metric>()));
        Mockito.verify(mListener, Mockito.times(1))
                .testRunEnded(
                        Mockito.anyLong(),
                        Mockito.<HashMap<String, MetricMeasurement.Metric>>any());
        // testName3 should NOT run.
        Mockito.verify(mListener, Mockito.never()).testRunStarted(eq(testName3), eq(1));
        Mockito.verify(mListener, Mockito.never()).testStarted(Mockito.eq(testDescription3));
        Mockito.verify(mListener, Mockito.never())
                .testEnded(
                        Mockito.eq(testDescription3),
                        Mockito.eq(new HashMap<String, MetricMeasurement.Metric>()));
    }

    /** Test the run method for a couple commands with IncludeFilter */
    @Test
    public void testRun_addIncludeFilter()
            throws DeviceNotAvailableException, ConfigurationException {
        mExecutableTargetTest =
                new ExecutableTargetTest() {
                    @Override
                    public String findBinary(String binary) {
                        return binary;
                    }

                    @Override
                    protected void checkCommandResult(
                            CommandResult result,
                            ITestInvocationListener listener,
                            TestDescription description) {}
                };
        mExecutableTargetTest.setDevice(mMockITestDevice);
        // Set test commands
        OptionSetter setter = new OptionSetter(mExecutableTargetTest);
        setter.setOptionValue("test-command-line", testName1, testCmd1);
        setter.setOptionValue("test-command-line", testName2, testCmd2);
        setter.setOptionValue("test-command-line", testName3, testCmd3);
        TestDescription testDescription = new TestDescription(testName1, testName1);
        TestDescription testDescription2 = new TestDescription(testName2, testName2);
        TestDescription testDescription3 = new TestDescription(testName3, testName3);
        mExecutableTargetTest.addIncludeFilter(testName2);
        mExecutableTargetTest.run(mTestInfo, mListener);
        // testName1 should NOT run.
        Mockito.verify(mListener, Mockito.never()).testRunStarted(eq(testName1), eq(1));
        Mockito.verify(mListener, Mockito.never()).testStarted(Mockito.eq(testDescription));
        Mockito.verify(mListener, Mockito.never())
                .testEnded(
                        Mockito.eq(testDescription),
                        Mockito.eq(new HashMap<String, MetricMeasurement.Metric>()));
        // run cmd2 test
        Mockito.verify(mListener, Mockito.times(1)).testRunStarted(eq(testName2), eq(1));
        Mockito.verify(mListener, Mockito.times(1)).testStarted(Mockito.eq(testDescription2));
        Mockito.verify(mListener, Mockito.times(1))
                .testEnded(
                        Mockito.eq(testDescription2),
                        Mockito.eq(new HashMap<String, MetricMeasurement.Metric>()));
        Mockito.verify(mListener, Mockito.times(1))
                .testRunEnded(
                        Mockito.anyLong(),
                        Mockito.<HashMap<String, MetricMeasurement.Metric>>any());
        // testName3 should NOT run.
        Mockito.verify(mListener, Mockito.never()).testRunStarted(eq(testName3), eq(1));
        Mockito.verify(mListener, Mockito.never()).testStarted(Mockito.eq(testDescription3));
        Mockito.verify(mListener, Mockito.never())
                .testEnded(
                        Mockito.eq(testDescription3),
                        Mockito.eq(new HashMap<String, MetricMeasurement.Metric>()));
    }

    /** Test the run method for a couple commands with IncludeFilter */
    @Test
    public void testRun_add_description_IncludeFilter()
            throws DeviceNotAvailableException, ConfigurationException {
        mExecutableTargetTest =
                new ExecutableTargetTest() {
                    @Override
                    public String findBinary(String binary) {
                        return binary;
                    }

                    @Override
                    protected void checkCommandResult(
                            CommandResult result,
                            ITestInvocationListener listener,
                            TestDescription description) {}
                };
        mExecutableTargetTest.setDevice(mMockITestDevice);
        // Set test commands
        OptionSetter setter = new OptionSetter(mExecutableTargetTest);
        setter.setOptionValue("test-command-line", testName1, testCmd1);
        setter.setOptionValue("test-command-line", testName2, testCmd2);
        setter.setOptionValue("test-command-line", testName3, testCmd3);
        TestDescription testDescription = new TestDescription(testName1, testName1);
        TestDescription testDescription2 = new TestDescription(testName2, testName2);
        TestDescription testDescription3 = new TestDescription(testName3, testName3);
        mExecutableTargetTest.addIncludeFilter(testDescription2.toString());
        mExecutableTargetTest.run(mTestInfo, mListener);
        // testName1 should NOT run.
        Mockito.verify(mListener, Mockito.never()).testRunStarted(eq(testName1), eq(1));
        Mockito.verify(mListener, Mockito.never()).testStarted(Mockito.eq(testDescription));
        Mockito.verify(mListener, Mockito.never())
                .testEnded(
                        Mockito.eq(testDescription),
                        Mockito.eq(new HashMap<String, MetricMeasurement.Metric>()));
        // run cmd2 test
        Mockito.verify(mListener, Mockito.times(1)).testRunStarted(eq(testName2), eq(1));
        Mockito.verify(mListener, Mockito.times(1)).testStarted(Mockito.eq(testDescription2));
        Mockito.verify(mListener, Mockito.times(1))
                .testEnded(
                        Mockito.eq(testDescription2),
                        Mockito.eq(new HashMap<String, MetricMeasurement.Metric>()));
        Mockito.verify(mListener, Mockito.times(1))
                .testRunEnded(
                        Mockito.anyLong(),
                        Mockito.<HashMap<String, MetricMeasurement.Metric>>any());
        // testName3 should NOT run.
        Mockito.verify(mListener, Mockito.never()).testRunStarted(eq(testName3), eq(1));
        Mockito.verify(mListener, Mockito.never()).testStarted(Mockito.eq(testDescription3));
        Mockito.verify(mListener, Mockito.never())
                .testEnded(
                        Mockito.eq(testDescription3),
                        Mockito.eq(new HashMap<String, MetricMeasurement.Metric>()));
    }

    /** Test split() for sharding */
    @Test
    public void testShard_Split() throws ConfigurationException {
        mExecutableTargetTest = new ExecutableTargetTest();
        // Set test commands
        OptionSetter setter = new OptionSetter(mExecutableTargetTest);
        setter.setOptionValue("test-command-line", testName1, testCmd1);
        setter.setOptionValue("test-command-line", testName2, testCmd2);
        setter.setOptionValue("test-command-line", testName3, testCmd3);
        // Split the shard.
        Collection<IRemoteTest> testShards = mExecutableTargetTest.split();
        // Test the size of the test Shard.
        assertEquals(3, testShards.size());
        // Test the command of each shard.
        for (IRemoteTest test : testShards) {
            Map<String, String> TestCommands = ((ExecutableTargetTest) test).getTestCommands();
            String cmd1 = TestCommands.get(testName1);
            if (cmd1 != null) assertEquals(testCmd1, cmd1);
            String cmd2 = TestCommands.get(testName2);
            if (cmd2 != null) assertEquals(testCmd2, cmd2);
            String cmd3 = TestCommands.get(testName3);
            if (cmd3 != null) assertEquals(testCmd3, cmd3);
            // The test command should equals to one of them.
            assertEquals(true, cmd1 != null || cmd2 != null || cmd3 != null);
        }
    }
}
