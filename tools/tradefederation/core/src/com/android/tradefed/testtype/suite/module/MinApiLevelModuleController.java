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
package com.android.tradefed.testtype.suite.module;

import com.android.tradefed.config.Option;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.log.LogUtil.CLog;

/** Base class for a module controller to not run tests when it below a specified API Level. */
public class MinApiLevelModuleController extends BaseModuleController {

    @Option(
            name = "api-level-prop",
            description = "Read the api level from which prop. e.g.'ro.product.first_api_level'")
    private String mApiLevelProp = "ro.build.version.sdk";

    @Option(name = "min-api-level", description = "The minimum api-level on which tests will run.")
    private Integer mMinApiLevel = 0;

    @Override
    public RunStrategy shouldRun(IInvocationContext context) {
        for (ITestDevice device : context.getDevices()) {
            if (device.getIDevice() instanceof StubDevice) {
                continue;
            }
            try {
                String apiLevelString = device.getProperty(mApiLevelProp);
                int apiLevel = -1;
                try {
                    if (apiLevelString != null) {
                        apiLevel = Integer.parseInt(apiLevelString);
                    } else {
                        CLog.d("Cannot get the API Level.");
                    }
                } catch (NumberFormatException e) {
                    CLog.d("Error parsing system property %s: %s", mApiLevelProp, e.getMessage());
                }
                if (apiLevel >= mMinApiLevel) {
                    continue;
                }
                CLog.d(
                        "Skipping module %s because API Level %d is < %d.",
                        getModuleName(), apiLevel, mMinApiLevel);
                return RunStrategy.FULL_MODULE_BYPASS;
            } catch (DeviceNotAvailableException e) {
                CLog.e("Couldn't check API Level on %s", device.getSerialNumber());
                CLog.e(e);
                throw new RuntimeException(e);
            }
        }
        return RunStrategy.RUN;
    }
}
