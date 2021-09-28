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

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.build.BuildInfoKey.BuildInfoFileKey;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.error.DeviceErrorIdentifier;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Test runner for executable running on the host. The runner implements {@link IDeviceTest} since
 * the host binary might communicate to a device. If the received device is not a {@link StubDevice}
 * the serial will be passed to the binary to be used.
 */
@OptionClass(alias = "executable-host-test")
public class ExecutableHostTest extends ExecutableBaseTest {

    private static final String ANDROID_SERIAL = "ANDROID_SERIAL";
    private static final String LOG_STDOUT_TAG = "-binary-stdout-";
    private static final String LOG_STDERR_TAG = "-binary-stderr-";

    @Option(
        name = "relative-path-execution",
        description =
                "Some scripts assume a relative location to their tests file, this allows to"
                        + " execute with that relative location."
    )
    private boolean mExecuteRelativeToScript = false;

    @Override
    public String findBinary(String binary) {
        File bin = new File(binary);
        // If it's a local path or absolute path
        if (bin.exists()) {
            return bin.getAbsolutePath();
        }
        if (getTestInfo().getBuildInfo() instanceof IDeviceBuildInfo) {
            IDeviceBuildInfo deviceBuild = (IDeviceBuildInfo) getTestInfo().getBuildInfo();
            File testsDir = deviceBuild.getTestsDir();

            List<File> scanDirs = new ArrayList<>();
            // If it exists, always look first in the ANDROID_HOST_OUT_TESTCASES
            File targetTestCases = deviceBuild.getFile(BuildInfoFileKey.HOST_LINKED_DIR);
            if (targetTestCases != null) {
                scanDirs.add(targetTestCases);
            }
            if (testsDir != null) {
                scanDirs.add(testsDir);
            }

            try {
                // Search the full tests dir if no target dir is available.
                File src = FileUtil.findFile(binary, getAbi(), scanDirs.toArray(new File[] {}));
                if (src != null) {
                    return src.getAbsolutePath();
                }
            } catch (IOException e) {
                CLog.e("Failed to find test files from directory.");
            }
        }
        return null;
    }

    @Override
    public void runBinary(
            String binaryPath, ITestInvocationListener listener, TestDescription description)
            throws DeviceNotAvailableException, IOException {
        IRunUtil runUtil = createRunUtil();
        // Output everything in stdout
        runUtil.setRedirectStderrToStdout(true);
        // If we are running against a real device, set ANDROID_SERIAL to the proper serial.
        if (!(getTestInfo().getDevice().getIDevice() instanceof StubDevice)) {
            runUtil.setEnvVariable(ANDROID_SERIAL, getTestInfo().getDevice().getSerialNumber());
        }
        // Ensure its executable
        FileUtil.chmodRWXRecursively(new File(binaryPath));

        List<String> command = new ArrayList<>();
        String scriptName = new File(binaryPath).getName();
        if (mExecuteRelativeToScript) {
            String parentDir = new File(binaryPath).getParent();
            command.add("bash");
            command.add("-c");
            command.add(String.format("pushd %s; ./%s;", parentDir, scriptName));
        } else {
            command.add(binaryPath);
        }
        File stdout = FileUtil.createTempFile(scriptName + LOG_STDOUT_TAG, ".txt");
        File stderr = FileUtil.createTempFile(scriptName + LOG_STDERR_TAG, ".txt");

        try (FileOutputStream stdoutStream = new FileOutputStream(stdout);
                FileOutputStream stderrStream = new FileOutputStream(stderr); ) {
            CommandResult res =
                    runUtil.runTimedCmd(
                            getTimeoutPerBinaryMs(),
                            stdoutStream,
                            stderrStream,
                            command.toArray(new String[0]));
            if (!CommandStatus.SUCCESS.equals(res.getStatus())) {
                FailureStatus status = FailureStatus.TEST_FAILURE;
                // Everything should be outputted in stdout with our redirect above.
                String errorMessage = FileUtil.readStringFromFile(stdout);
                if (CommandStatus.TIMED_OUT.equals(res.getStatus())) {
                    errorMessage += "\nTimeout.";
                    status = FailureStatus.TIMED_OUT;
                }
                if (res.getExitCode() != null) {
                    errorMessage += String.format("\nExit Code: %s", res.getExitCode());
                }
                listener.testFailed(
                        description,
                        FailureDescription.create(errorMessage).setFailureStatus(status));
            }
        } finally {
            logFile(stdout, listener);
            logFile(stderr, listener);
        }

        if (!(getTestInfo().getDevice().getIDevice() instanceof StubDevice)) {
            // Ensure that the binary did not leave the device offline.
            CLog.d("Checking whether device is still online after %s", binaryPath);
            try {
                getTestInfo().getDevice().waitForDeviceAvailable();
            } catch (DeviceNotAvailableException e) {
                FailureDescription failure =
                        FailureDescription.create(
                                        String.format(
                                                "Device became unavailable after %s.", binaryPath),
                                        FailureStatus.LOST_SYSTEM_UNDER_TEST)
                                .setErrorIdentifier(DeviceErrorIdentifier.DEVICE_UNAVAILABLE)
                                .setCause(e);
                listener.testRunFailed(failure);
                throw e;
            }
        }
    }

    @VisibleForTesting
    IRunUtil createRunUtil() {
        return new RunUtil();
    }

    private void logFile(File logFile, ITestLogger logger) {
        try (FileInputStreamSource source = new FileInputStreamSource(logFile, true)) {
            logger.testLog(logFile.getName(), LogDataType.TEXT, source);
        }
    }
}
