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

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.command.ICommandOptions;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.ConfigurationDescriptor;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationFactory;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.IDeviceSelection;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.IRescheduler;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.log.FileLogger;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.testtype.Abi;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.testtype.suite.BaseTestSuite;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.Set;

/** Unit tests for {@link RetryRescheduler}. */
@RunWith(JUnit4.class)
public class RetryReschedulerTest {

    private RetryRescheduler mTest;
    private IConfiguration mTopConfiguration;
    private IConfiguration mRescheduledConfiguration;
    private ICommandOptions mMockCommandOptions;
    private IDeviceSelection mMockRequirements;

    private ITestSuiteResultLoader mMockLoader;
    private IRescheduler mMockRescheduler;
    private IConfigurationFactory mMockFactory;
    private BaseTestSuite mSuite;

    private CollectingTestListener mFakeRecord;

    @Before
    public void setUp() throws Exception {
        mTest = new RetryRescheduler();
        mTopConfiguration = new Configuration("test", "test");
        mMockCommandOptions = EasyMock.createMock(ICommandOptions.class);
        mMockRequirements = EasyMock.createMock(IDeviceSelection.class);
        mRescheduledConfiguration = EasyMock.createMock(IConfiguration.class);
        EasyMock.expect(mRescheduledConfiguration.getCommandOptions())
                .andStubReturn(mMockCommandOptions);
        EasyMock.expect(mRescheduledConfiguration.getDeviceRequirements())
                .andStubReturn(mMockRequirements);
        mRescheduledConfiguration.setDeviceRequirements(EasyMock.anyObject());
        EasyMock.expectLastCall();
        EasyMock.expect(mRescheduledConfiguration.getLogOutput()).andStubReturn(new FileLogger());
        mMockLoader = EasyMock.createMock(ITestSuiteResultLoader.class);
        mMockRescheduler = EasyMock.createMock(IRescheduler.class);
        mMockFactory = EasyMock.createMock(IConfigurationFactory.class);
        mTopConfiguration.setConfigurationObject(
                RetryRescheduler.PREVIOUS_LOADER_NAME, mMockLoader);
        mTest.setConfiguration(mTopConfiguration);
        mTest.setRescheduler(mMockRescheduler);
        mTest.setConfigurationFactory(mMockFactory);

        mSuite = Mockito.mock(BaseTestSuite.class);
        EasyMock.expect(mRescheduledConfiguration.getTests()).andStubReturn(Arrays.asList(mSuite));

        mMockLoader.cleanUp();
        EasyMock.expectLastCall();

        mMockLoader.customizeConfiguration(EasyMock.anyObject());
        EasyMock.expectLastCall();

        mMockCommandOptions.setShardCount(null);
        mMockCommandOptions.setShardIndex(null);
    }

    /** Test rescheduling a tests that only had pass tests in the first run. */
    @Test
    public void testReschedule_onlyPassTests() throws Exception {
        populateFakeResults(2, 2, 0, 0, 0, false);
        mMockLoader.init();
        EasyMock.expect(mMockLoader.getCommandLine()).andReturn("previous_command");
        EasyMock.expect(mMockFactory.createConfigurationFromArgs(EasyMock.anyObject()))
                .andReturn(mRescheduledConfiguration);
        EasyMock.expect(mMockLoader.loadPreviousResults()).andReturn(mFakeRecord);

        mRescheduledConfiguration.setTests(EasyMock.anyObject());
        EasyMock.expectLastCall().times(1);

        EasyMock.expect(mMockRescheduler.scheduleConfig(mRescheduledConfiguration)).andReturn(true);
        EasyMock.replay(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions,
                mMockRequirements);
        mTest.run(null, null);
        EasyMock.verify(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions,
                mMockRequirements);

        Set<String> excludeRun0 = new HashSet<>();
        excludeRun0.add("run0");
        verify(mSuite).setExcludeFilter(excludeRun0);
        Set<String> excludeRun1 = new HashSet<>();
        excludeRun1.add("run1");
        verify(mSuite).setExcludeFilter(excludeRun1);
    }

