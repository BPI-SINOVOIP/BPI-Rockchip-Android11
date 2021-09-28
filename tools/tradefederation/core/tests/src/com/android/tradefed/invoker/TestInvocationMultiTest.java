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
package com.android.tradefed.invoker;

import static org.junit.Assert.assertEquals;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IBuildProvider;
import com.android.tradefed.command.CommandOptions;
import com.android.tradefed.command.CommandRunner.ExitCode;
import com.android.tradefed.config.ConfigurationDescriptor;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.DeviceConfigurationHolder;
import com.android.tradefed.config.DynamicRemoteFileResolver;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.guice.InvocationScope;
import com.android.tradefed.invoker.shard.IShardHelper;
import com.android.tradefed.invoker.shard.ShardHelper;
import com.android.tradefed.log.ILeveledLogOutput;
import com.android.tradefed.log.ILogRegistry;
import com.android.tradefed.postprocessor.IPostProcessor;
import com.android.tradefed.result.ActionInProgress;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ILogSaver;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.error.InfraErrorIdentifier;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.retry.BaseRetryDecision;

import org.easymock.Capture;
import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TestInvocation} for multi device invocation. */
@RunWith(JUnit4.class)
public class TestInvocationMultiTest {
    private TestInvocation mInvocation;
    private IInvocationContext mContext;
    private IConfiguration mMockConfig;
    private IRescheduler mMockRescheduler;
    private ITestInvocationListener mMockTestListener;
    private ILogSaver mMockLogSaver;
    private ILeveledLogOutput mMockLogger;
    private ILogRegistry mMockLogRegistry;
    private ConfigurationDescriptor mConfigDesc;

    private ITestDevice mDevice1;
    private ITestDevice mDevice2;
    private IBuildProvider mProvider1;
    private IBuildProvider mProvider2;

    private List<IPostProcessor> mPostProcessors;

    @Before
    public void setUp() {
        mContext = new InvocationContext();
        mPostProcessors = new ArrayList<>();

        mMockConfig = EasyMock.createMock(IConfiguration.class);
        EasyMock.expect(mMockConfig.getPostProcessors()).andReturn(mPostProcessors);
        EasyMock.expect(mMockConfig.getRetryDecision()).andReturn(new BaseRetryDecision());
        EasyMock.expect(mMockConfig.getConfigurationObject(ShardHelper.SHARED_TEST_INFORMATION))
                .andReturn(null);
        EasyMock.expect(mMockConfig.getConfigurationObject(ShardHelper.LAST_SHARD_DETECTOR))
                .andReturn(null);
        EasyMock.expect(mMockConfig.getConfigurationObject("DELEGATE")).andStubReturn(null);
        mMockRescheduler = EasyMock.createMock(IRescheduler.class);
        mMockTestListener = EasyMock.createMock(ITestInvocationListener.class);
        mMockLogSaver = EasyMock.createMock(ILogSaver.class);
        mMockLogger = EasyMock.createMock(ILeveledLogOutput.class);
        mMockLogRegistry = EasyMock.createMock(ILogRegistry.class);
        mConfigDesc = new ConfigurationDescriptor();
        mInvocation =
                new TestInvocation() {
                    @Override
                    ILogRegistry getLogRegistry() {
                        return mMockLogRegistry;
                    }

                    @Override
                    public IInvocationExecution createInvocationExec(RunMode mode) {
                        return new InvocationExecution() {
                            @Override
                            protected IShardHelper createShardHelper() {
                                return new ShardHelper();
                            }
                        };
                    }

                    @Override
                    protected void applyAutomatedReporters(IConfiguration config) {
                        // Empty on purpose
                    }

                    @Override
                    protected void setExitCode(ExitCode code, Throwable stack) {
                        // empty on purpose
                    }

                    @Override
                    InvocationScope getInvocationScope() {
                        // Avoid re-entry in the current TF invocation scope for unit tests.
                        return new InvocationScope();
                    }
                };
    }

    private void makeTwoDeviceContext() throws Exception {
        mDevice1 = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mDevice1.getIDevice()).andStubReturn(new StubDevice("serial1"));
        EasyMock.expect(mDevice1.getSerialNumber()).andStubReturn("serial1");
        mDevice1.clearLastConnectedWifiNetwork();
        DeviceConfigurationHolder holder1 = new DeviceConfigurationHolder();
        mProvider1 = EasyMock.createMock(IBuildProvider.class);
        holder1.addSpecificConfig(mProvider1);
        EasyMock.expect(mMockConfig.getDeviceConfigByName("device1")).andStubReturn(holder1);
        mDevice1.setOptions(EasyMock.anyObject());
        mDevice1.setRecovery(EasyMock.anyObject());

