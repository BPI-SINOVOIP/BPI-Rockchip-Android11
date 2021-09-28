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

import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import java.io.IOException;
import java.util.concurrent.TimeUnit;

/**
 * Test runner for executable running on the target and parsing tesult of kernel test.
 */
@OptionClass(alias = "kernel-target-test")
public class KernelTargetTest extends ExecutableTargetTest {
    @Option(name = "ignore-binary-check", description = "Ignore the binary check in findBinary().")
    private boolean mIgnoreBinaryCheck = false;

    @Override
    public String findBinary(String binary) throws DeviceNotAvailableException {
        if (mIgnoreBinaryCheck)
            return binary;
        return super.findBinary(binary);
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
        int exitCodeTCONF = 32;
        if (result.getExitCode() == exitCodeTCONF) {
            listener.testIgnored(description);
        } else {
            super.checkCommandResult(result, listener, description);
        }
    }
}
