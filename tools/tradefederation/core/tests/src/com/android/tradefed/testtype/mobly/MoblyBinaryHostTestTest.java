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

package com.android.tradefed.testtype.mobly;

import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.CoreMatchers.hasItem;
import static org.hamcrest.CoreMatchers.hasItems;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.core.StringStartsWith.startsWith;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.android.tradefed.build.DeviceBuildInfo;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.utils.FileUtils;

import com.google.common.base.Throwables;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.InputStream;
import java.io.StringWriter;
import java.io.Writer;
import java.util.Arrays;
import java.util.List;

@RunWith(JUnit4.class)
public class MoblyBinaryHostTestTest {
    private static final String BINARY_PATH = "/binary/file/path/test.par";
    private static final String LOG_PATH = "/log/dir/abs/path";
    private static final String DEVICE_SERIAL = "X123SER";
    private static final long DEFAULT_TIME_OUT = 30 * 1000L;
    private static final String TEST_RESULT_FILE_NAME = "test_summary.yaml";

    private MoblyBinaryHostTest mSpyTest;
    private ITestDevice mMockDevice;
    private IRunUtil mMockRunUtil;
    private MoblyYamlResultParser mMockParser;
    private InputStream mMockSummaryInputStream;
    private File mMoblyTestDir;
    private File mMoblyBinary; // used by python-binaries option
    private File mMoblyBinary2; // used by par-file-name option
    private DeviceBuildInfo mMockBuildInfo;

    @Before
    public void setUp() throws Exception {
        mSpyTest = Mockito.spy(new MoblyBinaryHostTest());
        mMockDevice = Mockito.mock(ITestDevice.class);
        mMockRunUtil = Mockito.mock(IRunUtil.class);
        mMockBuildInfo = Mockito.mock(DeviceBuildInfo.class);
        mSpyTest.setDevice(mMockDevice);
        Mockito.doReturn(mMockRunUtil).when(mSpyTest).getRunUtil();
        Mockito.doReturn(DEFAULT_TIME_OUT).when(mSpyTest).getTestTimeout();
        Mockito.doReturn("not_adb").when(mSpyTest).getAdbPath();
        mMoblyTestDir = FileUtil.createTempDir("mobly_tests");
        mMoblyBinary = FileUtil.createTempFile("mobly_binary", ".par", mMoblyTestDir);
        mMoblyBinary2 = FileUtil.createTempFile("mobly_binary_2", ".par", mMoblyTestDir);
        mSpyTest.setBuild(mMockBuildInfo);
    }

    @After
    public void tearDown() throws Exception {
        FileUtil.recursiveDelete(mMoblyTestDir);
    }

    @Test
    public void testRun_withPythonBinariesOption() throws Exception {
        OptionSetter setter = new OptionSetter(mSpyTest);
        setter.setOptionValue("python-binaries", mMoblyBinary.getAbsolutePath());
        File testResult = new File(mSpyTest.getLogDirAbsolutePath(), TEST_RESULT_FILE_NAME);
        // Mimics the behavior of a successful test run.
        Mockito.when(mMockRunUtil.runTimedCmd(anyLong(), any()))
                .thenAnswer(
                        new Answer<CommandResult>() {
                            @Override
                            public CommandResult answer(InvocationOnMock invocation)
                                    throws Throwable {
                                FileUtils.createFile(testResult, "");
                                FileUtils.createFile(
                                        new File(mSpyTest.getLogDirAbsolutePath(), "log"),
                                        "log content");
                                return new CommandResult(CommandStatus.SUCCESS);
                            }
                        });

        mSpyTest.run(Mockito.mock(ITestInvocationListener.class));

        verify(mSpyTest.getRunUtil()).runTimedCmd(anyLong(), any());
        assertFalse(new File(mSpyTest.getLogDirAbsolutePath()).exists());
    }

