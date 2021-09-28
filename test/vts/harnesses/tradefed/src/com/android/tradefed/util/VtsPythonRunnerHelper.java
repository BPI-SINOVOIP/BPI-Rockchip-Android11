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

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.targetprep.VtsPythonVirtualenvPreparer;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.OutputStream;

/**
 * A helper class for executing VTS python scripts.
 */
public class VtsPythonRunnerHelper {
    // The timeout for the runner's teardown prodedure.
    public static final long TEST_ABORT_TIMEOUT_MSECS = 1000 * 40;

    static final String PATH = "PATH";
    static final String PYTHONHOME = "PYTHONHOME";
    static final String VTS = "vts";

    // Python virtual environment root path
    private File mVirtualenvPath;
    protected IRunUtil mRunUtil;

    public VtsPythonRunnerHelper(IBuildInfo buildInfo, File workingDir) {
        this(buildInfo.getBuildAttributes().get(VtsPythonVirtualenvPreparer.VIRTUAL_ENV),
                workingDir);
    }

    public VtsPythonRunnerHelper(String virtualEnvPath, File workingDir) {
        this(virtualEnvPath == null ? null : new File(virtualEnvPath), workingDir);
    }

    public VtsPythonRunnerHelper(File virtualEnvPath, File workingDir) {
        this(virtualEnvPath, workingDir, new RunUtil());
    }

    public VtsPythonRunnerHelper(File virtualEnvPath, File workingDir, IRunUtil runUtil) {
        mVirtualenvPath = virtualEnvPath;
        mRunUtil = runUtil;
        activateVirtualenv(mRunUtil, getPythonVirtualEnv());
        mRunUtil.setWorkingDir(workingDir);
    }

    /**
     * Run VTS Python runner and handle interrupt from TradeFed.
     *
     * @param cmd the command to start VTS Python runner.
     * @param commandResult the object containing the command result.
     * @param timeout command timeout value.
     * @return null if the command terminates or times out; a message string if the command is
     * interrupted by TradeFed.
     */
    public String runPythonRunner(String[] cmd, CommandResult commandResult, long timeout) {
        OutputStream stdOut = new ByteArrayOutputStream();
        OutputStream stdErr = new ByteArrayOutputStream();
        try {
            return runPythonRunner(cmd, commandResult, timeout, stdOut, stdErr);
        } finally {
            try {
                stdOut.close();
                stdErr.close();
                commandResult.setStdout(((ByteArrayOutputStream) stdOut).toString("UTF-8"));
                commandResult.setStderr(((ByteArrayOutputStream) stdErr).toString("UTF-8"));
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
    }

    /**
     * Run VTS Python runner and handle interrupt from TradeFed.
     *
     * @param cmd the command to start VTS Python runner.
     * @param commandResult the object containing the command result.
     * @param timeout command timeout value.
     * @return null if the command terminates or times out; a message string if the command is
     * interrupted by TradeFed.
     */
    public String runPythonRunner(String[] cmd, CommandResult commandResult, long timeout,
            OutputStream stdOut, OutputStream stdErr) {
        String interruptMessage;
        CommandStatus commandStatus;
        CommandResult result;
        try {
            result = mRunUtil.runTimedCmd(timeout, stdOut, stdErr, cmd);
            commandStatus = result.getStatus();
            interruptMessage = null;
        } catch (RunInterruptedException e) {
            CLog.e("Python process is interrupted.");
            commandStatus = CommandStatus.TIMED_OUT;
            interruptMessage = (e.getMessage() != null ? e.getMessage() : "");
        }
        commandResult.setStatus(commandStatus);
        return interruptMessage;
    }

    /**
     * Gets python bin directory path.
     *
     * This method will check the directory existence.
     *
     * @return python bin directory; null if not exist.
     */
    public static String getPythonBinDir(String virtualenvPath) {
        if (virtualenvPath == null) {
            return null;
        }
        String binDirName = EnvUtil.isOnWindows() ? "Scripts" : "bin";
        File res = new File(virtualenvPath, binDirName);
        if (!res.exists()) {
            return null;
        }
        return res.getAbsolutePath();
    }

    /**
     * Get python virtualenv path
     * @return virutalenv path. null if doesn't exist
     */
    public String getPythonVirtualEnv() {
        if (mVirtualenvPath == null) {
            return null;
        }
        return mVirtualenvPath.getAbsolutePath();
    }

    /**
     * Activate virtualenv for a RunUtil.
     *
     * This method will check for python bin directory existence
     *
     * @param runUtil
     * @param virtualenvPath
     */
    public static void activateVirtualenv(IRunUtil runUtil, String virtualenvPath) {
        String pythonBinDir = getPythonBinDir(virtualenvPath);
        if (pythonBinDir == null || !new File(pythonBinDir).exists()) {
            CLog.e("Invalid python virtualenv path. Using python from system path.");
        } else {
            String separater = EnvUtil.isOnWindows() ? ";" : ":";
            runUtil.setEnvVariable(PATH, pythonBinDir + separater + System.getenv().get(PATH));
            runUtil.setEnvVariable(VtsPythonVirtualenvPreparer.VIRTUAL_ENV, virtualenvPath);
            runUtil.unsetEnvVariable(PYTHONHOME);
        }
    }
}
