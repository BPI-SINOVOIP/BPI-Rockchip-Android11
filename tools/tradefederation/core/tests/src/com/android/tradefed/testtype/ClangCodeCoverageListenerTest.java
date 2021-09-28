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

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IBuildProvider;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.metrics.proto.MetricMeasurement;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.testtype.coverage.CoverageOptions;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import com.google.common.base.VerifyException;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.protobuf.ByteString;

import org.apache.commons.compress.archivers.tar.TarArchiveEntry;
import org.apache.commons.compress.archivers.tar.TarArchiveOutputStream;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/** Unit tests for {@link ClangCodeCoverageListener}. */
@RunWith(JUnit4.class)
public class ClangCodeCoverageListenerTest {

    private static final String RUN_NAME = "SomeTest";
    private static final int TEST_COUNT = 5;
    private static final long ELAPSED_TIME = 1000;

    @Rule public TemporaryFolder folder = new TemporaryFolder();

    private HashMap<String, MetricMeasurement.Metric> mMetrics;

    /** Fakes, Mocks and Spies. */
    @Mock IBuildInfo mMockBuildInfo;

    @Mock IConfiguration mMockConfiguration;
    @Mock IBuildProvider mMockBuildProvider;
    @Mock ITestDevice mMockDevice;
    @Spy CommandArgumentCaptor mCommandArgumentCaptor;
    LogFileReader mFakeListener = new LogFileReader();

    /** Options for coverage. */
    CoverageOptions mCoverageOptions;

    OptionSetter mCoverageOptionsSetter = null;

    /** Object under test. */
    ClangCodeCoverageListener mListener;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        Map<String, String> metric = new HashMap<>();
        mMetrics = TfMetricProtoUtil.upgradeConvert(metric);

        mCoverageOptions = new CoverageOptions();
        mCoverageOptionsSetter = new OptionSetter(mCoverageOptions);

        doReturn(mCoverageOptions).when(mMockConfiguration).getCoverageOptions();
        doReturn(mMockBuildProvider).when(mMockConfiguration).getBuildProvider();
        doReturn(mMockBuildInfo).when(mMockBuildProvider).getBuild();

