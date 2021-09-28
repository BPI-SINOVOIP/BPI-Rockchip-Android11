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
package com.android.tradefed.testtype.suite;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.ddmlib.IDevice;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationDescriptor;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.DynamicRemoteFileResolver;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.DeviceUnresponsiveException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.ITestDevice.RecoveryMode;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.invoker.TestInvocation;
import com.android.tradefed.invoker.shard.token.TokenProperty;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ILogSaver;
import com.android.tradefed.result.ILogSaverListener;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.LogSaverResultForwarder;
import com.android.tradefed.result.MultiFailureDescription;
import com.android.tradefed.result.ResultForwarder;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.result.error.DeviceErrorIdentifier;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.retry.BaseRetryDecision;
import com.android.tradefed.retry.IRetryDecision;
import com.android.tradefed.targetprep.BaseTargetPreparer;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.targetprep.multi.IMultiTargetPreparer;
import com.android.tradefed.testtype.Abi;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.ITestFilterReceiver;
import com.android.tradefed.testtype.suite.module.BaseModuleController;
import com.android.tradefed.testtype.suite.module.IModuleController;
import com.android.tradefed.testtype.suite.module.TestFailureModuleController;

import org.easymock.Capture;
import org.easymock.EasyMock;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;


/** Unit tests for {@link ModuleDefinition} */
@RunWith(JUnit4.class)
public class ModuleDefinitionTest {

    private static final String MODULE_NAME = "fakeName";
    private static final String DEFAULT_DEVICE_NAME = "DEFAULT_DEVICE";
    private ModuleDefinition mModule;
    private TestInformation mModuleInfo;
    private List<IRemoteTest> mTestList;
    private ITestInterface mMockTest;
    private ITargetPreparer mMockPrep;
    private List<ITargetPreparer> mTargetPrepList;
    private Map<String, List<ITargetPreparer>> mMapDeviceTargetPreparer;
    private List<IMultiTargetPreparer> mMultiTargetPrepList;
    private ITestInvocationListener mMockListener;
    private IBuildInfo mMockBuildInfo;
    private ITestDevice mMockDevice;
    // Extra mock for log saving testing
    private ILogSaver mMockLogSaver;
    private ILogSaverListener mMockLogSaverListener;

    private IRetryDecision mDecision = new BaseRetryDecision();

    private interface ITestInterface
            extends IRemoteTest, IBuildReceiver, IDeviceTest, IConfigurationReceiver {}

    /** Test implementation that allows us to exercise different use cases * */
    private class TestObject implements ITestInterface {

        private ITestDevice mDevice;
        private String mRunName;
        private int mNumTest;
        private boolean mShouldThrow;
        private boolean mDeviceUnresponsive = false;
        private boolean mThrowError = false;
        private IConfiguration mConfig;

        public TestObject(String runName, int numTest, boolean shouldThrow) {
            mRunName = runName;
            mNumTest = numTest;
            mShouldThrow = shouldThrow;
        }

        public TestObject(
                String runName, int numTest, boolean shouldThrow, boolean deviceUnresponsive) {
            this(runName, numTest, shouldThrow);
            mDeviceUnresponsive = deviceUnresponsive;
        }

        public TestObject(
                String runName,
                int numTest,
                boolean shouldThrow,
                boolean deviceUnresponsive,
                boolean throwError) {
            this(runName, numTest, shouldThrow, deviceUnresponsive);
            mThrowError = throwError;
        }

        @Override
        public void run(TestInformation testInfo, ITestInvocationListener listener)
                throws DeviceNotAvailableException {
            Assert.assertNotNull(mConfig);
            listener.testRunStarted(mRunName, mNumTest);
            for (int i = 0; i < mNumTest; i++) {
                TestDescription test = new TestDescription(mRunName + "class", "test" + i);
                listener.testStarted(test);
                if (mShouldThrow && i == mNumTest / 2) {
                    throw new DeviceNotAvailableException(
                            "unavailable", "serial", DeviceErrorIdentifier.DEVICE_UNAVAILABLE);
                }
                if (mDeviceUnresponsive) {
                    throw new DeviceUnresponsiveException(
                            "unresponsive", "serial", DeviceErrorIdentifier.DEVICE_UNRESPONSIVE);
                }
                if (mThrowError && i == mNumTest / 2) {
                    throw new AssertionError("assert error");
                }
                listener.testEnded(test, new HashMap<String, Metric>());
            }
            listener.testRunEnded(0, new HashMap<String, Metric>());
        }

        @Override
        public void setBuild(IBuildInfo buildInfo) {
            // ignore
        }

        @Override
        public void setDevice(ITestDevice device) {
            mDevice = device;
        }

        @Override
        public ITestDevice getDevice() {
            return mDevice;
        }

        @Override
        public void setConfiguration(IConfiguration configuration) {
            mConfig = configuration;
        }
    }

    /** Test implementation that allows us to exercise different use cases * */
    private class MultiRunTestObject implements IRemoteTest, ITestFilterReceiver {

        private String mBaseRunName;
        private int mNumTest;
        private int mRepeatedRun;
        private int mFailedTest;
        private Set<String> mIncludeFilters;

        public MultiRunTestObject(
                String baseRunName, int numTest, int repeatedRun, int failedTest) {
            mBaseRunName = baseRunName;
            mNumTest = numTest;
            mRepeatedRun = repeatedRun;
            mFailedTest = failedTest;
            mIncludeFilters = new LinkedHashSet<>();
        }

        @Override
        public void run(TestInformation testInfo, ITestInvocationListener listener)
                throws DeviceNotAvailableException {
            // The runner generates several set of different runs.
            for (int j = 0; j < mRepeatedRun; j++) {
                String runName = mBaseRunName + j;
                if (mIncludeFilters.isEmpty()) {
                    listener.testRunStarted(runName, mNumTest);
                } else {
                    listener.testRunStarted(runName, mIncludeFilters.size() / mRepeatedRun);
                }
                for (int i = 0; i < mNumTest - mFailedTest; i++) {
                    // TODO: Store the list of expected test cases to verify against it.
                    TestDescription test = new TestDescription(runName + "class", "test" + i);
                    if (!mIncludeFilters.isEmpty() && !mIncludeFilters.contains(test.toString())) {
                        continue;
                    }
                    listener.testStarted(test);
                    listener.testEnded(test, new HashMap<String, Metric>());
                }
                for (int i = 0; i < mFailedTest; i++) {
                    TestDescription test = new TestDescription(runName + "class", "fail" + i);
                    if (!mIncludeFilters.isEmpty() && !mIncludeFilters.contains(test.toString())) {
                        continue;
                    }
                    listener.testStarted(test);
                    listener.testFailed(
                            test,
                            FailureDescription.create("I failed.", FailureStatus.TEST_FAILURE));
                    listener.testEnded(test, new HashMap<String, Metric>());
                }
                listener.testRunEnded(0, new HashMap<String, Metric>());
            }
        }

        @Override
        public void addIncludeFilter(String filter) {
            mIncludeFilters.add(filter);
        }

        @Override
        public void addAllIncludeFilters(Set<String> filters) {
            mIncludeFilters.addAll(filters);
        }

