/*
 * Copyright (C) 2017 The Android Open Source Project
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

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.suite.checker.StatusCheckerResult.CheckStatus;
import com.android.tradefed.util.ProcessInfo;
import com.android.tradefed.util.StreamUtil;


/**
 * Check if the pid of system_server has changed from before and after a module run. A new pid would
 * mean a runtime restart occurred during the module run.
 */
public class SystemServerStatusChecker implements ISystemStatusChecker {

    @Option(
        name = "disable-recovery-reboot",
        description =
                "If status checker is detected down (no process), attempt to reboot the device."
    )
    private boolean mShouldRecover = true;

    private ProcessInfo mSystemServerProcess;
    private boolean mShouldSkip = false;

    /** {@inheritDoc} */
    @Override
    public StatusCheckerResult preExecutionCheck(ITestDevice device)
            throws DeviceNotAvailableException {
        if (mShouldSkip) {
            return new StatusCheckerResult(CheckStatus.SUCCESS);
        }
        // process info relies on a time function fixed after P
        if (device.getApiLevel() < 29) {
            mShouldSkip = true;
            CLog.d("Api level is under 29 skipping SystemServerStatusChecker");
            return new StatusCheckerResult(CheckStatus.SUCCESS);
        }
        mSystemServerProcess = device.getProcessByName("system_server");
        StatusCheckerResult result = new StatusCheckerResult(CheckStatus.SUCCESS);
        if (mSystemServerProcess == null) {
            if (mShouldRecover) {
                device.reboot();
            }
            String message = "No valid system_server process is found.";
            CLog.w(message);
            result.setStatus(CheckStatus.FAILED);
            result.setBugreportNeeded(true);
            result.setErrorMessage(message);
            return result;
        }
        return result;
    }

    /** {@inheritDoc} */
    @Override
    public StatusCheckerResult postExecutionCheck(ITestDevice device)
            throws DeviceNotAvailableException {
        if (mShouldSkip) {
            return new StatusCheckerResult(CheckStatus.SUCCESS);
        }
        if (mSystemServerProcess == null) {
            CLog.d(
                    "No valid system_server process was found in preExecutionCheck, "
                            + "skipping system_server postExecutionCheck.");
            return new StatusCheckerResult(CheckStatus.SUCCESS);
        }
        try {
            if (!device.deviceSoftRestarted(mSystemServerProcess)) {
                return new StatusCheckerResult(CheckStatus.SUCCESS);
            }
        } catch (RuntimeException e) {
            CLog.w(StreamUtil.getStackTrace(e));
            StatusCheckerResult result = new StatusCheckerResult(CheckStatus.FAILED);
            result.setBugreportNeeded(true);
            result.setErrorMessage(StreamUtil.getStackTrace(e));
            return result;
        }

        StatusCheckerResult result = new StatusCheckerResult(CheckStatus.FAILED);
        result.setBugreportNeeded(true);
        result.setErrorMessage("The system-server crashed during test execution");
        return result;
    }

    /** Returns the current time. */
    @VisibleForTesting
    protected long getCurrentTime() {
        return System.currentTimeMillis();
    }

}
