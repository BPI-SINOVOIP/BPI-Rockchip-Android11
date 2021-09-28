/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.tradefed.testtype.suite.retry;

import com.android.ddmlib.IDevice;
import com.android.ddmlib.Log.LogLevel;
import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.ILeveledLogOutput;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestResult;
import com.android.tradefed.result.TestRunResult;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.AbstractMap.SimpleEntry;
import java.util.HashMap;
import java.util.Map.Entry;

/** Run unit tests for {@link ResultsPlayer}. */
@RunWith(JUnit4.class)
public class ResultsPlayerTest {
    private ResultsPlayer mPlayer;
    private ITestInvocationListener mMockListener;
    private IInvocationContext mContext;
    private TestInformation mTestInfo;
    private ITestDevice mMockDevice;
    private IDevice mMockIDevice;
    private IConfiguration mMockConfig;
    private ILeveledLogOutput mMockLogOutput;

    @Before
    public void setUp() throws Exception {
        mContext = new InvocationContext();
        mMockListener = EasyMock.createStrictMock(ITestInvocationListener.class);
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mMockIDevice = EasyMock.createMock(IDevice.class);
        mMockConfig = EasyMock.createMock(IConfiguration.class);
        mMockLogOutput = EasyMock.createMock(ILeveledLogOutput.class);
        EasyMock.expect(mMockConfig.getLogOutput()).andStubReturn(mMockLogOutput);
        EasyMock.expect(mMockLogOutput.getLogLevel()).andReturn(LogLevel.VERBOSE);
        mMockLogOutput.setLogLevel(LogLevel.WARN);
        mMockLogOutput.setLogLevel(LogLevel.VERBOSE);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(mContext).build();

        mPlayer = new ResultsPlayer();
        mPlayer.setConfiguration(mMockConfig);
        mContext.addAllocatedDevice(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockDevice);

        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice);
        mMockDevice.waitForDeviceAvailable();
        EasyMock.expectLastCall();
    }

    /** Test that the replay of a full test run is properly working. */
    @Test
    public void testReplay() throws DeviceNotAvailableException {
        mPlayer.addToReplay(null, createTestRunResult("run1", 2, 1), null);

        // Verify Mock
        mMockListener.testRunStarted("run1", 2);
        TestDescription test = new TestDescription("test.class", "method0");
        mMockListener.testStarted(EasyMock.eq(test), EasyMock.anyLong());
        mMockListener.testEnded(
                EasyMock.eq(test), EasyMock.anyLong(), EasyMock.eq(new HashMap<String, Metric>()));
        TestDescription testFail = new TestDescription("test.class", "fail0");
        mMockListener.testStarted(EasyMock.eq(testFail), EasyMock.anyLong());
        mMockListener.testFailed(testFail, "fail0");
        mMockListener.testEnded(
                EasyMock.eq(testFail),
                EasyMock.anyLong(),
                EasyMock.eq(new HashMap<String, Metric>()));
        mMockListener.testRunEnded(500L, new HashMap<String, Metric>());

        EasyMock.replay(mMockListener, mMockDevice, mMockConfig, mMockLogOutput);
        mPlayer.run(mTestInfo, mMockListener);
        EasyMock.verify(mMockListener, mMockDevice, mMockConfig, mMockLogOutput);
    }

    /** Test that when replaying a module we properly replay all the results. */
    @Test
    public void testReplayModules() throws DeviceNotAvailableException {
        IInvocationContext module1 = new InvocationContext();
        mPlayer.addToReplay(module1, createTestRunResult("run1", 2, 1), null);
        IInvocationContext module2 = new InvocationContext();
        mPlayer.addToReplay(module2, createTestRunResult("run2", 2, 1), null);

        // Verify Mock
        mMockListener.testModuleStarted(module1);
        mMockListener.testRunStarted("run1", 2);
        TestDescription test = new TestDescription("test.class", "method0");
        mMockListener.testStarted(EasyMock.eq(test), EasyMock.anyLong());
        mMockListener.testEnded(
                EasyMock.eq(test), EasyMock.anyLong(), EasyMock.eq(new HashMap<String, Metric>()));
        TestDescription testFail = new TestDescription("test.class", "fail0");
        mMockListener.testStarted(EasyMock.eq(testFail), EasyMock.anyLong());
        mMockListener.testFailed(testFail, "fail0");
        mMockListener.testEnded(
                EasyMock.eq(testFail),
                EasyMock.anyLong(),
                EasyMock.eq(new HashMap<String, Metric>()));
        mMockListener.testRunEnded(500L, new HashMap<String, Metric>());
        mMockListener.testModuleEnded();
        // Second module
        mMockListener.testModuleStarted(module2);
        mMockListener.testRunStarted("run2", 2);
        test = new TestDescription("test.class", "method0");
        mMockListener.testStarted(EasyMock.eq(test), EasyMock.anyLong());
        mMockListener.testEnded(
                EasyMock.eq(test), EasyMock.anyLong(), EasyMock.eq(new HashMap<String, Metric>()));
        testFail = new TestDescription("test.class", "fail0");
        mMockListener.testStarted(EasyMock.eq(testFail), EasyMock.anyLong());
        mMockListener.testFailed(testFail, "fail0");
        mMockListener.testEnded(
                EasyMock.eq(testFail),
                EasyMock.anyLong(),
                EasyMock.eq(new HashMap<String, Metric>()));
        mMockListener.testRunEnded(500L, new HashMap<String, Metric>());
        mMockListener.testModuleEnded();

        EasyMock.replay(mMockListener, mMockDevice, mMockConfig, mMockLogOutput);
        mPlayer.run(mTestInfo, mMockListener);
        EasyMock.verify(mMockListener, mMockDevice, mMockConfig, mMockLogOutput);
    }

    /** Test that the replay of a single requested test case is working. */
    @Test
    public void testReplay_oneTest() throws DeviceNotAvailableException {
        TestDescription test = new TestDescription("test.class", "method0");
        TestResult result = new TestResult();
        result.setStatus(TestStatus.ASSUMPTION_FAILURE);
        result.setStartTime(0L);
        result.setEndTime(10L);
        result.setStackTrace("assertionfailure");

        Entry<TestDescription, TestResult> entry = new SimpleEntry<>(test, result);
        mPlayer.addToReplay(null, createTestRunResult("run1", 2, 1), entry);

        // Verify Mock
        mMockListener.testRunStarted("run1", 1);
        // Only the provided test is re-run
        mMockListener.testStarted(EasyMock.eq(test), EasyMock.anyLong());
        mMockListener.testAssumptionFailure(test, "assertionfailure");
        mMockListener.testEnded(
                EasyMock.eq(test), EasyMock.anyLong(), EasyMock.eq(new HashMap<String, Metric>()));
        mMockListener.testRunEnded(500L, new HashMap<String, Metric>());

        EasyMock.replay(mMockListener, mMockDevice, mMockConfig, mMockLogOutput);
        mPlayer.run(mTestInfo, mMockListener);
        EasyMock.verify(mMockListener, mMockDevice, mMockConfig, mMockLogOutput);
    }

    /** Test requesting several tests to re-run. */
    @Test
    public void testReplay_MultiTest() throws DeviceNotAvailableException {
        TestRunResult runResult = createTestRunResult("run1", 5, 1);

        TestDescription test = new TestDescription("test.class", "method0");
        TestResult result = new TestResult();
        result.setStatus(TestStatus.ASSUMPTION_FAILURE);
        result.setStartTime(0L);
        result.setEndTime(10L);
        result.setStackTrace("assertionfailure");
        Entry<TestDescription, TestResult> entry = new SimpleEntry<>(test, result);
        mPlayer.addToReplay(null, runResult, entry);

        TestDescription test2 = new TestDescription("test.class", "fail0");
        TestResult result2 = new TestResult();
        result2.setStatus(TestStatus.FAILURE);
        result2.setStartTime(0L);
        result2.setEndTime(10L);
        result2.setStackTrace("fail0");
        Entry<TestDescription, TestResult> entry2 = new SimpleEntry<>(test2, result2);
        mPlayer.addToReplay(null, runResult, entry2);

        // Verify Mock
        mMockListener.testRunStarted("run1", 2);
        // Only the provided test is re-run
        mMockListener.testStarted(EasyMock.eq(test), EasyMock.anyLong());
        mMockListener.testAssumptionFailure(test, "assertionfailure");
        mMockListener.testEnded(
                EasyMock.eq(test), EasyMock.anyLong(), EasyMock.eq(new HashMap<String, Metric>()));

        mMockListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mMockListener.testFailed(test2, "fail0");
        mMockListener.testEnded(
                EasyMock.eq(test2), EasyMock.anyLong(), EasyMock.eq(new HashMap<String, Metric>()));

        mMockListener.testRunEnded(500L, new HashMap<String, Metric>());

        EasyMock.replay(mMockListener, mMockDevice, mMockConfig, mMockLogOutput);
        mPlayer.run(mTestInfo, mMockListener);
        EasyMock.verify(mMockListener, mMockDevice, mMockConfig, mMockLogOutput);
    }

    private TestRunResult createTestRunResult(String runName, int testCount, int failCount) {
        TestRunResult result = new TestRunResult();
        result.testRunStarted(runName, testCount);
        for (int i = 0; i < testCount - failCount; i++) {
            TestDescription test = new TestDescription("test.class", "method" + i);
            result.testStarted(test);
            result.testEnded(test, new HashMap<String, Metric>());
        }
        for (int i = 0; i < failCount; i++) {
            TestDescription test = new TestDescription("test.class", "fail" + i);
            result.testStarted(test);
            result.testFailed(test, "fail" + i);
            result.testEnded(test, new HashMap<String, Metric>());
        }
        result.testRunEnded(500L, new HashMap<String, Metric>());
        return result;
    }
}