        @Override
        public void addExcludeFilter(String filter) {}

        @Override
        public void addAllExcludeFilters(Set<String> filters) {}

        @Override
        public Set<String> getIncludeFilters() {
            return mIncludeFilters;
        }

        @Override
        public Set<String> getExcludeFilters() {
            return null;
        }

        @Override
        public void clearIncludeFilters() {
            mIncludeFilters.clear();
        }

        @Override
        public void clearExcludeFilters() {}
    }

    private class DirectFailureTestObject implements IRemoteTest {
        @Override
        public void run(TestInformation testInfo, ITestInvocationListener listener)
                throws DeviceNotAvailableException {
            throw new RuntimeException("early failure!");
        }
    }

    @BeforeClass
    public static void SetUpClass() throws ConfigurationException {
        try {
            GlobalConfiguration.createGlobalConfiguration(new String[] {"empty"});
        } catch (IllegalStateException e) {
            // Expected outside IDE.
        }
    }

    @Before
    public void setUp() {
        mMockLogSaver = EasyMock.createMock(ILogSaver.class);
        mMockLogSaverListener = EasyMock.createStrictMock(ILogSaverListener.class);

        mMockListener = EasyMock.createNiceMock(ITestInvocationListener.class);
        mTestList = new ArrayList<>();
        mMockTest = EasyMock.createNiceMock(ITestInterface.class);
        mTestList.add(mMockTest);
        mTargetPrepList = new ArrayList<>();
        mMockPrep = EasyMock.createNiceMock(ITargetPreparer.class);
        mTargetPrepList.add(mMockPrep);
        mMapDeviceTargetPreparer = new LinkedHashMap<>();
        mMapDeviceTargetPreparer.put(DEFAULT_DEVICE_NAME, mTargetPrepList);

        mMultiTargetPrepList = new ArrayList<>();
        mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        mTestList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.disableAutoRetryReportingTime();
        mModule.setRetryDecision(mDecision);
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
    }

    /** Helper for replaying mocks. */
    private void replayMocks() {
        EasyMock.replay(mMockListener, mMockLogSaver, mMockLogSaverListener, mMockDevice);
        for (IRemoteTest test : mTestList) {
            EasyMock.replay(test);
        }
        for (ITargetPreparer prep : mTargetPrepList) {
            try {
                EasyMock.replay(prep);
            } catch (IllegalArgumentException e) {
                // ignore
            }
        }
    }

    /** Helper for verifying mocks. */
    private void verifyMocks() {
        EasyMock.verify(mMockListener, mMockLogSaver, mMockLogSaverListener, mMockDevice);
        for (IRemoteTest test : mTestList) {
            EasyMock.verify(test);
        }
        for (ITargetPreparer prep : mTargetPrepList) {
            try {
                EasyMock.verify(prep);
            } catch (IllegalArgumentException e) {
                // ignore
            }
        }
    }