    @Test
    public void testRun_withPythonBinariesOption_binaryNotFound() throws Exception {
        OptionSetter setter = new OptionSetter(mSpyTest);
        setter.setOptionValue("python-binaries", mMoblyBinary.getAbsolutePath());
        FileUtil.deleteFile(mMoblyBinary);

        mSpyTest.run(Mockito.mock(ITestInvocationListener.class));

        verify(mSpyTest, never()).reportLogs(any(), any());
    }

    @Test
    public void testRun_withParFileNameOption() throws Exception {
        OptionSetter setter = new OptionSetter(mSpyTest);
        setter.setOptionValue("par-file-name", mMoblyBinary2.getName());
        Mockito.doReturn(mMoblyTestDir).when(mMockBuildInfo).getTestsDir();
        File testResult = new File(mSpyTest.getLogDirAbsolutePath(), TEST_RESULT_FILE_NAME);
        Mockito.when(mMockRunUtil.runTimedCmd(anyLong(), any()))
                .thenAnswer(
                        new Answer<CommandResult>() {
                            @Override
                            public CommandResult answer(InvocationOnMock invocation)
                                    throws Throwable {
                                FileUtils.createFile(testResult, "");
                                FileUtils.createFile(
                                        new File(mSpyTest.getLogDirAbsolutePath(), "log"),
                                        "log content");
                                return new CommandResult(CommandStatus.SUCCESS);
                            }
                        });

        mSpyTest.run(Mockito.mock(ITestInvocationListener.class));

        verify(mSpyTest.getRunUtil()).runTimedCmd(anyLong(), any());
        assertFalse(new File(mSpyTest.getLogDirAbsolutePath()).exists());
    }

    @Test
    public void testRun_withParFileNameOption_binaryNotFound() throws Exception {
        OptionSetter setter = new OptionSetter(mSpyTest);
        setter.setOptionValue("par-file-name", mMoblyBinary2.getName());
        Mockito.doReturn(mMoblyTestDir).when(mMockBuildInfo).getTestsDir();
        FileUtil.deleteFile(mMoblyBinary2);

        try {
            mSpyTest.run(Mockito.mock(ITestInvocationListener.class));
            fail("Should have thrown an exception");
        } catch (RuntimeException e) {
            verify(mSpyTest, never()).reportLogs(any(), any());
            assertEquals(
                    String.format(
                            "An unexpected exception was thrown, full stack trace: %s",
                            Throwables.getStackTraceAsString(e)),
                    e.getMessage(),
                    String.format("Couldn't find a par file %s", mMoblyBinary2.getName()));
        }
    }

    @Test
    public void testRun_testResultIsMissing() throws Exception {
        OptionSetter setter = new OptionSetter(mSpyTest);
        setter.setOptionValue("python-binaries", mMoblyBinary.getAbsolutePath());
        // Test result and log files were not created for some reasons during test run.
        Mockito.when(mMockRunUtil.runTimedCmd(anyLong(), any()))
                .thenAnswer(
                        new Answer<CommandResult>() {
                            @Override
                            public CommandResult answer(InvocationOnMock invocation)
                                    throws Throwable {
                                return new CommandResult(CommandStatus.SUCCESS);
                            }
                        });

        try {
            mSpyTest.run(Mockito.mock(ITestInvocationListener.class));
            fail("Should have thrown an exception");
        } catch (RuntimeException e) {
            assertThat(
                    String.format(
                            "An unexpected exception was thrown, full stack trace: %s",
                            Throwables.getStackTraceAsString(e)),
                    e.getMessage(),
                    containsString(
                            "Fail to find test summary file test_summary.yaml under directory"));
            assertFalse(new File(mSpyTest.getLogDirAbsolutePath()).exists());
        }
    }