        mListener = new ClangCodeCoverageListener(mMockDevice, mFakeListener);
        mListener.setConfiguration(mMockConfiguration);
        mListener.setRunUtil(mCommandArgumentCaptor);
    }

    @Test
    public void coverageDisabled_noCoverageLog() {
        // Simulate a test run.
        mListener.testRunStarted(RUN_NAME, TEST_COUNT);
        mListener.testRunEnded(ELAPSED_TIME, mMetrics);
        mListener.invocationEnded(ELAPSED_TIME);

        // Verify testLog(...) was never called.
        assertThat(mFakeListener.getLogs()).isEmpty();
    }

    @Test
    public void clangCoverageDisabled_noCoverageLog() throws Exception {
        mCoverageOptionsSetter.setOptionValue("coverage", "true");

        // Simulate a test run.
        mListener.testRunStarted(RUN_NAME, TEST_COUNT);
        mListener.testRunEnded(ELAPSED_TIME, mMetrics);
        mListener.invocationEnded(ELAPSED_TIME);

        // Verify testLog(...) was never called.
        assertThat(mFakeListener.getLogs()).isEmpty();
    }

    @Test
    public void coverageFlushEnabled_flushCalled() throws Exception {
        mCoverageOptionsSetter.setOptionValue("coverage", "true");
        mCoverageOptionsSetter.setOptionValue("coverage-toolchain", "CLANG");
        mCoverageOptionsSetter.setOptionValue("coverage-flush", "true");

        // Setup mocks.
        doReturn(true).when(mMockDevice).enableAdbRoot();
        doReturn(true).when(mMockDevice).isAdbRoot();
        doReturn(createTar(ImmutableMap.of())).when(mMockDevice).pullFile(anyString());

        // Simulate a test run.
        mListener.testRunStarted(RUN_NAME, TEST_COUNT);
        mListener.testRunEnded(ELAPSED_TIME, mMetrics);
        mListener.invocationEnded(ELAPSED_TIME);

        // Verify the flush-all-coverage command was called.
        verify(mMockDevice).executeShellCommand("kill -37 -1");
    }

    @Test
    public void testRun_logsCoverageFile() throws Exception {
        mCoverageOptionsSetter.setOptionValue("coverage", "true");
        mCoverageOptionsSetter.setOptionValue("coverage-toolchain", "CLANG");

        // Setup mocks.
        doReturn(true).when(mMockDevice).enableAdbRoot();
        File tarGz =
                createTar(
                        ImmutableMap.of(
                                "path/to/coverage.profraw",
                                ByteString.copyFromUtf8("coverage.profraw"),
                                "path/to/.hidden/coverage2.profraw",
                                ByteString.copyFromUtf8("coverage2.profraw")));
        doReturn(tarGz).when(mMockDevice).pullFile(anyString());

        doReturn(createProfileToolZip()).when(mMockBuildInfo).getFile(anyString());

        // Simulate a test run.
        mListener.testRunStarted(RUN_NAME, TEST_COUNT);
        mListener.testRunEnded(ELAPSED_TIME, mMetrics);
        mListener.invocationEnded(ELAPSED_TIME);

        // Verify that the command line contains the files above.
        List<String> command = mCommandArgumentCaptor.getCommand();
        checkListContainsSuffixes(
                command,
                ImmutableList.of(
                        "llvm-profdata",
                        "path/to/coverage.profraw",
                        "path/to/.hidden/coverage2.profraw"));

        // Verify testLog(..) was called with a single indexed profile data.
        List<ByteString> logs = mFakeListener.getLogs();
        assertThat(logs).hasSize(1);
    }

    @Test
    public void testOtherFileTypes_ignored() throws Exception {
        mCoverageOptionsSetter.setOptionValue("coverage", "true");
        mCoverageOptionsSetter.setOptionValue("coverage-toolchain", "CLANG");

        // Setup mocks.
        doReturn(true).when(mMockDevice).enableAdbRoot();
        File tarGz =
                createTar(
                        ImmutableMap.of(
                                "path/to/coverage.profraw",
                                ByteString.copyFromUtf8("coverage.profraw"),
                                "path/to/coverage.gcda",
                                ByteString.copyFromUtf8("coverage.gcda")));
        doReturn(tarGz).when(mMockDevice).pullFile(anyString());

        doReturn(createProfileToolZip()).when(mMockBuildInfo).getFile(anyString());

        // Simulate a test run.
        mListener.testRunStarted(RUN_NAME, TEST_COUNT);
        mListener.testRunEnded(ELAPSED_TIME, mMetrics);
        mListener.invocationEnded(ELAPSED_TIME);

        // Verify that the command line contains the files above, not including the .gcda file.
        List<String> command = mCommandArgumentCaptor.getCommand();
        checkListContainsSuffixes(
                command, ImmutableList.of("llvm-profdata", "path/to/coverage.profraw"));
        checkListDoesNotContainSuffix(command, "path/to/coverage.gcda");

        // Verify testLog(..) was called with a single indexed profile data.
        List<ByteString> logs = mFakeListener.getLogs();
        assertThat(logs).hasSize(1);
    }

    @Test
    public void testNoClangMeasurements_noLogFile() throws Exception {
        mCoverageOptionsSetter.setOptionValue("coverage", "true");
        mCoverageOptionsSetter.setOptionValue("coverage-toolchain", "CLANG");

        // Setup mocks.
        doReturn(true).when(mMockDevice).enableAdbRoot();
        doReturn(createTar(ImmutableMap.of())).when(mMockDevice).pullFile(anyString());

        // Simulate a test run.
        mListener.testRunStarted(RUN_NAME, TEST_COUNT);
        mListener.testRunEnded(ELAPSED_TIME, mMetrics);
        mListener.invocationEnded(ELAPSED_TIME);

        // Verify testLog(..) was never called.
        assertThat(mFakeListener.getLogs()).isEmpty();
    }

    @Test
    public void testProfileToolInConfiguration_notFromBuild() throws Exception {
        mCoverageOptionsSetter.setOptionValue("coverage", "true");
        mCoverageOptionsSetter.setOptionValue("coverage-toolchain", "CLANG");
        mCoverageOptionsSetter.setOptionValue("llvm-profdata-path", "/path/to/some/directory");

        // Setup mocks.
        doReturn(true).when(mMockDevice).enableAdbRoot();
        File tarGz =
                createTar(
                        ImmutableMap.of(
                                "path/to/coverage.profraw",
                                ByteString.copyFromUtf8("coverage.profraw"),
                                "path/to/.hidden/coverage2.profraw",
                                ByteString.copyFromUtf8("coverage2.profraw")));
        doReturn(tarGz).when(mMockDevice).pullFile(anyString());

        // Simulate a test run.
        mListener.testRunStarted(RUN_NAME, TEST_COUNT);
        mListener.testRunEnded(ELAPSED_TIME, mMetrics);
        mListener.invocationEnded(ELAPSED_TIME);

        // Verify that the command line contains the llvm-profile-path set above.
        List<String> command = mCommandArgumentCaptor.getCommand();
        assertThat(command.get(0)).isEqualTo("/path/to/some/directory/bin/llvm-profdata");
    }

    @Test
    public void testProfileToolNotFound_throwsException() throws Exception {
        mCoverageOptionsSetter.setOptionValue("coverage", "true");
        mCoverageOptionsSetter.setOptionValue("coverage-toolchain", "CLANG");

        // Setup mocks.
        doReturn(true).when(mMockDevice).enableAdbRoot();
        File tarGz =
                createTar(
                        ImmutableMap.of(
                                "path/to/coverage.profraw",
                                ByteString.copyFromUtf8("coverage.profraw"),
                                "path/to/.hidden/coverage2.profraw",
                                ByteString.copyFromUtf8("coverage2.profraw")));
        doReturn(tarGz).when(mMockDevice).pullFile(anyString());

        // Simulate a test run.
        try {
            mListener.testRunStarted(RUN_NAME, TEST_COUNT);
            mListener.testRunEnded(ELAPSED_TIME, mMetrics);
            mListener.invocationEnded(ELAPSED_TIME);
            fail("an exception should have been thrown");
        } catch (VerifyException e) {
            // Expected.
            assertThat(e).hasMessageThat().contains("llvm-profdata");
        }

        // Verify testLog(..) was never called.
        assertThat(mFakeListener.getLogs()).isEmpty();
    }

    @Test
    public void testProfileToolFailed_throwsException() throws Exception {
        mCoverageOptionsSetter.setOptionValue("coverage", "true");
        mCoverageOptionsSetter.setOptionValue("coverage-toolchain", "CLANG");

        // Setup mocks.
        doReturn(true).when(mMockDevice).enableAdbRoot();
        File tarGz =
                createTar(
                        ImmutableMap.of(
                                "path/to/coverage.profraw",
                                ByteString.copyFromUtf8("coverage.profraw"),
                                "path/to/.hidden/coverage2.profraw",
                                ByteString.copyFromUtf8("coverage2.profraw")));
        doReturn(tarGz).when(mMockDevice).pullFile(anyString());
        doReturn(createProfileToolZip()).when(mMockBuildInfo).getFile(anyString());

        mCommandArgumentCaptor.setResult(CommandStatus.FAILED);

        // Simulate a test run.
        try {
            mListener.testRunStarted(RUN_NAME, TEST_COUNT);
            mListener.testRunEnded(ELAPSED_TIME, mMetrics);
            fail("an exception should have been thrown");
        } catch (RuntimeException e) {
            // Expected.
            assertThat(e).hasMessageThat().contains("merge Clang profile data");
        }
        mListener.invocationEnded(ELAPSED_TIME);

        // Verify testLog(..) was never called.
        assertThat(mFakeListener.getLogs()).isEmpty();
    }

    abstract static class CommandArgumentCaptor implements IRunUtil {
        private List<String> mCommand;
        private CommandResult mResult = new CommandResult(CommandStatus.SUCCESS);

        /** Stores the command for retrieval later. */
        @Override
        public CommandResult runTimedCmd(long timeout, String... cmd) {
            mCommand = Arrays.asList(cmd);
            return mResult;
        }

        void setResult(CommandStatus status) {
            mResult = new CommandResult(status);
        }

        List<String> getCommand() {
            return mCommand;
        }
    }

    /** An {@link ITestInvocationListener} which reads test log data streams for verification. */
    private static class LogFileReader implements ITestInvocationListener {
        private List<ByteString> mLogs = new ArrayList<>();

        /** Reads the contents of the {@code dataStream} and saves it in the logs. */
        @Override
        public void testLog(String dataName, LogDataType dataType, InputStreamSource dataStream) {
            try (InputStream input = dataStream.createInputStream()) {
                mLogs.add(ByteString.readFrom(input));
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }

        List<ByteString> getLogs() {
            return new ArrayList<>(mLogs);
        }
    }

    /** Utility method to create .tar files. */
    private File createTar(Map<String, ByteString> fileContents) throws IOException {
        File tarFile = folder.newFile();
        try (TarArchiveOutputStream out =
                new TarArchiveOutputStream(new FileOutputStream(tarFile))) {
            for (Map.Entry<String, ByteString> file : fileContents.entrySet()) {
                TarArchiveEntry entry = new TarArchiveEntry(file.getKey());
                entry.setSize(file.getValue().size());

                out.putArchiveEntry(entry);
                file.getValue().writeTo(out);
                out.closeArchiveEntry();
            }
        }
        return tarFile;
    }

    private File createProfileToolZip() throws IOException {
        File profileToolZip = folder.newFile("llvm-profdata.zip");
        try (FileOutputStream stream = new FileOutputStream(profileToolZip);
                ZipOutputStream out = new ZipOutputStream(new BufferedOutputStream(stream))) {
            // Add bin/llvm-profdata.
            ZipEntry entry = new ZipEntry("bin/llvm-profdata");
            out.putNextEntry(entry);
            out.closeEntry();
        }
        return profileToolZip;
    }

    /** Utility function to verify that certain suffixes are contained in the List. */
    void checkListContainsSuffixes(List<String> list, List<String> suffixes) {
        for (String suffix : suffixes) {
            boolean found = false;
            for (String item : list) {
                if (item.endsWith(suffix)) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                fail("List " + list.toString() + " does not contain suffix '" + suffix + "'");
            }
        }
    }

    void checkListDoesNotContainSuffix(List<String> list, String suffix) {
        for (String item : list) {
            if (item.endsWith(suffix)) {
                fail("List " + list.toString() + " should not contain suffix '" + suffix + "'");
            }
        }
    }
}
