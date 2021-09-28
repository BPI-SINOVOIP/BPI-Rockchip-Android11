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

import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.IDeviceManager;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.io.FileNotFoundException;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.stream.Collectors;

import javax.annotation.Nullable;

/**
 * Target preparer which executes a script before running a test. The script can reference the
 * device's serial number using the ANDROID_SERIAL environment variable.
 */
@OptionClass(alias = "run-host-script")
public class RunHostScriptTargetPreparer extends BaseTargetPreparer {

    @Option(name = "script-file", description = "Path to the script to execute.")
    private String mScriptPath = null;

    @Option(name = "work-dir", description = "Working directory to use when executing script.")
    private File mWorkDir = null;

    @Option(name = "script-timeout", description = "Script execution timeout.")
    private Duration mTimeout = Duration.ofMinutes(1L);

    private IRunUtil mRunUtil;

    @Override
    public final void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        if (mScriptPath == null) {
            CLog.w("No script to execute.");
            return;
        }
        ITestDevice device = testInfo.getDevice();

        // Find script and verify it exists and is executable
        File scriptFile = findScriptFile(testInfo);
        if (scriptFile == null || !scriptFile.isFile()) {
            throw new TargetSetupError(
                    String.format("File %s not found.", mScriptPath), device.getDeviceDescriptor());
        }
        if (!scriptFile.canExecute()) {
            scriptFile.setExecutable(true);
        }

        // Set working directory and environment variables
        getRunUtil().setWorkingDir(mWorkDir);
        getRunUtil().setEnvVariable("ANDROID_SERIAL", device.getSerialNumber());
        setPathVariable(testInfo);

        // Execute script and handle result
        CommandResult result =
                getRunUtil().runTimedCmd(mTimeout.toMillis(), scriptFile.getAbsolutePath());
        switch (result.getStatus()) {
            case SUCCESS:
                CLog.i("Script executed successfully, stdout = [%s].", result.getStdout());
                break;
            case FAILED:
                throw new TargetSetupError(
                        String.format(
                                "Script execution failed, stdout = [%s], stderr = [%s].",
                                result.getStdout(), result.getStderr()),
                        device.getDeviceDescriptor());
            case TIMED_OUT:
                throw new TargetSetupError(
                        "Script execution timed out.", device.getDeviceDescriptor());
            case EXCEPTION:
                throw new TargetSetupError(
                        String.format(
                                "Exception during script execution, stdout = [%s], stderr = [%s].",
                                result.getStdout(), result.getStderr()),
                        device.getDeviceDescriptor());
        }
    }

    /** @return {@link IRunUtil} instance to use */
    @VisibleForTesting
    IRunUtil getRunUtil() {
        if (mRunUtil == null) {
            mRunUtil = new RunUtil();
        }
        return mRunUtil;
    }

    /** @return {@link IDeviceManager} instance used to fetch the configured adb/fastboot paths */
    @VisibleForTesting
    IDeviceManager getDeviceManager() {
        return GlobalConfiguration.getDeviceManagerInstance();
    }

    /** Find the script file to execute. */
    @Nullable
    private File findScriptFile(TestInformation testInfo) {
        File scriptFile = new File(mScriptPath);
        if (scriptFile.isAbsolute()) {
            return scriptFile;
        }
        // Try to find the script in the working directory if it was set
        if (mWorkDir != null) {
            scriptFile = new File(mWorkDir, mScriptPath);
            if (scriptFile.isFile()) {
                return scriptFile;
            }
        }
        // Otherwise, search for it in the test information
        try {
            return testInfo.getDependencyFile(mScriptPath, false);
        } catch (FileNotFoundException e) {
            return null;
        }
    }

    /** Update $PATH if necessary to use consistent adb and fastboot binaries. */
    private void setPathVariable(TestInformation testInfo) {
        List<String> paths = new ArrayList<>();
        // Use the test's adb binary or the globally defined one
        File adbBinary = testInfo.executionFiles().get(FilesKey.ADB_BINARY);
        if (adbBinary == null) {
            String adbPath = getDeviceManager().getAdbPath();
            if (!adbPath.equals("adb")) { // ignore default binary
                adbBinary = new File(adbPath);
            }
        }
        if (adbBinary != null && adbBinary.isFile()) {
            paths.add(adbBinary.getParentFile().getAbsolutePath());
        }
        // Use the globally defined fastboot binary
        String fastbootPath = getDeviceManager().getFastbootPath();
        if (!fastbootPath.equals("fastboot")) { // ignore default binary
            File fastbootBinary = new File(fastbootPath);
            if (fastbootBinary.isFile()) {
                paths.add(fastbootBinary.getParentFile().getAbsolutePath());
            }
        }
        // Prepend additional paths to the PATH variable
        if (!paths.isEmpty()) {
            String separator = System.getProperty("path.separator");
            paths.add(System.getenv("PATH"));
            String path = paths.stream().distinct().collect(Collectors.joining(separator));
            CLog.d("Using updated $PATH: %s", path);
            getRunUtil().setEnvVariable("PATH", path);
        }
    }
}