    @Test
    public void testCreateModule() {
        IConfiguration config = new Configuration("", "");
        ConfigurationDescriptor descriptor = config.getConfigurationDescription();
        descriptor.setAbi(new Abi("armeabi-v7a", "32"));
        descriptor.addMetadata(ITestSuite.PARAMETER_KEY, Arrays.asList("instant_app", "multi_abi"));
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        mTestList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        config);
        assertNotNull(mModule.getModuleInvocationContext());
        IInvocationContext moduleContext = mModule.getModuleInvocationContext();
        assertNull(moduleContext.getAttributes().get(ModuleDefinition.MODULE_PARAMETERIZATION));
    }

    @Test
    public void testCreateModule_withParams() {
        IConfiguration config = new Configuration("", "");
        ConfigurationDescriptor descriptor = config.getConfigurationDescription();
        descriptor.setAbi(new Abi("armeabi-v7a", "32"));
        descriptor.addMetadata(
                ConfigurationDescriptor.ACTIVE_PARAMETER_KEY, Arrays.asList("instant"));
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        mTestList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        config);
        assertNotNull(mModule.getModuleInvocationContext());
        IInvocationContext moduleContext = mModule.getModuleInvocationContext();
        assertEquals(
                1,
                moduleContext.getAttributes().get(ModuleDefinition.MODULE_PARAMETERIZATION).size());
        assertEquals(
                "instant",
                moduleContext
                        .getAttributes()
                        .getUniqueMap()
                        .get(ModuleDefinition.MODULE_PARAMETERIZATION));
    }

    /**
     * Test that {@link ModuleDefinition#run(TestInformation, ITestInvocationListener)} is properly
     * going through the execution flow.
     */
    @Test
    public void testRun() throws Exception {
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);
        EasyMock.expect(mMockPrep.isDisabled()).andReturn(false).times(2);
        mMockPrep.setUp(EasyMock.eq(mModuleInfo));
        mMockTest.setBuild(EasyMock.eq(mMockBuildInfo));
        mMockTest.setDevice(EasyMock.eq(mMockDevice));
        mMockTest.setConfiguration(EasyMock.anyObject());
        EasyMock.expectLastCall().times(2);
        mMockTest.run(EasyMock.eq(mModuleInfo), EasyMock.anyObject());
        EasyMock.expect(mMockPrep.isTearDownDisabled()).andStubReturn(false);
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());
        mMockListener.testRunStarted(
                EasyMock.eq(MODULE_NAME), EasyMock.eq(0), EasyMock.eq(0), EasyMock.anyLong());
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        replayMocks();
        mModule.run(mModuleInfo, mMockListener);
        verifyMocks();
    }

    @Test
    public void testDynamicDownloadThrows_ReportsRunFailed() throws Exception {
        String expectedMessage = "Ooops!";
        ModuleDefinition module =
                new ModuleDefinition(
                        MODULE_NAME,
                        mTestList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", "") {
                            @Override
                            public void resolveDynamicOptions(DynamicRemoteFileResolver resolver) {
                                throw new RuntimeException(expectedMessage);
                            }
                        });
        module.setEnableDynamicDownload(true);
        module.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        Capture<FailureDescription> failureDescription = new Capture<>();
        mMockListener.testRunFailed(EasyMock.capture(failureDescription));
        replayMocks();

        module.run(mModuleInfo, mMockListener);

        assertThat(failureDescription.getValue().getErrorMessage()).contains(expectedMessage);
    }

    /**
     * If an exception is thrown during tear down, report it for the module if there was no other
     * errors.
     */
    @Test
    public void testRun_tearDownException() throws Exception {
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);
        EasyMock.expect(mMockPrep.isDisabled()).andReturn(false).times(2);
        mMockPrep.setUp(EasyMock.eq(mModuleInfo));
        mMockTest.setBuild(EasyMock.eq(mMockBuildInfo));
        mMockTest.setDevice(EasyMock.eq(mMockDevice));
        mMockTest.setConfiguration(EasyMock.anyObject());
        EasyMock.expectLastCall().times(2);
        mMockTest.run(EasyMock.eq(mModuleInfo), EasyMock.anyObject());
        EasyMock.expect(mMockPrep.isTearDownDisabled()).andStubReturn(false);
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());
        // Exception thrown during tear down do not bubble up to invocation.
        RuntimeException exception = new RuntimeException("teardown failed");
        EasyMock.expectLastCall().andThrow(exception);
        mMockListener.testRunStarted(
                EasyMock.eq(MODULE_NAME), EasyMock.eq(0), EasyMock.eq(0), EasyMock.anyLong());
        Capture<FailureDescription> captured = new Capture<>();
        mMockListener.testRunFailed(EasyMock.capture(captured));
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        replayMocks();
        mModule.run(mModuleInfo, mMockListener);
        verifyMocks();
        assertTrue(captured.getValue().getErrorMessage().contains("teardown failed"));
    }

    /**
     * In case of multiple run failures happening, ensure we have some way to get them all
     * eventually.
     */
    @Test
    public void testRun_aggregateRunFailures() throws Exception {
        final int testCount = 4;
        List<IRemoteTest> testList = new ArrayList<>();
        testList.add(new TestObject("run1", testCount, false, true));
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        testList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.disableAutoRetryReportingTime();
        mModule.setRetryDecision(mDecision);
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);
        EasyMock.expect(mMockPrep.isDisabled()).andReturn(false).times(2);
        // no isTearDownDisabled() expected for setup
        mMockPrep.setUp(EasyMock.eq(mModuleInfo));
        EasyMock.expect(mMockPrep.isTearDownDisabled()).andStubReturn(false);
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());
        // Exception thrown during tear down do not bubble up to invocation.
        RuntimeException exception = new RuntimeException("teardown failed");
        EasyMock.expectLastCall().andThrow(exception);
        mMockListener.testRunStarted(
                EasyMock.eq(MODULE_NAME),
                EasyMock.eq(testCount),
                EasyMock.eq(0),
                EasyMock.anyLong());
        for (int i = 0; i < 1; i++) {
            mMockListener.testStarted((TestDescription) EasyMock.anyObject(), EasyMock.anyLong());
            mMockListener.testEnded(
                    (TestDescription) EasyMock.anyObject(),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());
        }
        mMockListener.testFailed(EasyMock.anyObject(), (FailureDescription) EasyMock.anyObject());
        Capture<FailureDescription> captured = new Capture<>();
        mMockListener.testRunFailed(EasyMock.capture(captured));
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        // There was a module failure so a bugreport should be captured.
        EasyMock.expect(mMockDevice.getIDevice()).andStubReturn(EasyMock.createMock(IDevice.class));
        EasyMock.expect(mMockDevice.getSerialNumber()).andReturn("SERIAL");
        EasyMock.expect(
                        mMockDevice.logBugreport(
                                EasyMock.eq("module-fakeName-failure-SERIAL-bugreport"),
                                EasyMock.anyObject()))
                .andReturn(true);

        replayMocks();
        CollectingTestListener errorChecker = new CollectingTestListener();
        // DeviceUnresponsive should not throw since it indicates that the device was recovered.
        mModule.run(mModuleInfo, new ResultForwarder(mMockListener, errorChecker));
        // Only one module
        assertEquals(1, mModule.getTestsResults().size());
        assertEquals(0, mModule.getTestsResults().get(0).getNumCompleteTests());
        verifyMocks();
        // Check that the error aggregates
        List<TestRunResult> res = errorChecker.getTestRunAttempts(MODULE_NAME);
        assertEquals(1, res.size());
        assertTrue(res.get(0).isRunFailure());
        assertTrue(
                res.get(0)
                        .getRunFailureDescription()
                        .getErrorMessage()
                        .contains(
                                "There were 2 failures:\n  unresponsive\n  "
                                        + "java.lang.RuntimeException: teardown failed"));
        assertTrue(captured.getValue() instanceof MultiFailureDescription);
    }

    /** Test that Module definition properly parse tokens out of the configuration description. */
    @Test
    public void testParseTokens() throws Exception {
        Configuration config = new Configuration("", "");
        ConfigurationDescriptor descriptor = config.getConfigurationDescription();
        descriptor.addMetadata(ITestSuite.TOKEN_KEY, Arrays.asList("SIM_CARD"));
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        mTestList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        config);
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);
        replayMocks();
        assertEquals(1, mModule.getRequiredTokens().size());
        assertEquals(TokenProperty.SIM_CARD, mModule.getRequiredTokens().iterator().next());
        verifyMocks();
    }

    /**
     * Test that {@link ModuleDefinition#run(TestInformation, ITestInvocationListener)} is properly
     * going through the execution flow and skip target preparers if disabled.
     */
    @Test
    public void testRun_disabledPreparation() throws Exception {
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);
        // No setup and teardown expected from preparers.
        EasyMock.expect(mMockPrep.isDisabled()).andReturn(true).times(2);
        mMockTest.setBuild(EasyMock.eq(mMockBuildInfo));
        mMockTest.setDevice(EasyMock.eq(mMockDevice));
        mMockTest.setConfiguration(EasyMock.anyObject());
        EasyMock.expectLastCall().times(2);
        mMockTest.run(EasyMock.eq(mModuleInfo), EasyMock.anyObject());
        mMockListener.testRunStarted(
                EasyMock.eq(MODULE_NAME), EasyMock.eq(0), EasyMock.eq(0), EasyMock.anyLong());
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        replayMocks();
        mModule.run(mModuleInfo, mMockListener);
        verifyMocks();
    }

    /**
     * Test that {@link ModuleDefinition#run(TestInformation, ITestInvocationListener)} is properly
     * going through the execution flow and skip target cleanup if teardown is disabled.
     */
    @Test
    public void testRun_disabledTearDown() throws Exception {
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);
        // Setup expected from preparers.
        EasyMock.expect(mMockPrep.isDisabled()).andReturn(false).times(2);
        mMockPrep.setUp(EasyMock.eq(mModuleInfo));
        mMockTest.setBuild(EasyMock.eq(mMockBuildInfo));
        mMockTest.setDevice(EasyMock.eq(mMockDevice));
        mMockTest.setConfiguration(EasyMock.anyObject());
        EasyMock.expectLastCall().times(2);
        mMockTest.run(EasyMock.eq(mModuleInfo), EasyMock.anyObject());
        EasyMock.expect(mMockPrep.isTearDownDisabled()).andStubReturn(true);
        // But no teardown expected from Cleaner.
        mMockListener.testRunStarted(
                EasyMock.eq(MODULE_NAME), EasyMock.eq(0), EasyMock.eq(0), EasyMock.anyLong());
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        replayMocks();
        mModule.run(mModuleInfo, mMockListener);
        verifyMocks();
    }

    /**
     * Test that {@link ModuleDefinition#run(TestInformation, ITestInvocationListener)} properly
     * propagate an early preparation failure.
     */
    @Test
    public void testRun_failPreparation() throws Exception {
        final String exceptionMessage = "ouch I failed";
        mTargetPrepList.clear();
        mTargetPrepList.add(
                new BaseTargetPreparer() {
                    @Override
                    public void setUp(TestInformation testInfo)
                            throws TargetSetupError, BuildError, DeviceNotAvailableException {
                        DeviceDescriptor nullDescriptor = null;
                        throw new TargetSetupError(exceptionMessage, nullDescriptor);
                    }
                });
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        mTestList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());
        mMockListener.testRunStarted(
                EasyMock.eq(MODULE_NAME), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        Capture<FailureDescription> captured = new Capture<>();
        mMockListener.testRunFailed(EasyMock.capture(captured));
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        replayMocks();
        mModule.run(mModuleInfo, mMockListener);
        verifyMocks();

        assertTrue(captured.getValue().getErrorMessage().contains(exceptionMessage));
    }

    /**
     * Test that {@link ModuleDefinition#run(TestInformation, ITestInvocationListener)} properly
     * propagate an early preparation failure, even for a runtime exception.
     */
    @Test
    public void testRun_failPreparation_runtime() throws Exception {
        final String exceptionMessage = "ouch I failed";
        mTargetPrepList.clear();
        mTargetPrepList.add(
                new BaseTargetPreparer() {
                    @Override
                    public void setUp(TestInformation testInfo)
                            throws TargetSetupError, BuildError, DeviceNotAvailableException {
                        throw new RuntimeException(exceptionMessage);
                    }
                });
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        mTestList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());
        mMockListener.testRunStarted(
                EasyMock.eq(MODULE_NAME), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        Capture<FailureDescription> captured = new Capture<>();
        mMockListener.testRunFailed(EasyMock.capture(captured));
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        replayMocks();
        mModule.run(mModuleInfo, mMockListener);
        verifyMocks();

        assertTrue(captured.getValue().getErrorMessage().contains(exceptionMessage));
    }

    @Test
    public void testRun_failPreparation_error() throws Exception {
        final String exceptionMessage = "ouch I failed";
        mTargetPrepList.clear();
        mTargetPrepList.add(
                new BaseTargetPreparer() {
                    @Override
                    public void setUp(TestInformation testInfo)
                            throws TargetSetupError, BuildError, DeviceNotAvailableException {
                        // Throw AssertionError
                        Assert.assertNull(exceptionMessage);
                    }
                });
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        mTestList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());
        mMockListener.testRunStarted(
                EasyMock.eq(MODULE_NAME), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        Capture<FailureDescription> captured = new Capture<>();
        mMockListener.testRunFailed(EasyMock.capture(captured));
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        replayMocks();
        mModule.run(mModuleInfo, mMockListener);
        verifyMocks();

        assertTrue(captured.getValue().getErrorMessage().contains(exceptionMessage));
    }

    /**
     * Test that {@link ModuleDefinition#run(TestInformation, ITestInvocationListener)} properly
     * pass the results of early failures to both main listener and module listeners.
     */
    @Test
    public void testRun_failPreparation_moduleListener() throws Exception {
        ITestInvocationListener mockModuleListener =
                EasyMock.createMock(ITestInvocationListener.class);
        final String exceptionMessage = "ouch I failed";
        mTargetPrepList.clear();
        mTargetPrepList.add(
                new BaseTargetPreparer() {
                    @Override
                    public void setUp(TestInformation testInfo)
                            throws TargetSetupError, BuildError, DeviceNotAvailableException {
                        DeviceDescriptor nullDescriptor = null;
                        throw new TargetSetupError(exceptionMessage, nullDescriptor);
                    }
                });
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        mTestList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());
        mMockListener.testRunStarted(
                EasyMock.eq(MODULE_NAME), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        Capture<FailureDescription> captured1 = new Capture<>();
        mMockListener.testRunFailed(EasyMock.capture(captured1));
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        // Ensure that module listeners receive the callbacks too.
        mockModuleListener.testRunStarted(
                EasyMock.eq(MODULE_NAME), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        Capture<FailureDescription> captured2 = new Capture<>();
        mockModuleListener.testRunFailed(EasyMock.capture(captured2));
        mockModuleListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        EasyMock.replay(mockModuleListener);
        replayMocks();
        mModule.run(mModuleInfo, mMockListener, Arrays.asList(mockModuleListener), null);
        verifyMocks();
        EasyMock.verify(mockModuleListener);

        assertTrue(captured1.getValue().getErrorMessage().contains(exceptionMessage));
        assertTrue(captured2.getValue().getErrorMessage().contains(exceptionMessage));
    }

    /** Test that {@link ModuleDefinition#run(TestInformation, ITestInvocationListener)} */
    @Test
    public void testRun_failPreparation_unresponsive() throws Exception {
        final String exceptionMessage = "ouch I failed";
        mTargetPrepList.clear();
        ITargetPreparer preparer =
                new BaseTargetPreparer() {
                    @Override
                    public void setUp(TestInformation testInfo)
                            throws TargetSetupError, BuildError, DeviceNotAvailableException {
                        throw new DeviceUnresponsiveException(exceptionMessage, "serial");
                    }
                };
        preparer.setDisableTearDown(true);
        mTargetPrepList.add(preparer);
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        mTestList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mMockListener.testRunStarted(
                EasyMock.eq(MODULE_NAME), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        Capture<FailureDescription> captured = new Capture<>();
        mMockListener.testRunFailed(EasyMock.capture(captured));
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        replayMocks();
        try {
            mModule.run(mModuleInfo, mMockListener);
            fail("Should have thrown an exception.");
        } catch (DeviceUnresponsiveException expected) {
            // The exception is still bubbled up.
            assertEquals(exceptionMessage, expected.getMessage());
        }
        verifyMocks();

        assertTrue(captured.getValue().getErrorMessage().contains(exceptionMessage));
    }

    /**
     * Test that {@link ModuleDefinition#run(TestInformation, ITestInvocationListener)} is properly
     * going through the execution flow with actual test callbacks.
     */
    @Test
    public void testRun_fullPass() throws Exception {
        final int testCount = 5;
        List<IRemoteTest> testList = new ArrayList<>();
        testList.add(new TestObject("run1", testCount, false));
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        testList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.setRetryDecision(mDecision);
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);
        EasyMock.expect(mMockPrep.isDisabled()).andReturn(false).times(2);
        // no isTearDownDisabled() expected for setup
        mMockPrep.setUp(EasyMock.eq(mModuleInfo));
        EasyMock.expect(mMockPrep.isTearDownDisabled()).andStubReturn(false);
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());
        mMockListener.testRunStarted(
                EasyMock.eq(MODULE_NAME),
                EasyMock.eq(testCount),
                EasyMock.eq(0),
                EasyMock.anyLong());
        for (int i = 0; i < testCount; i++) {
            mMockListener.testStarted((TestDescription) EasyMock.anyObject(), EasyMock.anyLong());
            mMockListener.testEnded(
                    (TestDescription) EasyMock.anyObject(),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());
        }
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        replayMocks();
        mModule.run(mModuleInfo, mMockListener);
        verifyMocks();
    }

    /**
     * Test that {@link ModuleDefinition#run(TestInformation, ITestInvocationListener)} is properly
     * going through the execution flow with actual test callbacks.
     */
    @Test
    public void testRun_partialRun() throws Exception {
        final int testCount = 4;
        List<IRemoteTest> testList = new ArrayList<>();
        testList.add(new TestObject("run1", testCount, true));
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        testList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.setRetryDecision(mDecision);
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);
        EasyMock.expect(mMockPrep.isDisabled()).andReturn(false).times(2);
        // no isTearDownDisabled() expected for setup
        mMockPrep.setUp(EasyMock.eq(mModuleInfo));
        EasyMock.expect(mMockPrep.isTearDownDisabled()).andStubReturn(false);
        mMockPrep.tearDown(
                EasyMock.eq(mModuleInfo), EasyMock.isA(DeviceNotAvailableException.class));
        mMockListener.testRunStarted(
                EasyMock.eq(MODULE_NAME),
                EasyMock.eq(testCount),
                EasyMock.eq(0),
                EasyMock.anyLong());
        for (int i = 0; i < 3; i++) {
            mMockListener.testStarted((TestDescription) EasyMock.anyObject(), EasyMock.anyLong());
            mMockListener.testEnded(
                    (TestDescription) EasyMock.anyObject(),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());
        }
        mMockListener.testFailed(EasyMock.anyObject(), (FailureDescription) EasyMock.anyObject());
        mMockListener.testRunFailed((FailureDescription) EasyMock.anyObject());
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        // Recovery is disabled during tearDown
        EasyMock.expect(mMockDevice.getRecoveryMode()).andReturn(RecoveryMode.AVAILABLE);
        mMockDevice.setRecoveryMode(RecoveryMode.NONE);
        mMockDevice.setRecoveryMode(RecoveryMode.AVAILABLE);

        EasyMock.expect(mMockDevice.getSerialNumber()).andStubReturn("serial");
        replayMocks();
        try {
            mModule.run(mModuleInfo, mMockListener);
            fail("Should have thrown an exception.");
        } catch (DeviceNotAvailableException expected) {
            // expected
        }
        // Only one module
        assertEquals(1, mModule.getTestsResults().size());
        assertEquals(2, mModule.getTestsResults().get(0).getNumCompleteTests());
        verifyMocks();
    }

    @Test
    public void testRun_partialRun_error() throws Exception {
        final int testCount = 4;
        List<IRemoteTest> testList = new ArrayList<>();
        testList.add(new TestObject("run1", testCount, false, false, true));
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        testList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.setRetryDecision(mDecision);
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);
        EasyMock.expect(mMockPrep.isDisabled()).andReturn(false).times(2);
        // no isTearDownDisabled() expected for setup
        mMockPrep.setUp(EasyMock.eq(mModuleInfo));
        EasyMock.expect(mMockPrep.isTearDownDisabled()).andStubReturn(false);
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());
        mMockListener.testRunStarted(
                EasyMock.eq(MODULE_NAME),
                EasyMock.eq(testCount),
                EasyMock.eq(0),
                EasyMock.anyLong());
        for (int i = 0; i < 3; i++) {
            mMockListener.testStarted((TestDescription) EasyMock.anyObject(), EasyMock.anyLong());
            mMockListener.testEnded(
                    (TestDescription) EasyMock.anyObject(),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());
        }
        mMockListener.testFailed(EasyMock.anyObject(), (FailureDescription) EasyMock.anyObject());
        mMockListener.testRunFailed((FailureDescription) EasyMock.anyObject());
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        replayMocks();
        mModule.run(mModuleInfo, mMockListener);
        // Only one module
        assertEquals(1, mModule.getTestsResults().size());
        assertEquals(2, mModule.getTestsResults().get(0).getNumCompleteTests());
        assertTrue(
                mModule.getTestsResults().get(0).getRunFailureMessage().contains("assert error"));
        verifyMocks();
    }

    /**
     * Test that when a module is created with some particular informations, the resulting {@link
     * IInvocationContext} of the module is properly populated.
     */
    @Test
    public void testAbiSetting() throws Exception {
        final int testCount = 5;
        IConfiguration config = new Configuration("", "");
        ConfigurationDescriptor descriptor = new ConfigurationDescriptor();
        descriptor.setAbi(new Abi("arm", "32"));
        descriptor.setModuleName(MODULE_NAME);
        config.setConfigurationObject(
                Configuration.CONFIGURATION_DESCRIPTION_TYPE_NAME, descriptor);
        List<IRemoteTest> testList = new ArrayList<>();
        testList.add(new TestObject("run1", testCount, false));
        mModule =
                new ModuleDefinition(
                        "arm32 " + MODULE_NAME,
                        testList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        config);
        // Check that the invocation module created has expected informations
        IInvocationContext moduleContext = mModule.getModuleInvocationContext();
        assertEquals(
                MODULE_NAME,
                moduleContext.getAttributes().get(ModuleDefinition.MODULE_NAME).get(0));
        assertEquals("arm", moduleContext.getAttributes().get(ModuleDefinition.MODULE_ABI).get(0));
        assertEquals(
                "arm32 " + MODULE_NAME,
                moduleContext.getAttributes().get(ModuleDefinition.MODULE_ID).get(0));
    }

    /**
     * Test running a module when the configuration has a module controller object that force a full
     * bypass of the module.
     */
    @Test
    public void testModuleController_fullBypass() throws Exception {
        IConfiguration config = new Configuration("", "");
        BaseModuleController moduleConfig =
                new BaseModuleController() {
                    @Override
                    public RunStrategy shouldRun(IInvocationContext context) {
                        return RunStrategy.FULL_MODULE_BYPASS;
                    }
                };
        config.setConfigurationObject(ModuleDefinition.MODULE_CONTROLLER, moduleConfig);
        List<IRemoteTest> testList = new ArrayList<>();
        testList.add(
                new IRemoteTest() {
                    @Override
                    public void run(TestInformation testInfo, ITestInvocationListener listener)
                            throws DeviceNotAvailableException {
                        listener.testRunStarted("test", 1);
                        listener.testFailed(
                                new TestDescription("failedclass", "failedmethod"),
                                FailureDescription.create("trace", FailureStatus.TEST_FAILURE));
                    }
                });
        mTargetPrepList.clear();
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        testList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        config);
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        // module is completely skipped, no tests is recorded.
        replayMocks();
        mModule.run(mModuleInfo, mMockListener, null, null);
        verifyMocks();
    }

    /**
     * Test running a module when the configuration has a module controller object that force to
     * skip all the module test cases.
     */
    @Test
    public void testModuleController_skipTestCases() throws Exception {
        IConfiguration config = new Configuration("", "");
        BaseModuleController moduleConfig =
                new BaseModuleController() {
                    @Override
                    public RunStrategy shouldRun(IInvocationContext context) {
                        return RunStrategy.SKIP_MODULE_TESTCASES;
                    }
                };
        config.setConfigurationObject(ModuleDefinition.MODULE_CONTROLLER, moduleConfig);
        List<IRemoteTest> testList = new ArrayList<>();
        testList.add(
                new IRemoteTest() {
                    @Override
                    public void run(TestInformation testInfo, ITestInvocationListener listener)
                            throws DeviceNotAvailableException {
                        TestDescription tid = new TestDescription("class", "method");
                        listener.testRunStarted("test", 1);
                        listener.testStarted(tid);
                        listener.testFailed(
                                tid,
                                FailureDescription.create("I failed", FailureStatus.TEST_FAILURE));
                        listener.testEnded(tid, new HashMap<String, Metric>());
                        listener.testRunEnded(0, new HashMap<String, Metric>());
                    }
                });
        mTargetPrepList.clear();
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        testList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        config);
        mModule.setRetryDecision(mDecision);
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        // expect the module to run but tests to be ignored
        mMockListener.testRunStarted(
                EasyMock.anyObject(), EasyMock.anyInt(), EasyMock.eq(0), EasyMock.anyLong());
        mMockListener.testStarted(EasyMock.anyObject(), EasyMock.anyLong());
        mMockListener.testIgnored(EasyMock.anyObject());
        mMockListener.testEnded(
                EasyMock.anyObject(),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        replayMocks();
        mModule.run(mModuleInfo, mMockListener, null, null);
        verifyMocks();
    }

    /** Test {@link IRemoteTest} that log a file during its run. */
    public class TestLogClass implements ITestInterface {

        @Override
        public void run(TestInformation testInfo, ITestInvocationListener listener)
                throws DeviceNotAvailableException {
            listener.testLog(
                    "testlogclass",
                    LogDataType.TEXT,
                    new ByteArrayInputStreamSource("".getBytes()));
        }

        @Override
        public void setBuild(IBuildInfo buildInfo) {}

        @Override
        public void setDevice(ITestDevice device) {}

        @Override
        public ITestDevice getDevice() {
            return null;
        }

        @Override
        public void setConfiguration(IConfiguration configuration) {}
    }

    /**
     * Test that the invocation level result_reporter receive the testLogSaved information from the
     * modules.
     *
     * <p>The {@link LogSaverResultForwarder} from the module is expected to log the file and ensure
     * that it passes the information to the {@link LogSaverResultForwarder} from the {@link
     * TestInvocation} in order for final result_reporter to know about logged files.
     */
    @Test
    public void testModule_LogSaverResultForwarder() throws Exception {
        List<IRemoteTest> testList = new ArrayList<>();
        testList.add(new TestLogClass());
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        testList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.setRetryDecision(mDecision);
        mModule.setLogSaver(mMockLogSaver);
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);
        EasyMock.expect(mMockPrep.isDisabled()).andReturn(false).times(2);
        // no isTearDownDisabled() expected for setup
        mMockPrep.setUp(EasyMock.eq(mModuleInfo));
        EasyMock.expect(mMockPrep.isTearDownDisabled()).andStubReturn(false);
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());
        mMockLogSaverListener.setLogSaver(mMockLogSaver);
        // The final reporter still receive the testLog signal
        mMockLogSaverListener.testLog(
                EasyMock.eq("testlogclass"), EasyMock.eq(LogDataType.TEXT), EasyMock.anyObject());

        LogFile loggedFile = new LogFile("path", "url", LogDataType.TEXT);
        EasyMock.expect(
                        mMockLogSaver.saveLogData(
                                EasyMock.eq("testlogclass"),
                                EasyMock.eq(LogDataType.TEXT),
                                EasyMock.anyObject()))
                .andReturn(loggedFile);
        // mMockLogSaverListener should receive the testLogSaved call even from the module
        mMockLogSaverListener.testLogSaved(
                EasyMock.eq("testlogclass"),
                EasyMock.eq(LogDataType.TEXT),
                EasyMock.anyObject(),
                EasyMock.eq(loggedFile));
        mMockLogSaverListener.logAssociation("testlogclass", loggedFile);

        mMockLogSaverListener.testRunStarted(
                EasyMock.eq(MODULE_NAME), EasyMock.eq(0), EasyMock.eq(0), EasyMock.anyLong());
        mMockLogSaverListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        // Simulate how the invoker actually put the log saver
        replayMocks();
        LogSaverResultForwarder forwarder =
                new LogSaverResultForwarder(mMockLogSaver, Arrays.asList(mMockLogSaverListener));
        mModule.run(mModuleInfo, forwarder);
        verifyMocks();
    }

    /**
     * Test that the {@link IModuleController} object can override the behavior of the capture of
     * the failure.
     */
    @Test
    public void testOverrideModuleConfig() throws Exception {
        // failure listener with capture logcat on failure and screenshot on failure.
        List<ITestDevice> listDevice = new ArrayList<>();
        listDevice.add(mMockDevice);
        EasyMock.expect(mMockDevice.getSerialNumber()).andReturn("Serial");
        TestFailureListener failureListener = new TestFailureListener(listDevice, true, false);
        failureListener.setLogger(mMockListener);
        IConfiguration config = new Configuration("", "");
        TestFailureModuleController moduleConfig = new TestFailureModuleController();
        OptionSetter setter = new OptionSetter(moduleConfig);
        // Module option should override the logcat on failure
        setter.setOptionValue("bugreportz-on-failure", "false");
        config.setConfigurationObject(ModuleDefinition.MODULE_CONTROLLER, moduleConfig);
        List<IRemoteTest> testList = new ArrayList<>();
        testList.add(
                new IRemoteTest() {
                    @Override
                    public void run(TestInformation testInfo, ITestInvocationListener listener)
                            throws DeviceNotAvailableException {
                        listener.testFailed(
                                new TestDescription("failedclass", "failedmethod"),
                                FailureDescription.create("trace", FailureStatus.TEST_FAILURE));
                    }
                });
        mTargetPrepList.clear();
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        testList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        config);
        mModule.setRetryDecision(mDecision);
        mModule.setLogSaver(mMockLogSaver);
        mMockListener.testRunStarted(
                EasyMock.eq("fakeName"), EasyMock.eq(0), EasyMock.eq(0), EasyMock.anyLong());
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        replayMocks();
        mModule.run(mModuleInfo, mMockListener, null, failureListener);
        verifyMocks();
    }

    /** Test when the test yields a DeviceUnresponsive exception. */
    @Test
    public void testRun_partialRun_deviceUnresponsive() throws Exception {
        final int testCount = 4;
        List<IRemoteTest> testList = new ArrayList<>();
        testList.add(new TestObject("run1", testCount, false, true));
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        testList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.setRetryDecision(mDecision);
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);
        EasyMock.expect(mMockPrep.isDisabled()).andReturn(false).times(2);
        // no isTearDownDisabled() expected for setup
        mMockPrep.setUp(EasyMock.eq(mModuleInfo));
        EasyMock.expect(mMockPrep.isTearDownDisabled()).andStubReturn(false);
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());
        mMockListener.testRunStarted(
                EasyMock.eq(MODULE_NAME),
                EasyMock.eq(testCount),
                EasyMock.eq(0),
                EasyMock.anyLong());
        for (int i = 0; i < 1; i++) {
            mMockListener.testStarted((TestDescription) EasyMock.anyObject(), EasyMock.anyLong());
            mMockListener.testEnded(
                    (TestDescription) EasyMock.anyObject(),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());
        }
        mMockListener.testFailed(EasyMock.anyObject(), (FailureDescription) EasyMock.anyObject());
        FailureDescription issues =
                FailureDescription.create("unresponsive", FailureStatus.LOST_SYSTEM_UNDER_TEST);
        mMockListener.testRunFailed(issues);
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        // There was a module failure so a bugreport should be captured.
        EasyMock.expect(mMockDevice.getIDevice()).andStubReturn(EasyMock.createMock(IDevice.class));
        EasyMock.expect(mMockDevice.getSerialNumber()).andReturn("SERIAL");
        EasyMock.expect(
                        mMockDevice.logBugreport(
                                EasyMock.eq("module-fakeName-failure-SERIAL-bugreport"),
                                EasyMock.anyObject()))
                .andReturn(true);

        replayMocks();
        // DeviceUnresponsive should not throw since it indicates that the device was recovered.
        mModule.run(mModuleInfo, mMockListener);
        // Only one module
        assertEquals(1, mModule.getTestsResults().size());
        assertEquals(0, mModule.getTestsResults().get(0).getNumCompleteTests());
        verifyMocks();
    }

    /**
     * Test that when a module level listener is specified it receives the events before the
     * buffering and replay.
     */
    @Test
    public void testRun_moduleLevelListeners() throws Exception {
        mMockListener = EasyMock.createStrictMock(ITestInvocationListener.class);
        final int testCount = 5;
        List<IRemoteTest> testList = new ArrayList<>();
        testList.add(new TestObject("run1", testCount, false));
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        testList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.setRetryDecision(mDecision);
        mModule.setLogSaver(mMockLogSaver);
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);
        EasyMock.expect(mMockPrep.isDisabled()).andReturn(false).times(2);
        // no isTearDownDisabled() expected for setup
        mMockPrep.setUp(EasyMock.eq(mModuleInfo));
        EasyMock.expect(mMockPrep.isTearDownDisabled()).andStubReturn(false);
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());

        mMockLogSaverListener.setLogSaver(mMockLogSaver);

        mMockListener.testRunStarted("run1", testCount, 0);
        for (int i = 0; i < testCount; i++) {
            mMockListener.testStarted((TestDescription) EasyMock.anyObject(), EasyMock.anyLong());
            mMockListener.testEnded(
                    (TestDescription) EasyMock.anyObject(),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());
        }
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        mMockLogSaverListener.testRunStarted(
                EasyMock.eq(MODULE_NAME),
                EasyMock.eq(testCount),
                EasyMock.eq(0),
                EasyMock.anyLong());
        for (int i = 0; i < testCount; i++) {
            mMockLogSaverListener.testStarted(
                    (TestDescription) EasyMock.anyObject(), EasyMock.anyLong());
            mMockLogSaverListener.testEnded(
                    (TestDescription) EasyMock.anyObject(),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());
        }
        mMockLogSaverListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        // Simulate how the invoker actually put the log saver
        replayMocks();
        LogSaverResultForwarder forwarder =
                new LogSaverResultForwarder(mMockLogSaver, Arrays.asList(mMockLogSaverListener));
        mModule.run(mModuleInfo, forwarder, Arrays.asList(mMockListener), null);
        verifyMocks();
    }

    /**
     * Test that {@link ModuleDefinition#run(TestInformation, ITestInvocationListener)} is properly
     * going through the execution flow and reports properly when the runner generates multiple
     * runs.
     */
    @Test
    public void testMultiRun() throws Exception {
        final String runName = "baseRun";
        List<IRemoteTest> testList = new ArrayList<>();
        // The runner will generates 2 test runs with 2 test cases each.
        testList.add(new MultiRunTestObject(runName, 2, 2, 0));
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        testList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.setRetryDecision(mDecision);
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);
        EasyMock.expect(mMockPrep.isDisabled()).andReturn(false).times(2);
        mMockPrep.setUp(EasyMock.eq(mModuleInfo));
        EasyMock.expect(mMockPrep.isTearDownDisabled()).andStubReturn(false);
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());
        // We expect a total count on the run start so 4, all aggregated under the same run
        mMockListener.testRunStarted(
                EasyMock.eq(MODULE_NAME), EasyMock.eq(4), EasyMock.eq(0), EasyMock.anyLong());
        // The first set of test cases from the first test run.
        for (int i = 0; i < 2; i++) {
            TestDescription testId = new TestDescription(runName + "0class", "test" + i);
            mMockListener.testStarted(EasyMock.eq(testId), EasyMock.anyLong());
            mMockListener.testEnded(
                    EasyMock.eq(testId),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());
        }
        // The second set of test cases from the second test run
        for (int i = 0; i < 2; i++) {
            TestDescription testId = new TestDescription(runName + "1class", "test" + i);
            mMockListener.testStarted(EasyMock.eq(testId), EasyMock.anyLong());
            mMockListener.testEnded(
                    EasyMock.eq(testId),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());
        }
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        replayMocks();
        mModule.run(mModuleInfo, mMockListener);
        verifyMocks();
    }

    @Test
    public void testRun_earlyFailure() throws Exception {
        List<IRemoteTest> testList = new ArrayList<>();
        testList.add(new DirectFailureTestObject());
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        testList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.setRetryDecision(mDecision);
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);

        EasyMock.expect(mMockPrep.isDisabled()).andReturn(false).times(2);
        // no isTearDownDisabled() expected for setup
        mMockPrep.setUp(EasyMock.eq(mModuleInfo));
        EasyMock.expect(mMockPrep.isTearDownDisabled()).andStubReturn(false);
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());

        mMockListener.testRunStarted(
                EasyMock.eq("fakeName"), EasyMock.eq(0), EasyMock.eq(0), EasyMock.anyLong());
        Capture<FailureDescription> captured = new Capture<>();
        mMockListener.testRunFailed(EasyMock.capture(captured));
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        replayMocks();
        mModule.run(mModuleInfo, mMockListener);
        verifyMocks();
        assertTrue(captured.getValue().getErrorMessage().contains("early failure!"));
    }

    /** Test retry and reporting all the different attempts. */
    @Test
    public void testMultiRun_multiAttempts() throws Exception {
        final String runName = "baseRun";
        List<IRemoteTest> testList = new ArrayList<>();
        // The runner will generates 2 test runs with 3 test cases each.
        testList.add(new MultiRunTestObject(runName, 3, 2, 1));
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        testList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.disableAutoRetryReportingTime();
        IRetryDecision decision = new BaseRetryDecision();
        OptionSetter setter = new OptionSetter(decision);
        setter.setOptionValue("retry-strategy", "ITERATIONS");
        setter.setOptionValue("max-testcase-run-count", Integer.toString(3));
        decision.setInvocationContext(mModule.getModuleInvocationContext());
        mModule.setRetryDecision(decision);
        mModule.setMergeAttemps(false);
        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);
        EasyMock.expect(mMockPrep.isDisabled()).andReturn(false).times(2);
        mMockPrep.setUp(EasyMock.eq(mModuleInfo));
        EasyMock.expect(mMockPrep.isTearDownDisabled()).andStubReturn(false);
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());
        // We expect a total count on the run start so 4, all aggregated under the same run
        for (int attempt = 0; attempt < 3; attempt++) {
            mMockListener.testRunStarted(
                    EasyMock.eq(MODULE_NAME),
                    EasyMock.eq(6),
                    EasyMock.eq(attempt),
                    EasyMock.anyLong());
            // The first set of test cases from the first test run.
            TestDescription testId0 = new TestDescription(runName + "0class", "test0");
            mMockListener.testStarted(EasyMock.eq(testId0), EasyMock.anyLong());
            mMockListener.testEnded(
                    EasyMock.eq(testId0),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());
            TestDescription testFail0 = new TestDescription(runName + "0class", "fail0");
            mMockListener.testStarted(EasyMock.eq(testFail0), EasyMock.anyLong());
            mMockListener.testFailed(
                    EasyMock.eq(testFail0), (FailureDescription) EasyMock.anyObject());
            mMockListener.testEnded(
                    EasyMock.eq(testFail0),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());
            TestDescription testId1 = new TestDescription(runName + "0class", "test1");
            mMockListener.testStarted(EasyMock.eq(testId1), EasyMock.anyLong());
            mMockListener.testEnded(
                    EasyMock.eq(testId1),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());

            // The second set of test cases from the second test run
            TestDescription testId0_1 = new TestDescription(runName + "1class", "test0");
            mMockListener.testStarted(EasyMock.eq(testId0_1), EasyMock.anyLong());
            mMockListener.testEnded(
                    EasyMock.eq(testId0_1),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());
            TestDescription testFail0_1 = new TestDescription(runName + "1class", "fail0");
            mMockListener.testStarted(EasyMock.eq(testFail0_1), EasyMock.anyLong());
            mMockListener.testFailed(
                    EasyMock.eq(testFail0_1), (FailureDescription) EasyMock.anyObject());
            mMockListener.testEnded(
                    EasyMock.eq(testFail0_1),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());
            TestDescription testId1_1 = new TestDescription(runName + "1class", "test1");
            mMockListener.testStarted(EasyMock.eq(testId1_1), EasyMock.anyLong());
            mMockListener.testEnded(
                    EasyMock.eq(testId1_1),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());

            mMockListener.testRunEnded(
                    EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        }
        replayMocks();
        mModule.run(mModuleInfo, mMockListener, null, null, 3);
        verifyMocks();
    }

    /** Test retry and reporting all the different attempts when retrying failures. */
    @Test
    public void testMultiRun_multiAttempts_filter() throws Exception {
        final String runName = "baseRun";
        List<IRemoteTest> testList = new ArrayList<>();
        // The runner will generates 2 test runs with 3 test cases each. (2 passes and 1 fail)
        testList.add(new MultiRunTestObject(runName, 3, 2, 1));
        mModule =
                new ModuleDefinition(
                        MODULE_NAME,
                        testList,
                        mMapDeviceTargetPreparer,
                        mMultiTargetPrepList,
                        new Configuration("", ""));
        mModule.disableAutoRetryReportingTime();
        IRetryDecision decision = new BaseRetryDecision();
        OptionSetter setter = new OptionSetter(decision);
        setter.setOptionValue("retry-strategy", "RETRY_ANY_FAILURE");
        setter.setOptionValue("max-testcase-run-count", Integer.toString(3));
        decision.setInvocationContext(mModule.getModuleInvocationContext());
        mModule.setRetryDecision(decision);
        mModule.setMergeAttemps(false);

        mModule.getModuleInvocationContext().addAllocatedDevice(DEFAULT_DEVICE_NAME, mMockDevice);
        mModule.getModuleInvocationContext()
                .addDeviceBuildInfo(DEFAULT_DEVICE_NAME, mMockBuildInfo);
        mModuleInfo =
                TestInformation.newBuilder()
                        .setInvocationContext(mModule.getModuleInvocationContext())
                        .build();
        mModule.setBuild(mMockBuildInfo);
        mModule.setDevice(mMockDevice);
        EasyMock.expect(mMockPrep.isDisabled()).andReturn(false).times(2);
        mMockPrep.setUp(EasyMock.eq(mModuleInfo));
        EasyMock.expect(mMockPrep.isTearDownDisabled()).andStubReturn(false);
        mMockPrep.tearDown(EasyMock.eq(mModuleInfo), EasyMock.isNull());
        EasyMock.expect(mMockDevice.getIDevice())
                .andReturn(EasyMock.createMock(IDevice.class))
                .times(2);
        // We expect a total count on the run start so 4, all aggregated under the same run
        for (int attempt = 0; attempt < 3; attempt++) {
            if (attempt == 0) {
                mMockListener.testRunStarted(
                        EasyMock.eq(MODULE_NAME),
                        EasyMock.eq(6),
                        EasyMock.eq(attempt),
                        EasyMock.anyLong());
            } else {
                mMockListener.testRunStarted(
                        EasyMock.eq(MODULE_NAME),
                        EasyMock.eq(2),
                        EasyMock.eq(attempt),
                        EasyMock.anyLong());
            }
            // The first set of test cases from the first test run.
            if (attempt < 1) {
                TestDescription testId0 = new TestDescription(runName + "0class", "test0");
                mMockListener.testStarted(EasyMock.eq(testId0), EasyMock.anyLong());
                mMockListener.testEnded(
                        EasyMock.eq(testId0),
                        EasyMock.anyLong(),
                        EasyMock.<HashMap<String, Metric>>anyObject());
            }
            TestDescription testFail0 = new TestDescription(runName + "0class", "fail0");
            mMockListener.testStarted(EasyMock.eq(testFail0), EasyMock.anyLong());
            mMockListener.testFailed(
                    EasyMock.eq(testFail0), (FailureDescription) EasyMock.anyObject());
            mMockListener.testEnded(
                    EasyMock.eq(testFail0),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());
            if (attempt < 1) {
                TestDescription testId1 = new TestDescription(runName + "0class", "test1");
                mMockListener.testStarted(EasyMock.eq(testId1), EasyMock.anyLong());
                mMockListener.testEnded(
                        EasyMock.eq(testId1),
                        EasyMock.anyLong(),
                        EasyMock.<HashMap<String, Metric>>anyObject());
            }

            // The second set of test cases from the second test run
            if (attempt < 1) {
                TestDescription testId0_1 = new TestDescription(runName + "1class", "test0");
                mMockListener.testStarted(EasyMock.eq(testId0_1), EasyMock.anyLong());
                mMockListener.testEnded(
                        EasyMock.eq(testId0_1),
                        EasyMock.anyLong(),
                        EasyMock.<HashMap<String, Metric>>anyObject());
            }
            TestDescription testFail0_1 = new TestDescription(runName + "1class", "fail0");
            mMockListener.testStarted(EasyMock.eq(testFail0_1), EasyMock.anyLong());
            mMockListener.testFailed(
                    EasyMock.eq(testFail0_1), (FailureDescription) EasyMock.anyObject());
            mMockListener.testEnded(
                    EasyMock.eq(testFail0_1),
                    EasyMock.anyLong(),
                    EasyMock.<HashMap<String, Metric>>anyObject());
            if (attempt < 1) {
                TestDescription testId1_1 = new TestDescription(runName + "1class", "test1");
                mMockListener.testStarted(EasyMock.eq(testId1_1), EasyMock.anyLong());
                mMockListener.testEnded(
                        EasyMock.eq(testId1_1),
                        EasyMock.anyLong(),
                        EasyMock.<HashMap<String, Metric>>anyObject());
            }
            mMockListener.testRunEnded(
                    EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        }
        replayMocks();
        mModule.run(mModuleInfo, mMockListener, null, null, 3);
        verifyMocks();
    }
}
