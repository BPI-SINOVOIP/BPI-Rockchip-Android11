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
package com.android.tradefed.testtype.suite.module;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;

/** This controller prevents execution of tests cases on non-automotive devices. */
public class CarModuleController extends BaseModuleController {

    private static final String FEATURE_AUTOMOTIVE = "android.hardware.type.automotive";

    @Override
    public RunStrategy shouldRun(IInvocationContext context) {
        for (ITestDevice device : context.getDevices()) {
            if (device.getIDevice() instanceof StubDevice) {
                continue;
            }
            try {
                if (device.hasFeature(FEATURE_AUTOMOTIVE)) {
                    return RunStrategy.RUN;
                }
            } catch (DeviceNotAvailableException e) {
                CLog.e("Couldn't check for automotive feature on %s", device.getSerialNumber());
                CLog.e(e);
            }
        }

        CLog.d("Bypassing module %s because it is only applicable to automotive.", getModuleName());

        return RunStrategy.FULL_MODULE_BYPASS;
    }
}