    /** Test rescheduling a tests where we request a new shard-count */
    @Test
    public void testReschedule_carryShardCount() throws Exception {
        mTopConfiguration.getCommandOptions().setShardCount(2);
        mTopConfiguration.getDeviceRequirements().setSerial("serial1", "serial2");
        populateFakeResults(2, 2, 0, 0, 0, false);
        mMockLoader.init();
        EasyMock.expect(mMockLoader.getCommandLine()).andReturn("previous_command");
        EasyMock.expect(mMockFactory.createConfigurationFromArgs(EasyMock.anyObject()))
                .andReturn(mRescheduledConfiguration);
        EasyMock.expect(mMockLoader.loadPreviousResults()).andReturn(mFakeRecord);
        // Shard count is carried from retry attempt
        EasyMock.reset(mMockCommandOptions);
        mMockCommandOptions.setShardCount(2);
        mMockCommandOptions.setShardIndex(null);

        mRescheduledConfiguration.setTests(EasyMock.anyObject());
        EasyMock.expectLastCall().times(1);

        EasyMock.expect(mMockRescheduler.scheduleConfig(mRescheduledConfiguration)).andReturn(true);
        EasyMock.replay(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions,
                mMockRequirements);
        mTest.run(null, null);
        EasyMock.verify(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions,
                mMockRequirements);

        Set<String> excludeRun0 = new HashSet<>();
        excludeRun0.add("run0");
        verify(mSuite).setExcludeFilter(excludeRun0);
        Set<String> excludeRun1 = new HashSet<>();
        excludeRun1.add("run1");
        verify(mSuite).setExcludeFilter(excludeRun1);
    }

    /** Test rescheduling a configuration when some tests previously failed. */
    @Test
    public void testReschedule_someFailedTests() throws Exception {
        populateFakeResults(2, 2, 1, 0, 0, false);
        mMockLoader.init();
        EasyMock.expect(mMockLoader.getCommandLine()).andReturn("previous_command");
        EasyMock.expect(mMockFactory.createConfigurationFromArgs(EasyMock.anyObject()))
                .andReturn(mRescheduledConfiguration);
        EasyMock.expect(mMockLoader.loadPreviousResults()).andReturn(mFakeRecord);

        mRescheduledConfiguration.setTests(EasyMock.anyObject());
        EasyMock.expectLastCall().times(1);

        EasyMock.expect(mMockRescheduler.scheduleConfig(mRescheduledConfiguration)).andReturn(true);
        EasyMock.replay(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);
        mTest.run(null, null);
        EasyMock.verify(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);
        // Only the passing tests are excluded since we don't want to re-run them
        Set<String> excludeRun0 = new HashSet<>();
        excludeRun0.add("run0 test.class#testPass0");
        verify(mSuite).setExcludeFilter(excludeRun0);
        Set<String> excludeRun1 = new HashSet<>();
        excludeRun1.add("run1 test.class#testPass0");
        verify(mSuite).setExcludeFilter(excludeRun1);
    }

    /**
     * Test rescheduling a configuration when some tests previously failed with assumption failures,
     * these tests will not be re-run.
     */
    @Test
    public void testReschedule_someAssumptionFailures() throws Exception {
        populateFakeResults(2, 2, 0, 1, 0, false);
        mMockLoader.init();
        EasyMock.expect(mMockLoader.getCommandLine()).andReturn("previous_command");
        EasyMock.expect(mMockFactory.createConfigurationFromArgs(EasyMock.anyObject()))
                .andReturn(mRescheduledConfiguration);
        EasyMock.expect(mMockLoader.loadPreviousResults()).andReturn(mFakeRecord);

        mRescheduledConfiguration.setTests(EasyMock.anyObject());
        EasyMock.expectLastCall().times(1);

        EasyMock.expect(mMockRescheduler.scheduleConfig(mRescheduledConfiguration)).andReturn(true);
        EasyMock.replay(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);
        mTest.run(null, null);
        EasyMock.verify(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);
        // Only the passing tests are excluded since we don't want to re-run them
        Set<String> excludeRun0 = new HashSet<>();
        excludeRun0.add("run0");
        verify(mSuite).setExcludeFilter(excludeRun0);
        Set<String> excludeRun1 = new HashSet<>();
        excludeRun1.add("run1");
        verify(mSuite).setExcludeFilter(excludeRun1);
    }

