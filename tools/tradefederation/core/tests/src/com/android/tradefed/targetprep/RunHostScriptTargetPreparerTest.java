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

package com.android.tradefed.targetprep;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.IDeviceManager;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Answers;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

/** Unit test for {@link RunHostScriptTargetPreparer}. */
@RunWith(JUnit4.class)
public final class RunHostScriptTargetPreparerTest {

    private static final String DEVICE_SERIAL = "DEVICE_SERIAL";

    @Rule public final MockitoRule mockito = MockitoJUnit.rule();

    @Mock(answer = Answers.RETURNS_DEEP_STUBS)
    private TestInformation mTestInfo;

    @Mock private IRunUtil mRunUtil;
    @Mock private IDeviceManager mDeviceManager;
    private RunHostScriptTargetPreparer mPreparer;
    private OptionSetter mOptionSetter;
    private File mWorkDir;
    private File mScriptFile;

    @Before
    public void setUp() throws IOException, ConfigurationException {
        // Initialize preparer and test information
        mPreparer =
                new RunHostScriptTargetPreparer() {
                    @Override
                    IRunUtil getRunUtil() {
                        return mRunUtil;
                    }

                    @Override
                    IDeviceManager getDeviceManager() {
                        return mDeviceManager;
                    }
                };
        mOptionSetter = new OptionSetter(mPreparer);
        when(mTestInfo.getDevice().getSerialNumber()).thenReturn(DEVICE_SERIAL);
        when(mTestInfo.executionFiles().get(any(FilesKey.class))).thenReturn(null);

        // Create temporary working directory and script file
        mWorkDir = FileUtil.createTempDir(this.getClass().getSimpleName());
        mScriptFile = File.createTempFile("script", ".sh", mWorkDir);

        // Default to default adb/fastboot paths
        when(mDeviceManager.getAdbPath()).thenReturn("adb");
        when(mDeviceManager.getFastbootPath()).thenReturn("fastboot");

        // Default to successful execution
        CommandResult result = new CommandResult(CommandStatus.SUCCESS);
        when(mRunUtil.runTimedCmd(anyLong(), any())).thenReturn(result);
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(mWorkDir);
    }

    @Test
    public void testSetUp() throws Exception {
        mOptionSetter.setOptionValue("script-file", mScriptFile.getAbsolutePath());
        mOptionSetter.setOptionValue("script-timeout", "10");
        // Verify environment, timeout, and script path
        mPreparer.setUp(mTestInfo);
        verify(mRunUtil).setEnvVariable("ANDROID_SERIAL", DEVICE_SERIAL);
        verify(mRunUtil, never()).setEnvVariable(eq("PATH"), any()); // uses default PATH
        verify(mRunUtil).runTimedCmd(10L, mScriptFile.getAbsolutePath());
        // Verify that script is executable
        assertTrue(mScriptFile.canExecute());
    }

    @Test
    public void testSetUp_workingDir() throws Exception {
        mOptionSetter.setOptionValue("work-dir", mWorkDir.getAbsolutePath());
        mOptionSetter.setOptionValue("script-file", mScriptFile.getName()); // relative
        // Verify that the working directory is set and script's path is resolved
        mPreparer.setUp(mTestInfo);
        verify(mRunUtil).setWorkingDir(mWorkDir);
        verify(mRunUtil).runTimedCmd(anyLong(), eq(mScriptFile.getAbsolutePath()));
    }

    @Test
    public void testSetUp_findFile() throws Exception {
        mOptionSetter.setOptionValue("script-file", mScriptFile.getName()); // relative
        when(mTestInfo.getDependencyFile(any(), anyBoolean())).thenReturn(mScriptFile);
        // Verify that the script is found in the test information
        mPreparer.setUp(mTestInfo);
        verify(mRunUtil).runTimedCmd(anyLong(), eq(mScriptFile.getAbsolutePath()));
    }

    @Test(expected = TargetSetupError.class)
    public void testSetUp_fileNotFound() throws Exception {
        mOptionSetter.setOptionValue("script-file", "unknown.sh");
        mPreparer.setUp(mTestInfo);
    }

    @Test(expected = TargetSetupError.class)
    public void testSetUp_executionError() throws Exception {
        mOptionSetter.setOptionValue("script-file", mScriptFile.getAbsolutePath());
        CommandResult result = new CommandResult(CommandStatus.FAILED);
        when(mRunUtil.runTimedCmd(anyLong(), any())).thenReturn(result);
        mPreparer.setUp(mTestInfo);
    }

    @Test
    public void testSetUp_pathVariable() throws Exception {
        mOptionSetter.setOptionValue("script-file", mScriptFile.getAbsolutePath());
        // Create and set dummy adb binary
        Path adbDir = Files.createTempDirectory(mWorkDir.toPath(), "adb");
        File adbBinary = File.createTempFile("adb", ".sh", adbDir.toFile());
        when(mTestInfo.executionFiles().get(eq(FilesKey.ADB_BINARY))).thenReturn(adbBinary);
        // Create and set dummy fastboot binary
        Path fastbootDir = Files.createTempDirectory(mWorkDir.toPath(), "fastboot");
        File fastbootBinary = File.createTempFile("fastboot", ".sh", fastbootDir.toFile());
        when(mDeviceManager.getFastbootPath()).thenReturn(fastbootBinary.getAbsolutePath());
        // Verify that binary paths were prepended to the path variable
        String separator = System.getProperty("path.separator");
        String expectedPath = adbDir + separator + fastbootDir + separator + System.getenv("PATH");
        mPreparer.setUp(mTestInfo);
        verify(mRunUtil).setEnvVariable("PATH", expectedPath);
    }
}
