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
import com.android.tradefed.util.AbiUtils;

/** Base class for a module controller to not run tests when it doesn't match the architecture . */
public class KernelTestModuleController extends BaseModuleController {
    private final String lowMemProp = "ro.config.low_ram";
    private final String productNameProp = "ro.product.name";

    @Option(name = "arch",
            description = "The architecture name that should run for this module."
                    + "This should be like arm64, arm, x86_64, x86, mips64, or mips.",
            mandatory = true)
    private String mArch = null;

    @Option(name = "is-low-mem",
            description = "If this option set to true, run this module if device prop"
                    + "of 'ro.config.low_ram' is true else skip it.")
    private boolean mIsLowMem = false;

    @Option(name = "is-hwasan",
            description = "If this option set to true, run this module if device prop "
                    + "of 'ro.product.name' ended with _hwasan else skip it.")
    private boolean mIsHwasan = false;

    @Override
    public RunStrategy shouldRun(IInvocationContext context) {
        // This should return arm64-v8a or armeabi-v7a
        String moduleAbiName = getModuleAbi().getName();
        // Use AbiUtils to get the actual architecture name.
        // If moduleAbiName is arm64-v8a then the moduleArchName will be arm64
        // If moduleAbiName is armeabi-v7a then the moduleArchName will be arm
        String moduleArchName = AbiUtils.getArchForAbi(moduleAbiName);

        if (mIsLowMem) {
            if (!deviceLowMem(context)) {
                CLog.d("Skipping module %s because %s is False.", getModuleName(), lowMemProp);
                return RunStrategy.FULL_MODULE_BYPASS;
            }
        } else {
            if (deviceLowMem(context)) {
                CLog.d("Skipping module %s because the test is not for low memory device.",
                        getModuleName());
                return RunStrategy.FULL_MODULE_BYPASS;
            }
        }

        if (mIsHwasan) {
            if (!deviceWithHwasan(context)) {
                CLog.d("Skipping module %s because %s is not ended with _hwasan.", getModuleName(),
                        productNameProp);
                return RunStrategy.FULL_MODULE_BYPASS;
            }
        } else {
            if (deviceWithHwasan(context)) {
                CLog.d("Skipping module %s because the test is for device of hwasan.",
                        getModuleName());
                return RunStrategy.FULL_MODULE_BYPASS;
            }
        }

        if (mArch.equals(moduleArchName)) {
            return RunStrategy.RUN;
        }
        CLog.d("Skipping module %s running on abi %s, which doesn't match any required setting "
                        + "of %s.",
                getModuleName(), moduleAbiName, mArch);
        return RunStrategy.FULL_MODULE_BYPASS;
    }

    private boolean deviceLowMem(IInvocationContext context) {
        for (ITestDevice device : context.getDevices()) {
            if (device.getIDevice() instanceof StubDevice) {
                continue;
            }
            try {
                String lowMemString = device.getProperty(lowMemProp);
                boolean isLowMem = false;
                if (lowMemString != null) {
                    isLowMem = Boolean.parseBoolean(lowMemString);
                } else {
                    CLog.d("Cannot get the prop of %s.", lowMemProp);
                }
                if (isLowMem) {
                    continue;
                }
                return false;
            } catch (DeviceNotAvailableException e) {
                CLog.e("Couldn't check prop of %s on %s", lowMemProp, device.getSerialNumber());
                CLog.e(e);
                throw new RuntimeException(e);
            }
        }
        return true;
    }

    private boolean deviceWithHwasan(IInvocationContext context) {
        for (ITestDevice device : context.getDevices()) {
            if (device.getIDevice() instanceof StubDevice) {
                continue;
            }
            try {
                String productName = device.getProperty(productNameProp);
                boolean isHwasan = false;
                if (productName != null) {
                    isHwasan = productName.contains("_hwasan");
                } else {
                    CLog.d("Cannot get the prop of %s.", productNameProp);
                }
                if (isHwasan) {
                    continue;
                }
                return false;
            } catch (DeviceNotAvailableException e) {
                CLog.e("Couldn't check prop of %s on %s", productNameProp,
                        device.getSerialNumber());
                CLog.e(e);
                throw new RuntimeException(e);
            }
        }
        return true;
    }
}
