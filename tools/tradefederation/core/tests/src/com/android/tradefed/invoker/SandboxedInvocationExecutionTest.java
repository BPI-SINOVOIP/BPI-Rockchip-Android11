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
package com.android.tradefed.invoker;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.times;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.BuildInfoKey.BuildInfoFileKey;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IBuildProvider;
import com.android.tradefed.command.CommandRunner.ExitCode;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.ConfigurationDescriptor;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.guice.InvocationScope;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.invoker.sandbox.SandboxedInvocationExecution;
import com.android.tradefed.log.ILogRegistry;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ILogSaver;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.sandbox.ISandbox;
import com.android.tradefed.targetprep.ITargetCleaner;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.testtype.IInvocationContextReceiver;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import java.io.File;
import java.util.Arrays;

/** Unit tests for {@link SandboxedInvocationExecution}. */
@RunWith(JUnit4.class)
public class SandboxedInvocationExecutionTest {

    private TestInvocation mInvocation;
    @Mock ILogRegistry mMockLogRegistry;
    @Mock IRescheduler mMockRescheduler;
    @Mock ITestInvocationListener mMockListener;
    @Mock ILogSaver mMockLogSaver;
    @Mock TestBuildProviderInterface mMockProvider;
    @Mock ITargetPreparer mMockPreparer;
    @Mock ITargetCleaner mMockCleaner;
    @Mock ITestDevice mMockDevice;

    @Mock ISandbox mMockSandbox;

    private IConfiguration mConfig;
    private IInvocationContext mContext;
    private InvocationExecution mSpyExec;
    private SandboxedInvocationExecution mExecution;
    private TestInformation mTestInfo;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        try {
            GlobalConfiguration.createGlobalConfiguration(new String[] {"empty"});
        } catch (IllegalStateException e) {
            // Avoid double init issues.
        }

        mInvocation =
                new TestInvocation() {
                    @Override
                    ILogRegistry getLogRegistry() {
                        return mMockLogRegistry;
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

                    @Override
                    public IInvocationExecution createInvocationExec(RunMode mode) {
                        mSpyExec =
                                (InvocationExecution) Mockito.spy(super.createInvocationExec(mode));
                        doReturn("version 123").when(mSpyExec).getAdbVersion();
                        return mSpyExec;
                    }
                };
        mConfig = new Configuration("test", "test");
        mConfig.getConfigurationDescription().setSandboxed(true);
        mContext = new InvocationContext();
        mContext.addAllocatedDevice(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockDevice);
        mContext.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, new BuildInfo());

        doReturn(new ByteArrayInputStreamSource("".getBytes())).when(mMockDevice).getLogcat();

