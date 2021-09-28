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
package com.android.tradefed.suite.checker;

import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.suite.checker.StatusCheckerResult.CheckStatus;

/** Status checker that ensures the status of Selinux. */
public class EnforcedSeLinuxChecker implements ISystemStatusChecker {

    private static final String ENFORCING_STRING = "enforcing";

    @Option(
        name = "expect-enforced",
        description = "Controls whether or not Selinux is enforced between modules."
    )
    private boolean mExpectedEnforced = false;

    /** {@inheritDoc} */
    @Override
    public StatusCheckerResult postExecutionCheck(ITestDevice device)
            throws DeviceNotAvailableException {
        StatusCheckerResult result = new StatusCheckerResult(CheckStatus.SUCCESS);
        if (mExpectedEnforced && !isEnforced(device)) {
            result.setStatus(CheckStatus.FAILED);
            result.setErrorMessage("Device is in permissive mode while enforced was expected.");
            setEnforced(device, true);
        } else if (!mExpectedEnforced && isEnforced(device)) {
            result.setStatus(CheckStatus.FAILED);
            result.setErrorMessage("Device is in enforced mode while permissive was expected.");
            setEnforced(device, false);
        }
        return result;
    }

    private boolean isEnforced(ITestDevice device) throws DeviceNotAvailableException {
        String result = device.executeShellCommand("getenforce");
        if (ENFORCING_STRING.equals(result.toLowerCase().trim())) {
            return true;
        }
        return false;
    }

    private void setEnforced(ITestDevice device, boolean enforced)
            throws DeviceNotAvailableException {
        String enforce = enforced ? "1" : "0";
        device.executeShellCommand("su root setenforce " + enforce);
    }
}