        mDevice2 = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mDevice2.getIDevice()).andStubReturn(new StubDevice("serial2"));
        EasyMock.expect(mDevice2.getSerialNumber()).andStubReturn("serial2");
        mDevice2.clearLastConnectedWifiNetwork();
        DeviceConfigurationHolder holder2 = new DeviceConfigurationHolder();
        mProvider2 = EasyMock.createMock(IBuildProvider.class);
        holder2.addSpecificConfig(mProvider2);
        EasyMock.expect(mMockConfig.getDeviceConfigByName("device2")).andStubReturn(holder2);
        mDevice2.setOptions(EasyMock.anyObject());

        mContext.addAllocatedDevice("device1", mDevice1);
        mContext.addAllocatedDevice("device2", mDevice2);
    }

    /**
     * Test for multi device invocation when the first download succeed and second one is missing.
     * We clean up all the downloaded builds.
     */
    @Test
    public void testRunBuildProvider_oneMiss() throws Throwable {
        makeTwoDeviceContext();

        List<ITestInvocationListener> configListener = new ArrayList<>();
        configListener.add(mMockTestListener);
        EasyMock.expect(mMockConfig.getTestInvocationListeners())
                .andReturn(configListener)
                .times(2);
        EasyMock.expect(mMockConfig.getLogSaver()).andReturn(mMockLogSaver);
        EasyMock.expect(mMockConfig.getLogOutput()).andStubReturn(mMockLogger);
        EasyMock.expect(mMockConfig.getConfigurationDescription()).andReturn(mConfigDesc);
        mMockLogger.init();
        EasyMock.expect(mMockLogger.getLog())
                .andReturn(new ByteArrayInputStreamSource("fake".getBytes()));
        mMockLogger.closeLog();
        EasyMock.expectLastCall().times(2);

        mMockLogRegistry.registerLogger(mMockLogger);
        mMockLogRegistry.dumpToGlobalLog(mMockLogger);
        mMockLogRegistry.unregisterLogger();
        EasyMock.expectLastCall().times(2);

        EasyMock.expect(mMockConfig.getCommandLine()).andStubReturn("empty");
        EasyMock.expect(mMockConfig.getCommandOptions()).andStubReturn(new CommandOptions());
        EasyMock.expect(mMockConfig.getTests()).andStubReturn(new ArrayList<>());
        mMockConfig.resolveDynamicOptions(EasyMock.anyObject());
        mMockConfig.cleanConfigurationData();
        IBuildInfo build1 = new BuildInfo();
        EasyMock.expect(mProvider1.getBuild()).andReturn(build1);
        // Second build is not found
        EasyMock.expect(mProvider2.getBuild()).andReturn(null);
        // The downloaded build is cleaned
        mProvider1.cleanUp(build1);

        Capture<IBuildInfo> captured = new Capture<>();
        mProvider2.cleanUp(EasyMock.capture(captured));

        mMockTestListener.invocationStarted(mContext);
        EasyMock.expect(mMockTestListener.getSummary()).andReturn(null);
        mMockLogSaver.invocationStarted(mContext);
        mMockTestListener.invocationFailed(EasyMock.<FailureDescription>anyObject());
        mMockTestListener.testLog(EasyMock.anyObject(), EasyMock.anyObject(), EasyMock.anyObject());
        EasyMock.expect(
                        mMockLogSaver.saveLogData(
                                EasyMock.anyObject(), EasyMock.anyObject(), EasyMock.anyObject()))
                .andReturn(new LogFile("", "", LogDataType.TEXT));
        EasyMock.expect(
                        mMockLogSaver.saveLogData(
                                EasyMock.eq(TestInvocation.TRADEFED_END_HOST_LOG),
                                EasyMock.anyObject(),
                                EasyMock.anyObject()))
                .andReturn(new LogFile("", "", LogDataType.TEXT));
        mMockTestListener.invocationEnded(EasyMock.anyLong());
        EasyMock.expect(mMockTestListener.getSummary()).andReturn(null);
        mMockLogSaver.invocationEnded(EasyMock.anyLong());

        EasyMock.replay(
                mMockConfig,
                mMockRescheduler,
                mMockTestListener,
                mMockLogSaver,
                mMockLogger,
                mMockLogRegistry,
                mDevice1,
                mDevice2,
                mProvider1,
                mProvider2);
        mInvocation.invoke(
                mContext, mMockConfig, mMockRescheduler, new ITestInvocationListener[] {});
        EasyMock.verify(
                mMockConfig,
                mMockRescheduler,
                mMockTestListener,
                mMockLogSaver,
                mMockLogger,
                mMockLogRegistry,
                mDevice1,
                mDevice2,
                mProvider1,
                mProvider2);

        IBuildInfo stubBuild = captured.getValue();
        assertEquals(BuildInfo.UNKNOWN_BUILD_ID, stubBuild.getBuildId());
        stubBuild.cleanUp();
    }

    /**
     * Test when the {@link IConfiguration#resolveDynamicOptions(DynamicRemoteFileResolver)} fails,
     * ensure we report all the logs and error.
     */
    @Test
    public void testResolveDynamicFails() throws Throwable {
        mDevice1 = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mDevice1.getIDevice()).andStubReturn(new StubDevice("serial1"));
        mDevice2 = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mDevice2.getIDevice()).andStubReturn(new StubDevice("serial1"));
        mContext.addAllocatedDevice("device1", mDevice1);
        mContext.addAllocatedDevice("device2", mDevice2);

        List<ITestInvocationListener> configListener = new ArrayList<>();
        configListener.add(mMockTestListener);
        EasyMock.expect(mMockConfig.getTestInvocationListeners())
                .andReturn(configListener)
                .times(2);
        EasyMock.expect(mMockConfig.getLogSaver()).andReturn(mMockLogSaver);
        EasyMock.expect(mMockConfig.getLogOutput()).andStubReturn(mMockLogger);
        EasyMock.expect(mMockConfig.getConfigurationDescription()).andReturn(mConfigDesc);
        mMockLogger.init();
        EasyMock.expect(mMockLogger.getLog())
                .andReturn(new ByteArrayInputStreamSource("fake".getBytes()));
        mMockLogger.closeLog();
        EasyMock.expectLastCall().times(2);

        mMockLogRegistry.registerLogger(mMockLogger);
        mMockLogRegistry.dumpToGlobalLog(mMockLogger);
        mMockLogRegistry.unregisterLogger();
        EasyMock.expectLastCall().times(2);

        EasyMock.expect(mMockConfig.getCommandLine()).andStubReturn("empty");
        EasyMock.expect(mMockConfig.getCommandOptions()).andStubReturn(new CommandOptions());
        EasyMock.expect(mMockConfig.getTests()).andStubReturn(new ArrayList<>());

        ConfigurationException configException = new ConfigurationException("failed to resolve");
        mMockConfig.resolveDynamicOptions(EasyMock.anyObject());
        EasyMock.expectLastCall().andThrow(configException);

        DeviceConfigurationHolder holder1 = new DeviceConfigurationHolder();
        mProvider1 = EasyMock.createMock(IBuildProvider.class);
        holder1.addSpecificConfig(mProvider1);
        EasyMock.expect(mMockConfig.getDeviceConfigByName("device1")).andStubReturn(holder1);
        EasyMock.expect(mDevice1.getSerialNumber()).andReturn("serial1");
        mMockConfig.cleanConfigurationData();

        mMockTestListener.invocationStarted(mContext);
        EasyMock.expect(mMockTestListener.getSummary()).andReturn(null);
        mMockLogSaver.invocationStarted(mContext);
        FailureDescription failure =
                FailureDescription.create(configException.getMessage(), FailureStatus.INFRA_FAILURE)
                        .setActionInProgress(ActionInProgress.FETCHING_ARTIFACTS);
        mMockTestListener.invocationFailed(EasyMock.eq(failure));
        mMockTestListener.testLog(EasyMock.anyObject(), EasyMock.anyObject(), EasyMock.anyObject());
        EasyMock.expect(
                        mMockLogSaver.saveLogData(
                                EasyMock.anyObject(), EasyMock.anyObject(), EasyMock.anyObject()))
                .andReturn(new LogFile("", "", LogDataType.TEXT));
        EasyMock.expect(
                        mMockLogSaver.saveLogData(
                                EasyMock.eq(TestInvocation.TRADEFED_END_HOST_LOG),
                                EasyMock.anyObject(),
                                EasyMock.anyObject()))
                .andReturn(new LogFile("", "", LogDataType.TEXT));
        mMockTestListener.invocationEnded(EasyMock.anyLong());
        EasyMock.expect(mMockTestListener.getSummary()).andReturn(null);
        mMockLogSaver.invocationEnded(EasyMock.anyLong());

        EasyMock.replay(
                mMockConfig,
                mMockRescheduler,
                mMockTestListener,
                mMockLogSaver,
                mMockLogger,
                mMockLogRegistry,
                mDevice1,
                mDevice2);
        mInvocation.invoke(
                mContext, mMockConfig, mMockRescheduler, new ITestInvocationListener[] {});
        EasyMock.verify(
                mMockConfig,
                mMockRescheduler,
                mMockTestListener,
                mMockLogSaver,
                mMockLogger,
                mMockLogRegistry,
                mDevice1,
                mDevice2);
    }

    @Test
    public void testRunBuildProvider_oneThrow() throws Throwable {
        makeTwoDeviceContext();

        List<ITestInvocationListener> configListener = new ArrayList<>();
        configListener.add(mMockTestListener);
        EasyMock.expect(mMockConfig.getTestInvocationListeners())
                .andReturn(configListener)
                .times(2);
        EasyMock.expect(mMockConfig.getLogSaver()).andReturn(mMockLogSaver);
        EasyMock.expect(mMockConfig.getLogOutput()).andStubReturn(mMockLogger);
        EasyMock.expect(mMockConfig.getConfigurationDescription()).andReturn(mConfigDesc);
        mMockLogger.init();
        EasyMock.expect(mMockLogger.getLog())
                .andReturn(new ByteArrayInputStreamSource("fake".getBytes()));
        mMockLogger.closeLog();
        EasyMock.expectLastCall().times(2);

        mMockLogRegistry.registerLogger(mMockLogger);
        mMockLogRegistry.dumpToGlobalLog(mMockLogger);
        mMockLogRegistry.unregisterLogger();
        EasyMock.expectLastCall().times(2);

        EasyMock.expect(mMockConfig.getCommandLine()).andStubReturn("empty");
        EasyMock.expect(mMockConfig.getCommandOptions()).andStubReturn(new CommandOptions());
        EasyMock.expect(mMockConfig.getTests()).andStubReturn(new ArrayList<>());
        mMockConfig.resolveDynamicOptions(EasyMock.anyObject());
        mMockConfig.cleanConfigurationData();

        mMockTestListener.invocationStarted(mContext);
        EasyMock.expect(mMockTestListener.getSummary()).andReturn(null);
        mMockLogSaver.invocationStarted(mContext);
        mMockTestListener.invocationFailed(EasyMock.<FailureDescription>anyObject());
        mMockTestListener.testLog(EasyMock.anyObject(), EasyMock.anyObject(), EasyMock.anyObject());
        EasyMock.expect(
                        mMockLogSaver.saveLogData(
                                EasyMock.anyObject(), EasyMock.anyObject(), EasyMock.anyObject()))
                .andReturn(new LogFile("", "", LogDataType.TEXT));
        EasyMock.expect(
                        mMockLogSaver.saveLogData(
                                EasyMock.eq(TestInvocation.TRADEFED_END_HOST_LOG),
                                EasyMock.anyObject(),
                                EasyMock.anyObject()))
                .andReturn(new LogFile("", "", LogDataType.TEXT));
        mMockTestListener.invocationEnded(EasyMock.anyLong());
        EasyMock.expect(mMockTestListener.getSummary()).andReturn(null);
        mMockLogSaver.invocationEnded(EasyMock.anyLong());

        IBuildInfo build1 = new BuildInfo();
        EasyMock.expect(mProvider1.getBuild()).andReturn(build1);
        // Second build is not found
        EasyMock.expect(mProvider2.getBuild())
                .andThrow(
                        new BuildRetrievalError(
                                "fail", InfraErrorIdentifier.ARTIFACT_DOWNLOAD_ERROR));
        // The downloaded build is cleaned
        mProvider1.cleanUp(build1);
        // A second build from the BuildRetrievalError is generated but still cleaned.
        mProvider2.cleanUp(EasyMock.anyObject());

        EasyMock.replay(
                mMockConfig,
                mMockRescheduler,
                mMockTestListener,
                mMockLogSaver,
                mMockLogger,
                mMockLogRegistry,
                mDevice1,
                mDevice2,
                mProvider1,
                mProvider2);
        mInvocation.invoke(
                mContext, mMockConfig, mMockRescheduler, new ITestInvocationListener[] {});
        EasyMock.verify(
                mMockConfig,
                mMockRescheduler,
                mMockTestListener,
                mMockLogSaver,
                mMockLogger,
                mMockLogRegistry,
                mDevice1,
                mDevice2,
                mProvider1,
                mProvider2);
    }

    /**
     * Test when the provider clean up throws an exception, we still continue to clean up the rest
     * to ensure nothing is left afterward.
     */
    @Test
    public void testRunBuildProvider_cleanUpThrow() throws Throwable {
        makeTwoDeviceContext();

        List<ITestInvocationListener> configListener = new ArrayList<>();
        configListener.add(mMockTestListener);
        EasyMock.expect(mMockConfig.getTestInvocationListeners())
                .andReturn(configListener)
                .times(2);
        EasyMock.expect(mMockConfig.getLogSaver()).andReturn(mMockLogSaver);
        EasyMock.expect(mMockConfig.getLogOutput()).andStubReturn(mMockLogger);
        EasyMock.expect(mMockConfig.getConfigurationDescription()).andReturn(mConfigDesc);
        mMockLogger.init();
        EasyMock.expect(mMockLogger.getLog())
                .andReturn(new ByteArrayInputStreamSource("fake".getBytes()));
        mMockLogger.closeLog();
        EasyMock.expectLastCall().times(2);

        mMockLogRegistry.registerLogger(mMockLogger);
        mMockLogRegistry.dumpToGlobalLog(mMockLogger);
        mMockLogRegistry.unregisterLogger();
        EasyMock.expectLastCall().times(2);

        EasyMock.expect(mMockConfig.getCommandLine()).andStubReturn("empty");
        EasyMock.expect(mMockConfig.getCommandOptions()).andStubReturn(new CommandOptions());
        EasyMock.expect(mMockConfig.getTests()).andStubReturn(new ArrayList<>());
        mMockConfig.resolveDynamicOptions(EasyMock.anyObject());
        mMockConfig.cleanConfigurationData();

        mMockTestListener.invocationStarted(mContext);
        EasyMock.expect(mMockTestListener.getSummary()).andReturn(null);
        mMockLogSaver.invocationStarted(mContext);
        mMockTestListener.invocationFailed(EasyMock.<FailureDescription>anyObject());
        mMockTestListener.testLog(EasyMock.anyObject(), EasyMock.anyObject(), EasyMock.anyObject());
        EasyMock.expect(
                        mMockLogSaver.saveLogData(
                                EasyMock.anyObject(), EasyMock.anyObject(), EasyMock.anyObject()))
                .andReturn(new LogFile("", "", LogDataType.TEXT));
        EasyMock.expect(
                        mMockLogSaver.saveLogData(
                                EasyMock.eq(TestInvocation.TRADEFED_END_HOST_LOG),
                                EasyMock.anyObject(),
                                EasyMock.anyObject()))
                .andReturn(new LogFile("", "", LogDataType.TEXT));
        mMockTestListener.invocationEnded(EasyMock.anyLong());
        EasyMock.expect(mMockTestListener.getSummary()).andReturn(null);
        mMockLogSaver.invocationEnded(EasyMock.anyLong());

        IBuildInfo build1 = new BuildInfo();
        EasyMock.expect(mProvider1.getBuild()).andReturn(build1);
        // Second build is not found
        EasyMock.expect(mProvider2.getBuild())
                .andThrow(
                        new BuildRetrievalError(
                                "fail", InfraErrorIdentifier.ARTIFACT_DOWNLOAD_ERROR));
        // The downloaded build is cleaned
        mProvider1.cleanUp(build1);
        EasyMock.expectLastCall().andThrow(new RuntimeException("I failed to clean!"));
        // A second build from the BuildRetrievalError is generated but still cleaned, even if the
        // first clean up failed.
        mProvider2.cleanUp(EasyMock.anyObject());

        EasyMock.replay(
                mMockConfig,
                mMockRescheduler,
                mMockTestListener,
                mMockLogSaver,
                mMockLogger,
                mMockLogRegistry,
                mDevice1,
                mDevice2,
                mProvider1,
                mProvider2);
        mInvocation.invoke(
                mContext, mMockConfig, mMockRescheduler, new ITestInvocationListener[] {});
        EasyMock.verify(
                mMockConfig,
                mMockRescheduler,
                mMockTestListener,
                mMockLogSaver,
                mMockLogger,
                mMockLogRegistry,
                mDevice1,
                mDevice2,
                mProvider1,
                mProvider2);
    }
}
