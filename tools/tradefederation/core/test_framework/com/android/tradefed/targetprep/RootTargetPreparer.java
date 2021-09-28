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
package com.android.tradefed.targetprep;

import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.error.DeviceErrorIdentifier;

/**
 * Target preparer that performs "adb root" or "adb unroot" based on option "force-root".
 *
 * <p>Will restore back to original root state on tear down.
 */
@OptionClass(alias = "root-preparer")
public final class RootTargetPreparer extends BaseTargetPreparer {

    private boolean mWasRoot = false;

    @Option(
            name = "force-root",
            description =
                    "Force the preparer to enable adb root if set to true. Otherwise, disable adb "
                            + "root during setup.")
    private boolean mForceRoot = true;

    @Option(
            name = "throw-on-error",
            description = "Throws TargetSetupError if adb root/unroot fails")
    private boolean mThrowOnError = true;

    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        ITestDevice device = testInfo.getDevice();
        // Ignore setUp if it's a stub device, since there is no real device to set up.
        if (device.getIDevice() instanceof StubDevice) {
            return;
        }
        mWasRoot = device.isAdbRoot();
        if (!mWasRoot && mForceRoot && !device.enableAdbRoot()) {
            throwOrLog("Failed to adb root device", device.getDeviceDescriptor());
        } else if (mWasRoot && !mForceRoot && !device.disableAdbRoot()) {
            throwOrLog("Failed to adb unroot device", device.getDeviceDescriptor());
        }
    }

    @Override
    public void tearDown(TestInformation testInfo, Throwable e) throws DeviceNotAvailableException {
        ITestDevice device = testInfo.getDevice();
        // Ignore tearDown if it's a stub device, since there is no real device to clean.
        if (device.getIDevice() instanceof StubDevice) {
            return;
        }
        if (!mWasRoot && mForceRoot) {
            device.disableAdbRoot();
        } else if (mWasRoot && !mForceRoot) {
            device.enableAdbRoot();
        }
    }

    private void throwOrLog(String message, DeviceDescriptor deviceDescriptor)
            throws TargetSetupError {
        if (mThrowOnError) {
            throw new TargetSetupError(
                    message, deviceDescriptor, DeviceErrorIdentifier.DEVICE_UNEXPECTED_RESPONSE);
        } else {
            CLog.w(message + " " + deviceDescriptor);
        }
    }
}
