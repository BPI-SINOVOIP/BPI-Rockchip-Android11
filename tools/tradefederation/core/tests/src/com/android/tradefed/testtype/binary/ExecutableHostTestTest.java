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
package com.android.tradefed.testtype.binary;

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.DeviceBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.error.InfraErrorIdentifier;
import com.android.tradefed.result.error.DeviceErrorIdentifier;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import java.io.File;
import java.io.OutputStream;
import java.util.HashMap;

/** Unit tests for {@link ExecutableHostTest}. */
@RunWith(JUnit4.class)
public class ExecutableHostTestTest {

    private ExecutableHostTest mExecutableTest;
    private ITestInvocationListener mMockListener;
    private ITestDevice mMockDevice;
    private IRunUtil mMockRunUtil;
    private TestInformation mTestInfo;

    @Before
    public void setUp() {
        mMockListener = Mockito.mock(ITestInvocationListener.class);
        mMockDevice = Mockito.mock(ITestDevice.class);
        mMockRunUtil = Mockito.mock(IRunUtil.class);
        mExecutableTest =
                new ExecutableHostTest() {
                    @Override
                    IRunUtil createRunUtil() {
                        return mMockRunUtil;
                    }
                };
        InvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mMockDevice);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    @Test
    public void testRunHostExecutable_noBinaries() throws Exception {
        mExecutableTest.run(mTestInfo, mMockListener);

        verify(mMockListener, times(0)).testRunStarted(any(), anyInt());
    }

    @Test
    public void testRunHostExecutable_doesNotExists() throws Exception {
        String path = "/does/not/exists/path/bin/test";
        OptionSetter setter = new OptionSetter(mExecutableTest);
        setter.setOptionValue("binary", path);
        mTestInfo.getContext().addDeviceBuildInfo("device", new BuildInfo());
        mExecutableTest.run(mTestInfo, mMockListener);

        verify(mMockListener, Mockito.times(1)).testRunStarted(eq("test"), eq(0));
        FailureDescription failure =
                FailureDescription.create(
                                String.format(ExecutableBaseTest.NO_BINARY_ERROR, path),
                                FailureStatus.TEST_FAILURE)
                        .setErrorIdentifier(InfraErrorIdentifier.ARTIFACT_NOT_FOUND);
        verify(mMockListener, Mockito.times(1)).testRunFailed(failure);
        verify(mMockListener, Mockito.times(1))
                .testRunEnded(eq(0L), Mockito.<HashMap<String, Metric>>any());
    }