    /**
     * Test rescheduling a configuration when some mix of failures and assumption failures are
     * present. We reschedule the module with the passed and assumption failure tests excluded.
     */
    @Test
    public void testReschedule_mixedFailedAssumptionFailures() throws Exception {
        populateFakeResults(2, 3, 1, 1, 0, false);
        mMockLoader.init();
        EasyMock.expect(mMockLoader.getCommandLine()).andReturn("previous_command");
        EasyMock.expect(mMockFactory.createConfigurationFromArgs(EasyMock.anyObject()))
                .andReturn(mRescheduledConfiguration);
        EasyMock.expect(mMockLoader.loadPreviousResults()).andReturn(mFakeRecord);

        mRescheduledConfiguration.setTests(EasyMock.anyObject());
        EasyMock.expectLastCall().times(1);

        EasyMock.expect(mMockRescheduler.scheduleConfig(mRescheduledConfiguration)).andReturn(true);
        EasyMock.replay(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);
        mTest.run(null, null);
        EasyMock.verify(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);
        // Only the passing and assumption failures are excluded
        Set<String> excludeRun0 = new HashSet<>();
        excludeRun0.add("run0 test.class#testPass0");
        verify(mSuite).setExcludeFilter(excludeRun0);
        Set<String> excludeRun0_assume = new HashSet<>();
        excludeRun0_assume.add("run0 test.class#testAssume0");
        verify(mSuite).setExcludeFilter(excludeRun0_assume);

        Set<String> excludeRun1 = new HashSet<>();
        excludeRun1.add("run1 test.class#testPass0");
        verify(mSuite).setExcludeFilter(excludeRun1);
        Set<String> excludeRun1_assume = new HashSet<>();
        excludeRun1_assume.add("run1 test.class#testAssume0");
        verify(mSuite).setExcludeFilter(excludeRun1_assume);
    }

    /** Test when an extra exclude-filter is provided. */
    @Test
    public void testReschedule_excludeFilters() throws Exception {
        OptionSetter setter = new OptionSetter(mTest);
        setter.setOptionValue(BaseTestSuite.EXCLUDE_FILTER_OPTION, "run1");
        populateFakeResults(2, 2, 1, 0, 0, false);
        mMockLoader.init();
        EasyMock.expect(mMockLoader.getCommandLine()).andReturn("previous_command");
        EasyMock.expect(mMockFactory.createConfigurationFromArgs(EasyMock.anyObject()))
                .andReturn(mRescheduledConfiguration);
        EasyMock.expect(mMockLoader.loadPreviousResults()).andReturn(mFakeRecord);

        mRescheduledConfiguration.setTests(EasyMock.anyObject());
        EasyMock.expectLastCall().times(1);

        EasyMock.expect(mMockRescheduler.scheduleConfig(mRescheduledConfiguration)).andReturn(true);
        EasyMock.replay(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);
        mTest.run(null, null);
        EasyMock.verify(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);

        Set<String> excludeRun0 = new HashSet<>();
        excludeRun0.add("run0 test.class#testPass0");
        verify(mSuite).setExcludeFilter(excludeRun0);
        Set<String> excludeRun1 = new HashSet<>();
        // Even if run1 had failed test cases, it was excluded so it's not running.
        excludeRun1.add("run1");
        verify(mSuite).setExcludeFilter(excludeRun1);
    }

