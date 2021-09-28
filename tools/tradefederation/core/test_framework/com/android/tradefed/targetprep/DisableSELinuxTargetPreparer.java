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
package com.android.tradefed.targetprep;

import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;

/**
 * Target preparer that disables SELinux if enabled.
 *
 * <p>Will restore back to original state on tear down.
 */
@OptionClass(alias = "disable-selinux-preparer")
public class DisableSELinuxTargetPreparer extends BaseTargetPreparer {

    private boolean mWasRoot = false;
    private boolean mWasPermissive = false;
    private static final String PERMISSIVE = "Permissive";

    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        ITestDevice device = testInfo.getDevice();
        CommandResult result = device.executeShellV2Command("getenforce");
        mWasPermissive = result.getStdout().contains(PERMISSIVE);
        if (mWasPermissive) {
            CLog.d(
                    "SELinux was not enabled therefore DisableSELinuxTargetPreparer will do "
                            + "nothing. This is because enabling 'setenforce 1' blindly on "
                            + "tearDown can cause problems on devices that don't support it, "
                            + "i.e. on gce avds the GPU will crash.");
            return;
        }
        mWasRoot = device.isAdbRoot();
        if (!mWasRoot && !device.enableAdbRoot()) {
            throw new TargetSetupError("Failed to adb root device", device.getDeviceDescriptor());
        }
        CLog.d("Disabling SELinux.");
        result = device.executeShellV2Command("setenforce 0");
        if (result.getStatus() != CommandStatus.SUCCESS) {
            throw new TargetSetupError(
                    "Disabling SELinux failed with status: " + result.getStatus(),
                    device.getDeviceDescriptor());
        }
        if (!mWasRoot) {
            device.disableAdbRoot();
        }
    }

    @Override
    public void tearDown(TestInformation testInfo, Throwable e) throws DeviceNotAvailableException {
        if (mWasPermissive) {
            return;
        }
        ITestDevice device = testInfo.getDevice();
        if (!mWasRoot) {
            device.enableAdbRoot();
        }
        CLog.d("Enabling SELinux.");
        device.executeShellV2Command("setenforce 1");
        if (!mWasRoot) {
            device.disableAdbRoot();
        }
    }
}
