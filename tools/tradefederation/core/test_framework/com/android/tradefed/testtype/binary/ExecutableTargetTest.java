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
package com.android.tradefed.testtype.binary;

import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;

import java.io.IOException;
import java.util.concurrent.TimeUnit;

/**
 * Test runner for executable running on the target. The runner implements {@link IDeviceTest} since
 * the binary run on a device.
 */
@OptionClass(alias = "executable-target-test")
public class ExecutableTargetTest extends ExecutableBaseTest implements IDeviceTest {

    private ITestDevice mDevice = null;

    /** {@inheritDoc} */
    @Override
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    /** {@inheritDoc} */
    @Override
    public ITestDevice getDevice() {
        return mDevice;
    }

    @Override
    public String findBinary(String binary) throws DeviceNotAvailableException {
        for (String path : binary.split(" ")) {
            if (mDevice.isExecutable(path)) return binary;
        }
        return null;
    }

    @Override
    public void runBinary(
            String binaryPath, ITestInvocationListener listener, TestDescription description)
            throws DeviceNotAvailableException, IOException {
        if (mDevice == null) {
            throw new IllegalArgumentException("Device has not been set");
        }
        CommandResult result =
                mDevice.executeShellV2Command(
                        binaryPath, getTimeoutPerBinaryMs(), TimeUnit.MILLISECONDS);
        checkCommandResult(result, listener, description);
    }

    /**
     * Check the result of the test command.
     *
     * @param result test result of the command {@link CommandResult}
     * @param listener the {@link ITestInvocationListener}
     * @param description The test in progress.
     */
    protected void checkCommandResult(
            CommandResult result, ITestInvocationListener listener, TestDescription description) {
        if (!CommandStatus.SUCCESS.equals(result.getStatus())) {
            String error_message;
            error_message =
                    String.format(
                            "binary returned non-zero. Exit code: %d, stderr: %s, stdout: %s",
                            result.getExitCode(), result.getStderr(), result.getStdout());
            listener.testFailed(
                    description,
                    FailureDescription.create(error_message)
                            .setFailureStatus(FailureStatus.TEST_FAILURE));
        }
    }
}