    @Test
    public void testBuildCommandLineArrayWithOutConfig() throws Exception {
        Mockito.doNothing().when(mSpyTest).reportLogs(any(), any());
        Mockito.doReturn(DEVICE_SERIAL).when(mMockDevice).getSerialNumber();
        Mockito.doReturn(LOG_PATH).when(mSpyTest).getLogDirAbsolutePath();
        List<String> expOptions = Arrays.asList("--option1", "--option2=test_option");
        Mockito.doReturn(expOptions).when(mSpyTest).getTestOptions();
        String[] cmdArray = mSpyTest.buildCommandLineArray(BINARY_PATH);
        assertThat(cmdArray[0], is(BINARY_PATH));
        assertThat(cmdArray[1], is("--"));
        assertThat(Arrays.asList(cmdArray), not(hasItem(startsWith("--config"))));
        assertThat(
                Arrays.asList(cmdArray),
                hasItems(
                        "--device_serial=" + DEVICE_SERIAL,
                        "--log_path=" + LOG_PATH,
                        "--option1",
                        "--option2=test_option"));
    }

    @Test
    public void testBuildCommandLineArrayWithConfig() throws Exception {
        Mockito.doNothing().when(mSpyTest).reportLogs(any(), any());
        Mockito.doReturn(DEVICE_SERIAL).when(mMockDevice).getSerialNumber();
        Mockito.doReturn(LOG_PATH).when(mSpyTest).getLogDirAbsolutePath();
        List<String> expOptions = Arrays.asList("--option1", "--option2=test_option");
        Mockito.doReturn(expOptions).when(mSpyTest).getTestOptions();
        String configFilePath = "/test/config/file/path.yaml";
        Mockito.doReturn(configFilePath).when(mSpyTest).getConfigPath();
        String[] cmdArray = mSpyTest.buildCommandLineArray(BINARY_PATH);
        assertThat(cmdArray[0], is(BINARY_PATH));
        assertThat(cmdArray[1], is("--"));
        assertThat(
                Arrays.asList(cmdArray),
                hasItems(
                        "--device_serial=" + DEVICE_SERIAL,
                        "--log_path=" + LOG_PATH,
                        "--config=" + configFilePath,
                        "--option1",
                        "--option2=test_option"));
    }

    @Test
    public void testProcessYamlTestResultsSuccess() throws Exception {
        Mockito.doNothing().when(mSpyTest).reportLogs(any(), any());
        mMockSummaryInputStream = Mockito.mock(InputStream.class);
        mMockParser = Mockito.mock(MoblyYamlResultParser.class);
        mSpyTest.processYamlTestResults(mMockSummaryInputStream, mMockParser);
        verify(mMockParser, times(1)).parse(mMockSummaryInputStream);
    }

    @Test
    public void testUpdateConfigFile() throws Exception {
        Mockito.doNothing().when(mSpyTest).reportLogs(any(), any());
        Mockito.doReturn("testBedName").when(mSpyTest).getTestBed();
        String configString =
                new StringBuilder()
                        .append("TestBeds:")
                        .append("\n")
                        .append("- TestParams:")
                        .append("\n")
                        .append("    dut_name: is_dut")
                        .append("\n")
                        .append("  Name: testBedName")
                        .append("\n")
                        .append("  Controllers:")
                        .append("\n")
                        .append("    AndroidDevice:")
                        .append("\n")
                        .append("    - dimensions: {mobile_type: 'dut_rear'}")
                        .append("\n")
                        .append("      serial: old123")
                        .append("\n")
                        .append("MoblyParams: {{LogPath: {log_path}}}")
                        .append("\n")
                        .toString();
        InputStream inputStream = new ByteArrayInputStream(configString.getBytes());
        Writer writer = new StringWriter();
        mSpyTest.updateConfigFile(inputStream, writer, DEVICE_SERIAL);
        String updatedConfigString = writer.toString();
        LogUtil.CLog.d("Updated config string: %s", updatedConfigString);
        // Check if serial injected.
        assertThat(updatedConfigString, containsString(DEVICE_SERIAL));
        // Check if original still exists.
        assertThat(updatedConfigString, containsString("mobile_type"));
    }
}
