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
package com.android.tradefed.testtype.rust;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.util.HashMap;

/** Unit tests for {@link RustBinaryHostTest}. */
@RunWith(JUnit4.class)
public class RustBinaryHostTestTest {
    private RustBinaryHostTest mTest;
    private IRunUtil mMockRunUtil;
    private IBuildInfo mMockBuildInfo;
    private TestInformation mTestInfo;
    private ITestInvocationListener mMockListener;
    // TODO(chh): maybe we will need mFakeAdb later like PythonBinaryHostTestTest.

    @Before
    public void setUp() throws Exception {
        mMockRunUtil = EasyMock.createMock(IRunUtil.class);
        mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        mMockListener = EasyMock.createMock(ITestInvocationListener.class);
        mTest =
                new RustBinaryHostTest() {
                    @Override
                    IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }
                };
        mTest.setBuild(mMockBuildInfo);
        InvocationContext context = new InvocationContext();
        context.addDeviceBuildInfo("device", mMockBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    private CommandResult newCommandResult(CommandStatus status, String stderr, String stdout) {
        CommandResult res = new CommandResult();
        res.setStatus(status);
        res.setStderr(stderr);
        res.setStdout(stdout);
        return res;
    }

    private String resultCount(int pass, int fail, int ignore) {
        return "test result: ok. " + pass + " passed; " + fail + " failed; " + ignore + " ignored;";
    }

    private CommandResult successResult(String stderr, String stdout) throws Exception {
        return newCommandResult(CommandStatus.SUCCESS, stderr, stdout);
    }

    /** Add mocked call "binary --list" to count the number of tests. */
    private void mockCountTests(File binary, int numOfTest) throws Exception {
        EasyMock.expect(
                        mMockRunUtil.runTimedCmdSilently(
                                EasyMock.anyLong(),
                                EasyMock.eq(binary.getAbsolutePath()),
                                EasyMock.eq("--list")))
                .andReturn(successResult("", numOfTest + " tests, 0 benchmarks"));
    }

    /** Add mocked testRunStarted call to the listener. */
    private void mockListenerStarted(File binary, int count) throws Exception {
        mMockListener.testRunStarted(
                EasyMock.eq(binary.getName()),
                EasyMock.eq(count),
                EasyMock.anyInt(),
                EasyMock.anyLong());
    }

    /** Add mocked call to check listener log file. */
    private void mockListenerLog(File binary) {
        mMockListener.testLog(
                EasyMock.eq(binary.getName() + "-stderr"),
                EasyMock.eq(LogDataType.TEXT),
                EasyMock.anyObject());
    }

    private void mockTestRunExpect(File binary, CommandResult res) throws Exception {
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(), EasyMock.eq(binary.getAbsolutePath())))
                .andReturn(res);
    }

    /** Add mocked call to testRunEnded. */
    private void mockTestRunEnded() {
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
    }

    /** Call replay/run/verify. */
    private void callReplayRunVerify() throws Exception {
        EasyMock.replay(mMockRunUtil, mMockBuildInfo, mMockListener);
        mTest.run(mTestInfo, mMockListener);
        EasyMock.verify(mMockRunUtil, mMockBuildInfo, mMockListener);
    }

    /** Test that when running a rust binary the output is parsed to obtain results. */
    @Test
    public void testRun() throws Exception {
        File binary = FileUtil.createTempFile("rust-dir", "");
        try {
            OptionSetter setter = new OptionSetter(mTest);
            setter.setOptionValue("test-file", binary.getAbsolutePath());
            mockCountTests(binary, 9);
            mockListenerStarted(binary, 9);
            mockListenerLog(binary);
            CommandResult res = successResult("", resultCount(6, 1, 2));
            mockTestRunExpect(binary, res);
            mockTestRunEnded();
            callReplayRunVerify();
        } finally {
            FileUtil.deleteFile(binary);
        }
    }

    /**
     * Test running the rust tests when an adb path has been set. In that case we ensure the rust
     * test will use the provided adb.
     */
    @Test
    public void testRun_withAdbPath() throws Exception {
        mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        mTest.setBuild(mMockBuildInfo);

        File binary = FileUtil.createTempFile("rust-dir", "");
        try {
            OptionSetter setter = new OptionSetter(mTest);
            setter.setOptionValue("test-file", binary.getAbsolutePath());
            mockCountTests(binary, 9);
            mockListenerStarted(binary, 9);
            mockListenerLog(binary);
            CommandResult res = successResult("", resultCount(6, 1, 2));
            mockTestRunExpect(binary, res);
            mockTestRunEnded();
            callReplayRunVerify();
        } finally {
            FileUtil.deleteFile(binary);
        }
    }

    /** If the binary returns an exception status, it is treated as a failed test. */
    @Test
    public void testRunFail_exception() throws Exception {
        File binary = FileUtil.createTempFile("rust-dir", "");
        try {
            OptionSetter setter = new OptionSetter(mTest);
            setter.setOptionValue("test-file", binary.getAbsolutePath());
            mockCountTests(binary, 0);
            mockListenerStarted(binary, 0);
            mockListenerLog(binary);
            CommandResult res = newCommandResult(CommandStatus.EXCEPTION, "Err.", "Exception.");
            mockTestRunExpect(binary, res);
            mMockListener.testRunFailed((String) EasyMock.anyObject());
            mMockListener.testRunFailed((FailureDescription) EasyMock.anyObject());
            mockTestRunEnded();
            callReplayRunVerify();
        } finally {
            FileUtil.deleteFile(binary);
        }
    }

    /** If the binary reports a FAILED status, it is treated as a failed test. */
    @Test
    public void testRunFail_list() throws Exception {
        File binary = FileUtil.createTempFile("rust-dir", "");
        try {
            OptionSetter setter = new OptionSetter(mTest);
            setter.setOptionValue("test-file", binary.getAbsolutePath());
            CommandResult listRes = newCommandResult(CommandStatus.FAILED, "", "");
            EasyMock.expect(
                            mMockRunUtil.runTimedCmdSilently(
                                    EasyMock.anyLong(),
                                    EasyMock.eq(binary.getAbsolutePath()),
                                    EasyMock.eq("--list")))
                    .andReturn(listRes);
            mMockListener.testRunStarted(
                    EasyMock.eq(binary.getName()),
                    EasyMock.eq(0),
                    EasyMock.anyInt(),
                    EasyMock.anyLong());
            mockListenerLog(binary);
            CommandResult res = newCommandResult(CommandStatus.FAILED, "", resultCount(6, 1, 2));
            mockTestRunExpect(binary, res);
            mMockListener.testRunFailed((FailureDescription) EasyMock.anyObject());
            mockTestRunEnded();
            callReplayRunVerify();
        } finally {
            FileUtil.deleteFile(binary);
        }
    }

    /** If the binary reports a FAILED status, it is treated as a failed test. */
    @Test
    public void testRunFail_failureOnly() throws Exception {
        File binary = FileUtil.createTempFile("rust-dir", "");
        try {
            OptionSetter setter = new OptionSetter(mTest);
            setter.setOptionValue("test-file", binary.getAbsolutePath());
            mockCountTests(binary, 9);
            mockListenerStarted(binary, 9);
            mockListenerLog(binary);
            CommandResult res = newCommandResult(CommandStatus.FAILED, "", resultCount(6, 1, 2));
            mockTestRunExpect(binary, res);
            mMockListener.testRunFailed((FailureDescription) EasyMock.anyObject());
            mockTestRunEnded();
            callReplayRunVerify();
        } finally {
            FileUtil.deleteFile(binary);
        }
    }

    /** Test the exclude filtering of test methods. */
    @Test
    public void testExcludeFilter() throws Exception {
        File binary = FileUtil.createTempFile("rust-dir", "");
        try {
            OptionSetter setter = new OptionSetter(mTest);
            setter.setOptionValue("test-file", binary.getAbsolutePath());
            setter.setOptionValue("exclude-filter", "NotMe");
            setter.setOptionValue("exclude-filter", "Long");
            EasyMock.expect(
                            mMockRunUtil.runTimedCmdSilently(
                                    EasyMock.anyLong(),
                                    EasyMock.eq(binary.getAbsolutePath()),
                                    EasyMock.eq("--skip"),
                                    EasyMock.eq("NotMe"),
                                    EasyMock.eq("--skip"),
                                    EasyMock.eq("Long"),
                                    EasyMock.eq("--list")))
                    .andReturn(successResult("", "9 tests, 0 benchmarks"));
            mockListenerStarted(binary, 9);
            mockListenerLog(binary);
            CommandResult res = successResult("", resultCount(6, 1, 2));
            EasyMock.expect(
                            mMockRunUtil.runTimedCmd(
                                    EasyMock.anyLong(),
                                    EasyMock.eq(binary.getAbsolutePath()),
                                    EasyMock.eq("--skip"),
                                    EasyMock.eq("NotMe"),
                                    EasyMock.eq("--skip"),
                                    EasyMock.eq("Long")))
                    .andReturn(res);

            mockTestRunEnded();
            callReplayRunVerify();
        } finally {
            FileUtil.deleteFile(binary);
        }
    }

    /** Test both include and exclude filters. */
    @Test
    public void testIncludeExcludeFilter() throws Exception {
        File binary = FileUtil.createTempFile("rust-dir", "");
        try {
            OptionSetter setter = new OptionSetter(mTest);
            setter.setOptionValue("test-file", binary.getAbsolutePath());
            setter.setOptionValue("exclude-filter", "NotMe");
            setter.setOptionValue("include-filter", "OnlyMe");
            setter.setOptionValue("exclude-filter", "Other");
            setter.setOptionValue("include-filter", "Me2");
            // We always pass the include-filter before exclude-filter strings.
            // Multiple include filters are accepted but all except the 1st are ignored.
            EasyMock.expect(
                            mMockRunUtil.runTimedCmdSilently(
                                    EasyMock.anyLong(),
                                    EasyMock.eq(binary.getAbsolutePath()),
                                    EasyMock.eq("OnlyMe"),
                                    EasyMock.eq("Me2"),
                                    EasyMock.eq("--skip"),
                                    EasyMock.eq("NotMe"),
                                    EasyMock.eq("--skip"),
                                    EasyMock.eq("Other"),
                                    EasyMock.eq("--list")))
                    .andReturn(successResult("", "3 tests, 0 benchmarks"));
            mockListenerStarted(binary, 3);

            mockListenerLog(binary);
            CommandResult res = successResult("", resultCount(3, 0, 0));
            EasyMock.expect(
                            mMockRunUtil.runTimedCmd(
                                    EasyMock.anyLong(),
                                    EasyMock.eq(binary.getAbsolutePath()),
                                    EasyMock.eq("OnlyMe"),
                                    EasyMock.eq("Me2"),
                                    EasyMock.eq("--skip"),
                                    EasyMock.eq("NotMe"),
                                    EasyMock.eq("--skip"),
                                    EasyMock.eq("Other")))
                    .andReturn(res);

            mockTestRunEnded();
            callReplayRunVerify();
        } finally {
            FileUtil.deleteFile(binary);
        }
    }
}
