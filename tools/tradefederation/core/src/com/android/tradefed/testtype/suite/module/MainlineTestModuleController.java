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

import com.android.tradefed.device.ITestDevice.ApexInfo;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import com.android.tradefed.config.Option;

/**
 * Base class for a module controller to run tests based on the preloaded mainline modules on the
 * device under test.
 */
public class MainlineTestModuleController extends BaseModuleController {

    @Option(name = "enable", description = "Enable or disable this module controller.")
    private boolean mControllerEnabled = false;

    @Option(
            name = "mainline-module-package-name",
            description =
                    "The mainline modules that must be preloaded in order to"
                            + "run the test module. Any matching module preloaded on"
                            + "test device will cause the test module to be executed.")
    private List<String> mMainlineModules = new ArrayList<String>();

    private Set<ApexInfo> mActiveApexes;

    @Override
    public RunStrategy shouldRun(IInvocationContext context) {
        if (!mControllerEnabled) {
            return RunStrategy.RUN;
        }
        if (mMainlineModules.isEmpty()) {
            CLog.i(
                    "MainlineTestModuleController is enabled, no mainline"
                            + "module specified. Running test module: %s.",
                    getModuleName());
            return RunStrategy.RUN;
        }
        for (ITestDevice device : context.getDevices()) {
            if (device.getIDevice() instanceof StubDevice) {
                continue;
            }
            try {
                boolean modulePreloaded = false;
                mActiveApexes = device.getActiveApexes();
                for (ITestDevice.ApexInfo ap : mActiveApexes) {
                    if (mMainlineModules.contains(ap.name)) {
                        modulePreloaded = true;
                        break;
                    }
                }
                if (!modulePreloaded) {
                    CLog.i(
                            "Skipping test module %s because mainline module %s is not active.",
                            getModuleName(), mMainlineModules.toString());
                    return RunStrategy.FULL_MODULE_BYPASS;
                }
            } catch (DeviceNotAvailableException e) {
                CLog.e("Skipping the test module %s.", getModuleName());
                CLog.e(e);
                return RunStrategy.FULL_MODULE_BYPASS;
            }
        }
        return RunStrategy.RUN;
    }
}
