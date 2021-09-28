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
package com.android.tradefed.testtype.suite.module;

import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;

/** A module controller to not run tests when it doesn't support certain feature. */
public class DeviceFeatureModuleController extends BaseModuleController {

    @Option(name = "required-feature", description = "Required feature for the test to run.")
    private String mRequiredFeature;

    @Override
    public RunStrategy shouldRun(IInvocationContext context) {
        for (ITestDevice device : context.getDevices()) {
            if (device.getIDevice() instanceof StubDevice) {
                continue;
            }
            try {
                if (!device.hasFeature(mRequiredFeature)) {
                    CLog.d(
                            "Skipping module %s because the device does not have feature %s.",
                            getModuleName(), mRequiredFeature);
                    return RunStrategy.FULL_MODULE_BYPASS;
                }
            } catch (DeviceNotAvailableException e) {
                CLog.e("Couldn't get device features on %s", device.getSerialNumber());
                CLog.e(e);
                throw new RuntimeException(e);
            }
        }
        return RunStrategy.RUN;
    }
}
