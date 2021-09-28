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
package com.android.tradefed.module;

import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.suite.module.IModuleController;
import com.android.tradefed.testtype.suite.module.BaseModuleController;

/**
 * Controller to skip the HAL adapter test if the targeting HAL is not available.
 * Only used for single-device testing or the primary device in multi-device
 * testing.
 */
public class VtsHalAdapterModuleController extends BaseModuleController {
    // Command to list the registered instance for the given hal@version.
    static final String LIST_HAL_CMD =
            "lshal -ti --neat 2>/dev/null | grep -e '^hwbinder' | awk '{print $2}' | grep %s";

    @Option(name = "hal-package-name",
            description =
                    "The package name of a target HAL required, e.g., android.hardware.foo@1.0")
    private String mPackageName = null;

    /**
     * {@inheritDoc}
     */
    @Override
    public RunStrategy shouldRun(IInvocationContext context) {
        ITestDevice device = context.getDevices().get(0);
        try {
            String out = device.executeShellCommand(String.format(LIST_HAL_CMD, mPackageName));
            if (out.isEmpty()) {
                CLog.w("The specific HAL service is not running. Skip test module.");
                return RunStrategy.SKIP_MODULE_TESTCASES;
            }
            return RunStrategy.RUN;
        } catch (DeviceNotAvailableException e) {
            CLog.e("Failed to list HAL service.");
            return RunStrategy.RUN;
        }
    }
}