    /** Ensure that the --module option can be used to force a single module to run. */
    @Test
    public void testReschedule_module_option() throws Exception {
        OptionSetter setter = new OptionSetter(mTest);
        setter.setOptionValue(BaseTestSuite.MODULE_OPTION, "run0");
        populateFakeResults(2, 2, 1, 0, 0, false);
        mMockLoader.init();
        EasyMock.expect(mMockLoader.getCommandLine()).andReturn("previous_command");
        EasyMock.expect(mMockFactory.createConfigurationFromArgs(EasyMock.anyObject()))
                .andReturn(mRescheduledConfiguration);
        EasyMock.expect(mMockLoader.loadPreviousResults()).andReturn(mFakeRecord);

        mRescheduledConfiguration.setTests(EasyMock.anyObject());
        EasyMock.expectLastCall().times(1);

        EasyMock.expect(mMockRescheduler.scheduleConfig(mRescheduledConfiguration)).andReturn(true);
        EasyMock.replay(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);
        mTest.run(null, null);
        EasyMock.verify(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);

        Set<String> excludeRun0 = new HashSet<>();
        excludeRun0.add("run0 test.class#testPass0");
        verify(mSuite).setExcludeFilter(excludeRun0);
        Set<String> excludeRun1 = new HashSet<>();
        // Only run0 was requested to run.
        excludeRun1.add("run1");
        verify(mSuite).setExcludeFilter(excludeRun1);
    }

    /**
     * Test that if an exclude-filter is provided without abi, we are still able to exclude all the
     * matching modules for all abis.
     */
    @Test
    public void testReschedule_excludeFilters_abi() throws Exception {
        OptionSetter setter = new OptionSetter(mTest);
        // We specify to exclude "run1"
        setter.setOptionValue(BaseTestSuite.EXCLUDE_FILTER_OPTION, "run1");
        populateFakeResults(2, 2, 1, 0, 0, false, new Abi("armeabi-v7a", "32"));
        mMockLoader.init();
        EasyMock.expect(mMockLoader.getCommandLine()).andReturn("previous_command");
        EasyMock.expect(mMockFactory.createConfigurationFromArgs(EasyMock.anyObject()))
                .andReturn(mRescheduledConfiguration);
        EasyMock.expect(mMockLoader.loadPreviousResults()).andReturn(mFakeRecord);

        mRescheduledConfiguration.setTests(EasyMock.anyObject());
        EasyMock.expectLastCall().times(1);

        EasyMock.expect(mMockRescheduler.scheduleConfig(mRescheduledConfiguration)).andReturn(true);
        EasyMock.replay(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);
        mTest.run(null, null);
        EasyMock.verify(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);

        Set<String> excludeRun0 = new HashSet<>();
        // Run with the abi are excluded
        excludeRun0.add("armeabi-v7a run0 test.class#testPass0");
        verify(mSuite).setExcludeFilter(excludeRun0);
        Set<String> excludeRun1 = new HashSet<>();
        // Even if run1 had failed test cases, it was excluded so it's not running.
        excludeRun1.add("armeabi-v7a run1");
        verify(mSuite).setExcludeFilter(excludeRun1);
    }

    /** Ensure that --module works when abi are present. */
    @Test
    public void testReschedule_moduleOption_abi() throws Exception {
        OptionSetter setter = new OptionSetter(mTest);
        // We specify to exclude "run1"
        setter.setOptionValue(BaseTestSuite.MODULE_OPTION, "run0");
        populateFakeResults(2, 2, 1, 0, 0, false, new Abi("armeabi-v7a", "32"));
        mMockLoader.init();
        EasyMock.expect(mMockLoader.getCommandLine()).andReturn("previous_command");
        EasyMock.expect(mMockFactory.createConfigurationFromArgs(EasyMock.anyObject()))
                .andReturn(mRescheduledConfiguration);
        EasyMock.expect(mMockLoader.loadPreviousResults()).andReturn(mFakeRecord);

        mRescheduledConfiguration.setTests(EasyMock.anyObject());
        EasyMock.expectLastCall().times(1);

        EasyMock.expect(mMockRescheduler.scheduleConfig(mRescheduledConfiguration)).andReturn(true);
        EasyMock.replay(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);
        mTest.run(null, null);
        EasyMock.verify(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);

        Set<String> excludeRun0 = new HashSet<>();
        // Run with the abi are excluded
        excludeRun0.add("armeabi-v7a run0 test.class#testPass0");
        verify(mSuite).setExcludeFilter(excludeRun0);
        Set<String> excludeRun1 = new HashSet<>();
        // Even if run1 had failed test cases, it was excluded so it's not running.
        excludeRun1.add("armeabi-v7a run1");
        verify(mSuite).setExcludeFilter(excludeRun1);
    }