    @Test
    public void testRunHostExecutable() throws Exception {
        File tmpBinary = FileUtil.createTempFile("test-executable", "");
        try {
            OptionSetter setter = new OptionSetter(mExecutableTest);
            setter.setOptionValue("binary", tmpBinary.getAbsolutePath());

            CommandResult result = new CommandResult(CommandStatus.SUCCESS);
            doReturn(result)
                    .when(mMockRunUtil)
                    .runTimedCmd(
                            Mockito.anyLong(),
                            (OutputStream) Mockito.any(),
                            Mockito.any(),
                            Mockito.eq(tmpBinary.getAbsolutePath()));

            mExecutableTest.run(mTestInfo, mMockListener);

            verify(mMockListener, Mockito.times(1)).testRunStarted(eq(tmpBinary.getName()), eq(1));
            verify(mMockListener, Mockito.times(0)).testRunFailed((String) any());
            verify(mMockListener, Mockito.times(0)).testFailed(any(), (String) any());
            verify(mMockListener, Mockito.times(1))
                    .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());
        } finally {
            FileUtil.recursiveDelete(tmpBinary);
        }
    }

    @Test
    public void testRunHostExecutable_relativePath() throws Exception {
        File tmpBinary = FileUtil.createTempFile("test-executable", "");
        try {
            OptionSetter setter = new OptionSetter(mExecutableTest);
            setter.setOptionValue("binary", tmpBinary.getAbsolutePath());
            setter.setOptionValue("relative-path-execution", "true");

            CommandResult result = new CommandResult(CommandStatus.SUCCESS);
            doReturn(result)
                    .when(mMockRunUtil)
                    .runTimedCmd(
                            Mockito.anyLong(),
                            (OutputStream) Mockito.any(),
                            Mockito.any(),
                            Mockito.eq("bash"),
                            Mockito.eq("-c"),
                            Mockito.eq(
                                    String.format(
                                            "pushd %s; ./%s;",
                                            tmpBinary.getParent(), tmpBinary.getName())));

            mExecutableTest.run(mTestInfo, mMockListener);

            verify(mMockListener, Mockito.times(1)).testRunStarted(eq(tmpBinary.getName()), eq(1));
            verify(mMockListener, Mockito.times(0)).testRunFailed((String) any());
            verify(mMockListener, Mockito.times(0)).testFailed(any(), (String) any());
            verify(mMockListener, Mockito.times(1))
                    .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());
        } finally {
            FileUtil.recursiveDelete(tmpBinary);
        }
    }

    @Test
    public void testRunHostExecutable_dnae() throws Exception {
        File tmpBinary = FileUtil.createTempFile("test-executable", "");
        try {
            OptionSetter setter = new OptionSetter(mExecutableTest);
            setter.setOptionValue("binary", tmpBinary.getAbsolutePath());

            CommandResult result = new CommandResult(CommandStatus.SUCCESS);
            doReturn(result)
                    .when(mMockRunUtil)
                    .runTimedCmd(
                            Mockito.anyLong(),
                            (OutputStream) Mockito.any(),
                            Mockito.any(),
                            Mockito.eq(tmpBinary.getAbsolutePath()));

            doThrow(new DeviceNotAvailableException("test", "serial"))
                    .when(mMockDevice)
                    .waitForDeviceAvailable();
            DeviceNotAvailableException dnae = null;
            try {
                mExecutableTest.run(mTestInfo, mMockListener);
                fail("Should have thrown an exception.");
            } catch (DeviceNotAvailableException expected) {
                // Expected
                dnae = expected;
            }

            verify(mMockListener, Mockito.times(1)).testRunStarted(eq(tmpBinary.getName()), eq(1));
            FailureDescription failure =
                    FailureDescription.create(
                                    String.format(
                                            "Device became unavailable after %s.",
                                            tmpBinary.getAbsolutePath()),
                                    FailureStatus.LOST_SYSTEM_UNDER_TEST)
                            .setErrorIdentifier(DeviceErrorIdentifier.DEVICE_UNAVAILABLE)
                            .setCause(dnae);
            verify(mMockListener, Mockito.times(1)).testRunFailed(eq(failure));
            verify(mMockListener, Mockito.times(0)).testFailed(any(), (String) any());
            verify(mMockListener, Mockito.times(1))
                    .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());
        } finally {
            FileUtil.recursiveDelete(tmpBinary);
        }
    }

    /** If the binary is available from the tests directory we can find it and run it. */
    @Test
    public void testRunHostExecutable_search() throws Exception {
        File testsDir = FileUtil.createTempDir("executable-tests-dir");
        File tmpBinary = FileUtil.createTempFile("test-executable", "", testsDir);
        try {
            IDeviceBuildInfo info = new DeviceBuildInfo();
            info.setTestsDir(testsDir, "testversion");
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            OptionSetter setter = new OptionSetter(mExecutableTest);
            setter.setOptionValue("binary", tmpBinary.getName());

            CommandResult result = new CommandResult(CommandStatus.SUCCESS);
            doReturn(result)
                    .when(mMockRunUtil)
                    .runTimedCmd(
                            Mockito.anyLong(),
                            (OutputStream) Mockito.any(),
                            Mockito.any(),
                            Mockito.eq(tmpBinary.getAbsolutePath()));

            mExecutableTest.run(mTestInfo, mMockListener);

            verify(mMockListener, Mockito.times(1)).testRunStarted(eq(tmpBinary.getName()), eq(1));
            verify(mMockListener, Mockito.times(0)).testRunFailed((String) any());
            verify(mMockListener, Mockito.times(0)).testFailed(any(), (String) any());
            verify(mMockListener, Mockito.times(1))
                    .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    @Test
    public void testRunHostExecutable_notFound() throws Exception {
        File testsDir = FileUtil.createTempDir("executable-tests-dir");
        File tmpBinary = FileUtil.createTempFile("test-executable", "", testsDir);
        try {
            IDeviceBuildInfo info = new DeviceBuildInfo();
            info.setTestsDir(testsDir, "testversion");
            mTestInfo.getContext().addDeviceBuildInfo("device", info);
            OptionSetter setter = new OptionSetter(mExecutableTest);
            setter.setOptionValue("binary", tmpBinary.getName());
            tmpBinary.delete();

            CommandResult result = new CommandResult(CommandStatus.SUCCESS);
            doReturn(result)
                    .when(mMockRunUtil)
                    .runTimedCmd(Mockito.anyLong(), Mockito.eq(tmpBinary.getAbsolutePath()));

            mExecutableTest.run(mTestInfo, mMockListener);

            verify(mMockListener, Mockito.times(1)).testRunStarted(eq(tmpBinary.getName()), eq(0));
            FailureDescription failure =
                    FailureDescription.create(
                                    String.format(
                                            ExecutableBaseTest.NO_BINARY_ERROR,
                                            tmpBinary.getName()),
                                    FailureStatus.TEST_FAILURE)
                            .setErrorIdentifier(InfraErrorIdentifier.ARTIFACT_NOT_FOUND);
            verify(mMockListener, Mockito.times(1)).testRunFailed(eq(failure));
            verify(mMockListener, Mockito.times(1))
                    .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    @Test
    public void testRunHostExecutable_failure() throws Exception {
        File tmpBinary = FileUtil.createTempFile("test-executable", "");
        try {
            OptionSetter setter = new OptionSetter(mExecutableTest);
            setter.setOptionValue("binary", tmpBinary.getAbsolutePath());

            CommandResult result = new CommandResult(CommandStatus.FAILED);
            result.setExitCode(5);
            result.setStdout("stdout");

            doAnswer(
                            new Answer<CommandResult>() {

                                @Override
                                public CommandResult answer(InvocationOnMock invocation)
                                        throws Throwable {
                                    OutputStream outputStream = invocation.getArgument(1);
                                    outputStream.write("stdout".getBytes());
                                    return result;
                                }
                            })
                    .when(mMockRunUtil)
                    .runTimedCmd(
                            Mockito.anyLong(),
                            (OutputStream) Mockito.any(),
                            Mockito.any(),
                            Mockito.eq(tmpBinary.getAbsolutePath()));

            mExecutableTest.run(mTestInfo, mMockListener);

            verify(mMockListener, Mockito.times(1)).testRunStarted(eq(tmpBinary.getName()), eq(1));
            verify(mMockListener, Mockito.times(0)).testRunFailed((String) any());
            verify(mMockListener, Mockito.times(1))
                    .testFailed(
                            any(),
                            eq(
                                    FailureDescription.create("stdout\nExit Code: 5")
                                            .setFailureStatus(FailureStatus.TEST_FAILURE)));
            verify(mMockListener, Mockito.times(1))
                    .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());
        } finally {
            FileUtil.recursiveDelete(tmpBinary);
        }
    }

    @Test
    public void testRunHostExecutable_timeout() throws Exception {
        File tmpBinary = FileUtil.createTempFile("test-executable", "");
        try {
            OptionSetter setter = new OptionSetter(mExecutableTest);
            setter.setOptionValue("binary", tmpBinary.getAbsolutePath());

            CommandResult result = new CommandResult(CommandStatus.TIMED_OUT);
            result.setExitCode(5);
            result.setStdout("stdout");

            doAnswer(
                            new Answer<CommandResult>() {

                                @Override
                                public CommandResult answer(InvocationOnMock invocation)
                                        throws Throwable {
                                    OutputStream outputStream = invocation.getArgument(1);
                                    outputStream.write("stdout".getBytes());
                                    return result;
                                }
                            })
                    .when(mMockRunUtil)
                    .runTimedCmd(
                            Mockito.anyLong(),
                            (OutputStream) Mockito.any(),
                            Mockito.any(),
                            Mockito.eq(tmpBinary.getAbsolutePath()));

            mExecutableTest.run(mTestInfo, mMockListener);

            verify(mMockListener, Mockito.times(1)).testRunStarted(eq(tmpBinary.getName()), eq(1));
            verify(mMockListener, Mockito.times(0)).testRunFailed((String) any());
            verify(mMockListener, Mockito.times(1))
                    .testFailed(
                            any(),
                            eq(
                                    FailureDescription.create("stdout\nTimeout.\nExit Code: 5")
                                            .setFailureStatus(FailureStatus.TIMED_OUT)));
            verify(mMockListener, Mockito.times(1))
                    .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());
        } finally {
            FileUtil.recursiveDelete(tmpBinary);
        }
    }
}
