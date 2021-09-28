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
package com.android.tradefed.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.build.IFolderBuildInfo;
import com.android.tradefed.log.LogUtil.CLog;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.IOException;

/**
 * Unit tests for {@link VtsPythonRunnerHelper}.
 */
@RunWith(JUnit4.class)
public class VtsPythonRunnerHelperTest {
    private static final String[] mPythonCmd = {"python"};
    private static final long mTestTimeout = 1000 * 5;

    private VtsPythonRunnerHelper mVtsPythonRunnerHelper = null;
    private String mVirtualenvPath = "virtualenv_path_" + System.currentTimeMillis();

    @Before
    public void setUp() throws Exception {
        IFolderBuildInfo buildInfo = EasyMock.createNiceMock(IFolderBuildInfo.class);
        EasyMock.replay(buildInfo);
    }

    /**
     * Create a mock runUtil with returns the expected results.
     */
    private IRunUtil createMockRunUtil() {
        IRunUtil runUtil = new RunUtil() {
            private String path = null;

            @Override
            public void setEnvVariable(String key, String value) {
                super.setEnvVariable(key, value);
                if (key.equals("PATH")) {
                    path = value;
                }
            }

            @Override
            public CommandResult runTimedCmd(final long timeout, final String... command) {
                CommandResult cmdRes = new CommandResult(CommandStatus.SUCCESS);
                String out = "";
                if (command.length == 2 && command[0].equals("which")
                        && command[1].equals("python")) {
                    if (path != null) {
                        out = path.split(":")[0] + "/python";
                    } else {
                        out = "/usr/bin/python";
                    }
                }
                cmdRes.setStdout(out);
                return cmdRes;
            }
        };
        return runUtil;
    }

    /**
     * Create a mock runUtil with returns the expected command status.
     */
    private IRunUtil createMockRunUtil(CommandStatus status) {
        IRunUtil runUtil = EasyMock.createMock(IRunUtil.class);
        EasyMock.expect(runUtil.runTimedCmd(EasyMock.anyLong(), EasyMock.anyObject(),
                                EasyMock.anyObject(), EasyMock.anyObject()))
                .andReturn(new CommandResult(status));
        runUtil.setWorkingDir(null);
        return runUtil;
    }

    @Test
    public void testProcessRunSuccess() {
        IRunUtil runUtil = createMockRunUtil(CommandStatus.SUCCESS);
        EasyMock.replay(runUtil);
        mVtsPythonRunnerHelper =
                new VtsPythonRunnerHelper(new File(mVirtualenvPath), null, runUtil);
        CommandResult commandResult = new CommandResult();
        String interruptMessage =
                mVtsPythonRunnerHelper.runPythonRunner(mPythonCmd, commandResult, mTestTimeout);
        assertEquals(interruptMessage, null);
        assertEquals(commandResult.getStatus(), CommandStatus.SUCCESS);
        EasyMock.verify(runUtil);
    }

    @Test
    public void testProcessRunFailed() {
        IRunUtil runUtil = createMockRunUtil(CommandStatus.FAILED);
        EasyMock.replay(runUtil);
        mVtsPythonRunnerHelper =
                new VtsPythonRunnerHelper(new File(mVirtualenvPath), null, runUtil);
        CommandResult commandResult = new CommandResult();
        String interruptMessage =
                mVtsPythonRunnerHelper.runPythonRunner(mPythonCmd, commandResult, mTestTimeout);
        assertEquals(interruptMessage, null);
        assertEquals(commandResult.getStatus(), CommandStatus.FAILED);
        EasyMock.verify(runUtil);
    }

    @Test
    public void testProcessRunTimeout() {
        IRunUtil runUtil = createMockRunUtil(CommandStatus.TIMED_OUT);
        EasyMock.replay(runUtil);
        mVtsPythonRunnerHelper =
                new VtsPythonRunnerHelper(new File(mVirtualenvPath), null, runUtil);
        CommandResult commandResult = new CommandResult();
        String interruptMessage =
                mVtsPythonRunnerHelper.runPythonRunner(mPythonCmd, commandResult, mTestTimeout);
        assertEquals(interruptMessage, null);
        assertEquals(commandResult.getStatus(), CommandStatus.TIMED_OUT);
        EasyMock.verify(runUtil);
    }

    @Test
    public void testProcessRunInterrupted() {
        IRunUtil runUtil = EasyMock.createMock(IRunUtil.class);
        EasyMock.expect(runUtil.runTimedCmd(EasyMock.anyLong(), EasyMock.anyObject(),
                                EasyMock.anyObject(), EasyMock.anyObject()))
                .andThrow(new RunInterruptedException("abort"));
        runUtil.setWorkingDir(null);
        EasyMock.replay(runUtil);
        mVtsPythonRunnerHelper =
                new VtsPythonRunnerHelper(new File(mVirtualenvPath), null, runUtil);
        CommandResult commandResult = new CommandResult();
        String interruptMessage =
                mVtsPythonRunnerHelper.runPythonRunner(mPythonCmd, commandResult, mTestTimeout);
        assertNotEquals(interruptMessage, null);
        assertEquals(commandResult.getStatus(), CommandStatus.TIMED_OUT);
        EasyMock.verify(runUtil);
    }

    @Test
    public void testActivateVirtualEnvNotExist() {
        IRunUtil runUtil = createMockRunUtil();
        assertEquals(null, VtsPythonRunnerHelper.getPythonBinDir(mVirtualenvPath));
        VtsPythonRunnerHelper.activateVirtualenv(runUtil, mVirtualenvPath);
        String pythonBinary = runUtil.runTimedCmd(1000, "which", "python").getStdout();
        assertEquals(pythonBinary, "/usr/bin/python");
    }

    @Test
    public void testActivateVirtualEnvExist() {
        IRunUtil runUtil = createMockRunUtil();
        String binDirName = EnvUtil.isOnWindows() ? "Scripts" : "bin";
        File envDir = new File(mVirtualenvPath);
        File binDir = new File(mVirtualenvPath, binDirName);
        try {
            CLog.d("%s", envDir.mkdir());
            CLog.d("%s", binDir.mkdir());
            assertTrue(binDir.exists());
            assertEquals(binDir.getAbsolutePath(),
                    VtsPythonRunnerHelper.getPythonBinDir(mVirtualenvPath));
            VtsPythonRunnerHelper.activateVirtualenv(runUtil, mVirtualenvPath);
            String pythonBinary = runUtil.runTimedCmd(1000, "which", "python").getStdout();
            assertEquals(pythonBinary, new File(binDir, "python").getAbsolutePath());
        } finally {
            FileUtil.recursiveDelete(envDir);
            FileUtil.recursiveDelete(binDir);
        }
    }

}