    /** Test rescheduling a configuration when no parameterized tests previously failed. */
    @Test
    public void testReschedule_parameterized_nofail() throws Exception {
        populateFakeResults(2, 4, 1, 0, 2, false);
        mMockLoader.init();
        EasyMock.expect(mMockLoader.getCommandLine()).andReturn("previous_command");
        EasyMock.expect(mMockFactory.createConfigurationFromArgs(EasyMock.anyObject()))
                .andReturn(mRescheduledConfiguration);
        EasyMock.expect(mMockLoader.loadPreviousResults()).andReturn(mFakeRecord);

        mRescheduledConfiguration.setTests(EasyMock.anyObject());
        EasyMock.expectLastCall().times(1);

        EasyMock.expect(mMockRescheduler.scheduleConfig(mRescheduledConfiguration)).andReturn(true);
        EasyMock.replay(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);
        mTest.run(null, null);
        EasyMock.verify(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);
        // Only the passing tests are excluded since we don't want to re-run them
        Set<String> excludeRun0_pass = new LinkedHashSet<>();
        excludeRun0_pass.add("run0 test.class#testPass0");
        verify(mSuite).setExcludeFilter(excludeRun0_pass);
        Set<String> excludeRun0_0 = new LinkedHashSet<>();
        excludeRun0_0.add("run0 test.class#parameterized0");
        verify(mSuite, times(1)).setExcludeFilter(excludeRun0_0);
        Set<String> excludeRun0_1 = new LinkedHashSet<>();
        excludeRun0_1.add("run0 test.class#parameterized1");
        verify(mSuite, times(1)).setExcludeFilter(excludeRun0_1);

        Set<String> excludeRun1_pass = new LinkedHashSet<>();
        excludeRun1_pass.add("run1 test.class#testPass0");
        verify(mSuite).setExcludeFilter(excludeRun1_pass);
        Set<String> excludeRun1_0 = new LinkedHashSet<>();
        excludeRun1_0.add("run1 test.class#parameterized0");
        verify(mSuite, times(1)).setExcludeFilter(excludeRun1_0);
        Set<String> excludeRun1_1 = new LinkedHashSet<>();
        excludeRun1_1.add("run1 test.class#parameterized1");
        verify(mSuite, times(1)).setExcludeFilter(excludeRun1_1);
    }

