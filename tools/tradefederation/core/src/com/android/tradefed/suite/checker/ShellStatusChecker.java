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
package com.android.tradefed.suite.checker;

import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.suite.checker.StatusCheckerResult.CheckStatus;

/**
 * Check if the shell status is as expected before and after a module run. Any changes may
 * unexpectedly affect test cases.
 *
 * <p>There is a command-line option to disable the checker entirely:
 *
 * <pre>--skip-system-status-check=com.android.tradefed.suite.checker.ShellStatusChecker
 * </pre>
 */
@OptionClass(alias = "shell-status-checker")
public class ShellStatusChecker implements ISystemStatusChecker {

    private static final String FAIL_STRING = " Reset failed.";

    /* Option for the expected root state at pre- and post-check. */
    @Option(
        name = "expect-root",
        description =
                "Controls whether the shell root status is expected to be "
                        + "root ('true') or non-root ('false'). "
                        + "The checker will warn and try to revert the state "
                        + "if not as expected at pre- or post-check."
    )
    private boolean mExpectedRoot = false;

    /** {@inheritDoc} */
    @Override
    public StatusCheckerResult preExecutionCheck(ITestDevice device)
            throws DeviceNotAvailableException {
        StatusCheckerResult result = new StatusCheckerResult(CheckStatus.SUCCESS);
        if (mExpectedRoot != device.isAdbRoot()) {
            String message =
                    "This module unexpectedly started in a "
                            + (mExpectedRoot ? "non-root" : "root")
                            + " shell. Leaked from earlier module?";
            result.setStatus(CheckStatus.FAILED);

            boolean reset;
            if (mExpectedRoot) {
                reset = device.enableAdbRoot();
            } else {
                reset = device.disableAdbRoot();
            }
            if (!reset) {
                message += FAIL_STRING;
            }
            CLog.w(message);
            result.setErrorMessage(message);
        }

        return result;
    }

    /** {@inheritDoc} */
    @Override
    public StatusCheckerResult postExecutionCheck(ITestDevice device)
            throws DeviceNotAvailableException {
        StatusCheckerResult result = new StatusCheckerResult(CheckStatus.SUCCESS);
        if (mExpectedRoot != device.isAdbRoot()) {
            String message =
                    "This module changed shell root status to "
                            + (mExpectedRoot ? "non-root" : "root")
                            + ". Leaked from a test case or setup?";
            result.setStatus(CheckStatus.FAILED);

            boolean reset;
            if (mExpectedRoot) {
                reset = device.enableAdbRoot();
            } else {
                reset = device.disableAdbRoot();
            }
            if (!reset) {
                message += FAIL_STRING;
            }
            CLog.w(message);
            result.setErrorMessage(message);
        }

        return result;
    }
}