        mExecution = new SandboxedInvocationExecution();
        mTestInfo = TestInformation.newBuilder().setInvocationContext(mContext).build();
    }

    /** Interface to test the build provider receiving the context */
    private interface TestBuildProviderInterface
            extends IBuildProvider, IInvocationContextReceiver {}

    /** Basic test to go through the flow of a sandbox invocation. */
    @Test
    public void testSandboxInvocation() throws Throwable {
        // Setup as a sandbox invocation
        ConfigurationDescriptor descriptor = new ConfigurationDescriptor();
        descriptor.setSandboxed(true);
        mConfig.setConfigurationObject(
                Configuration.CONFIGURATION_DESCRIPTION_TYPE_NAME, descriptor);
        mConfig.setLogSaver(mMockLogSaver);
        mConfig.setBuildProvider(mMockProvider);

        doReturn(new LogFile("file", "url", LogDataType.TEXT))
                .when(mMockLogSaver)
                .saveLogData(any(), any(), any());

        mInvocation.invoke(mContext, mConfig, mMockRescheduler, mMockListener);

        // Ensure that in sandbox we don't download again.
        Mockito.verify(mMockProvider, times(0)).getBuild();
        // Ensure that the context is still set.
        Mockito.verify(mMockProvider, times(1)).setInvocationContext(mContext);
    }

    /**
     * Test that the parent invocation of sandboxing does not call shardConfig. Sharding should
     * happen in the subprocess.
     */
    @Test
    public void testParentSandboxInvocation_sharding() throws Throwable {
        mInvocation =
                new TestInvocation() {
                    @Override
                    ILogRegistry getLogRegistry() {
                        return mMockLogRegistry;
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

                    @Override
                    public IInvocationExecution createInvocationExec(RunMode mode) {
                        return new InvocationExecution() {
                            @Override
                            public boolean shardConfig(
                                    IConfiguration config,
                                    TestInformation testInfo,
                                    IRescheduler rescheduler,
                                    ITestLogger logger) {
                                // Ensure that sharding is not called against a sandbox
                                // configuration run
                                throw new RuntimeException("Should not be called.");
                            }

                            @Override
                            protected String getAdbVersion() {
                                return "version 123";
                            }
                        };
                    }
                };

        ConfigurationDescriptor descriptor = new ConfigurationDescriptor();
        // We are the parent kick off the sandbox
        mConfig.getCommandOptions().setShouldUseSandboxing(true);
        mConfig.getCommandOptions().setShardCount(5);
        mConfig.getCommandOptions().setShardIndex(1);
        mConfig.setConfigurationObject(
                Configuration.CONFIGURATION_DESCRIPTION_TYPE_NAME, descriptor);
        mConfig.setConfigurationObject(Configuration.SANDBOX_TYPE_NAME, mMockSandbox);

        mConfig.setLogSaver(mMockLogSaver);
        mConfig.setBuildProvider(mMockProvider);

        doReturn(new BuildInfo()).when(mMockProvider).getBuild();

        doReturn(new LogFile("file", "url", LogDataType.TEXT))
                .when(mMockLogSaver)
                .saveLogData(any(), any(), any());

        CommandResult result = new CommandResult(CommandStatus.SUCCESS);
        doReturn(result).when(mMockSandbox).run(any(), any());

        mInvocation.invoke(mContext, mConfig, mMockRescheduler, mMockListener);
    }

    /**
     * Test that the parent sandbox process does not call clean up when target prep was not called.
     */
    @Test
    public void testParentSandboxInvocation() throws Throwable {
        ConfigurationDescriptor descriptor = new ConfigurationDescriptor();
        // We are the parent kick off the sandbox
        mConfig.getCommandOptions().setShouldUseSandboxing(true);
        mConfig.setConfigurationObject(
                Configuration.CONFIGURATION_DESCRIPTION_TYPE_NAME, descriptor);
        mConfig.setLogSaver(mMockLogSaver);
        mConfig.setBuildProvider(mMockProvider);
        mConfig.setTargetPreparers(Arrays.asList(mMockPreparer, mMockCleaner));
        mConfig.setConfigurationObject(Configuration.SANDBOX_TYPE_NAME, mMockSandbox);

        doReturn(new LogFile("file", "url", LogDataType.TEXT))
                .when(mMockLogSaver)
                .saveLogData(any(), any(), any());

        CommandResult result = new CommandResult(CommandStatus.SUCCESS);
        result.setExitCode(0);
        doReturn(result).when(mMockSandbox).run(any(), any());

        doReturn(new BuildInfo()).when(mMockProvider).getBuild();

        mInvocation.invoke(mContext, mConfig, mMockRescheduler, mMockListener);

        // Ensure no preparer and cleaner are called in parent process
        Mockito.verify(mMockPreparer, times(0)).setUp(any());
        Mockito.verify(mMockCleaner, times(0)).tearDown(any(), any());
    }

    /**
     * Test that when sharding does not return any tests for a shard, we still report start and stop
     * of the invocation.
     */
    @Test
    public void testInvocation_sharding_notTests() throws Throwable {
        mInvocation =
                new TestInvocation() {
                    @Override
                    ILogRegistry getLogRegistry() {
                        return mMockLogRegistry;
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

                    @Override
                    public IInvocationExecution createInvocationExec(RunMode mode) {
                        mSpyExec =
                                (InvocationExecution) Mockito.spy(super.createInvocationExec(mode));
                        doReturn("version 123").when(mSpyExec).getAdbVersion();
                        return mSpyExec;
                    }
                };

        ConfigurationDescriptor descriptor = new ConfigurationDescriptor();
        mConfig.getCommandOptions().setShardCount(5);
        mConfig.getCommandOptions().setShardIndex(1);
        mConfig.setConfigurationObject(
                Configuration.CONFIGURATION_DESCRIPTION_TYPE_NAME, descriptor);

        mConfig.setLogSaver(mMockLogSaver);
        mConfig.setBuildProvider(mMockProvider);

        doReturn(new LogFile("file", "url", LogDataType.TEXT))
                .when(mMockLogSaver)
                .saveLogData(any(), any(), any());

        CommandResult result = new CommandResult(CommandStatus.SUCCESS);
        doReturn(result).when(mMockSandbox).run(any(), any());

        doReturn(new BuildInfo()).when(mMockProvider).getBuild();

        mInvocation.invoke(mContext, mConfig, mMockRescheduler, mMockListener);
        // No tests to run but we still call start/end
        Mockito.verify(mMockListener).invocationStarted(mContext);
        Mockito.verify(mMockListener).invocationEnded(0L);
        // Invocation did not start for real so context is not locked.
        mContext.addInvocationAttribute("test", "test");
        // Device early preInvocationSetup was called and even if no tests run we still call tear
        // down
        Mockito.verify(mMockDevice).preInvocationSetup(any());
        Mockito.verify(mMockDevice).postInvocationTearDown(null);
    }

    /**
     * Ensure that in case of preInvocationSetup failure, we still report the invocation failure and
     * the logs.
     */
    @Test
    public void testInvocation_preInvocationFailing() throws Throwable {
        mInvocation =
                new TestInvocation() {
                    @Override
                    ILogRegistry getLogRegistry() {
                        return mMockLogRegistry;
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

                    @Override
                    public IInvocationExecution createInvocationExec(RunMode mode) {
                        mSpyExec =
                                (InvocationExecution) Mockito.spy(super.createInvocationExec(mode));
                        doReturn("version 123").when(mSpyExec).getAdbVersion();
                        return mSpyExec;
                    }
                };

        ConfigurationDescriptor descriptor = new ConfigurationDescriptor();
        mConfig.getCommandOptions().setShardCount(5);
        mConfig.getCommandOptions().setShardIndex(1);
        mConfig.setConfigurationObject(
                Configuration.CONFIGURATION_DESCRIPTION_TYPE_NAME, descriptor);

        mConfig.setLogSaver(mMockLogSaver);
        mConfig.setBuildProvider(mMockProvider);

        doReturn(new LogFile("file", "url", LogDataType.TEXT))
                .when(mMockLogSaver)
                .saveLogData(any(), any(), any());

        CommandResult result = new CommandResult(CommandStatus.SUCCESS);
        doReturn(result).when(mMockSandbox).run(any(), any());

        IBuildInfo info = new BuildInfo();
        doReturn(info).when(mMockProvider).getBuild();

        DeviceNotAvailableException exception = new DeviceNotAvailableException("reason", "serial");
        doThrow(exception).when(mMockDevice).preInvocationSetup(eq(info));

        mInvocation.invoke(mContext, mConfig, mMockRescheduler, mMockListener);
        // No tests to run but we still call start/end
        Mockito.verify(mMockListener).invocationStarted(mContext);
        FailureDescription failure = FailureDescription.create(exception.getMessage());
        failure.setCause(exception).setFailureStatus(FailureStatus.INFRA_FAILURE);
        Mockito.verify(mMockListener).invocationFailed(failure);
        Mockito.verify(mMockListener).invocationEnded(0L);
        // Invocation did not start for real so context is not locked.
        mContext.addInvocationAttribute("test", "test");
        // Device early preInvocationSetup was called and even if no tests run we still call tear
        // down
        Mockito.verify(mMockDevice).preInvocationSetup(any());
        Mockito.verify(mMockDevice).postInvocationTearDown(exception);
    }

    @Test
    public void testBackFill() throws Exception {
        IBuildInfo info = new BuildInfo();
        File buildFile = FileUtil.createTempFile("sandboxedfile", "test");
        info.setFile(BuildInfoFileKey.HOST_LINKED_DIR, buildFile, "1");
        File tmpFile = FileUtil.createTempFile("sandboxedTest", "test");
        try {
            info.setFile("random-key", tmpFile, "1");
            mContext.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, info);
            assertEquals(0, mTestInfo.executionFiles().getAll().size());
            mExecution.fetchBuild(mTestInfo, mConfig, null, null);
            // After fetchBuild, the TestInformation is filled
            assertEquals(3, mTestInfo.executionFiles().getAll().size());
            assertTrue(mTestInfo.executionFiles().containsKey("random-key"));
            assertTrue(
                    mTestInfo
                            .executionFiles()
                            .containsKey(FilesKey.HOST_TESTS_DIRECTORY.toString()));
            assertTrue(
                    mTestInfo
                            .executionFiles()
                            .containsKey(BuildInfoFileKey.HOST_LINKED_DIR.toString()));
        } finally {
            FileUtil.deleteFile(tmpFile);
            FileUtil.deleteFile(buildFile);
        }
    }
}