    /** Test rescheduling a configuration when some parameterized tests previously failed. */
    @Test
    public void testReschedule_parameterized_failed() throws Exception {
        populateFakeResults(2, 4, 1, 0, 2, true);
        mMockLoader.init();
        EasyMock.expect(mMockLoader.getCommandLine()).andReturn("previous_command");
        EasyMock.expect(mMockFactory.createConfigurationFromArgs(EasyMock.anyObject()))
                .andReturn(mRescheduledConfiguration);
        EasyMock.expect(mMockLoader.loadPreviousResults()).andReturn(mFakeRecord);

        mRescheduledConfiguration.setTests(EasyMock.anyObject());
        EasyMock.expectLastCall().times(1);

        EasyMock.expect(mMockRescheduler.scheduleConfig(mRescheduledConfiguration)).andReturn(true);
        EasyMock.replay(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);
        mTest.run(null, null);
        EasyMock.verify(
                mMockRescheduler,
                mMockLoader,
                mMockFactory,
                mRescheduledConfiguration,
                mMockCommandOptions);
        // Only the passing tests are excluded since we don't want to re-run them
        Set<String> excludeRun0_pass = new LinkedHashSet<>();
        excludeRun0_pass.add("run0 test.class#testPass0");
        verify(mSuite).setExcludeFilter(excludeRun0_pass);
        Set<String> excludeRun0_0 = new LinkedHashSet<>();

        excludeRun0_0.add("run0 test.class#parameterized0[1]");
        verify(mSuite, times(0)).setExcludeFilter(excludeRun0_0);
        Set<String> excludeRun0_1 = new LinkedHashSet<>();
        excludeRun0_1.add("run0 test.class#parameterized1[1]");
        verify(mSuite, times(0)).setExcludeFilter(excludeRun0_1);

        Set<String> excludeRun1_pass = new LinkedHashSet<>();
        excludeRun1_pass.add("run1 test.class#testPass0");
        verify(mSuite).setExcludeFilter(excludeRun1_pass);

        Set<String> excludeRun1_0 = new LinkedHashSet<>();
        excludeRun1_0.add("run1 test.class#parameterized0[1]");
        verify(mSuite, times(0)).setExcludeFilter(excludeRun1_0);
        Set<String> excludeRun1_1 = new LinkedHashSet<>();
        excludeRun1_1.add("run1 test.class#parameterized1[1]");
        verify(mSuite, times(0)).setExcludeFilter(excludeRun1_1);
    }

    private void populateFakeResults(
            int numModule,
            int numTests,
            int failedTests,
            int assumpFailure,
            int parameterized,
            boolean failedParam) {
        populateFakeResults(
                numModule, numTests, failedTests, assumpFailure, parameterized, failedParam, null);
    }

    private void populateFakeResults(
            int numModule,
            int numTests,
            int failedTests,
            int assumpFailure,
            int parameterized,
            boolean failedParam,
            IAbi abi) {
        CollectingTestListener reporter = new CollectingTestListener();
        IInvocationContext context = new InvocationContext();
        context.setConfigurationDescriptor(new ConfigurationDescriptor());
        context.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, new BuildInfo());
        reporter.invocationStarted(context);
        for (int i = 0; i < numModule; i++) {
            String runName = "run" + i;
            if (abi != null) {
                runName = abi.getName() + " " + runName;
            }
            reporter.testRunStarted(runName, numTests);
            for (int j = 0; j < numTests - failedTests - assumpFailure - parameterized; j++) {
                TestDescription test = new TestDescription("test.class", "testPass" + j);
                reporter.testStarted(test);
                reporter.testEnded(test, new HashMap<String, Metric>());
            }
            for (int f = 0; f < failedTests; f++) {
                TestDescription test = new TestDescription("test.class", "testFail" + f);
                reporter.testStarted(test);
                reporter.testFailed(test, "failure" + f);
                reporter.testEnded(test, new HashMap<String, Metric>());
            }
            for (int f = 0; f < parameterized; f++) {
                // First parameter fail
                TestDescription test =
                        new TestDescription("test.class", "parameterized" + f + "[0]");
                reporter.testStarted(test);
                if (failedParam) {
                    reporter.testFailed(test, "parameterized" + f);
                }
                reporter.testEnded(test, new HashMap<String, Metric>());
                // Second parameter pass
                TestDescription test1 =
                        new TestDescription("test.class", "parameterized" + f + "[1]");
                reporter.testStarted(test1);
                reporter.testEnded(test1, new HashMap<String, Metric>());
            }
            for (int f = 0; f < assumpFailure; f++) {
                TestDescription test = new TestDescription("test.class", "testAssume" + f);
                reporter.testStarted(test);
                reporter.testAssumptionFailure(test, "assume" + f);
                reporter.testEnded(test, new HashMap<String, Metric>());
            }
            reporter.testRunEnded(500L, new HashMap<String, Metric>());
        }
        reporter.invocationEnded(0L);
        mFakeRecord = reporter;
    }
}
