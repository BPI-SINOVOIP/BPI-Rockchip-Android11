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

package com.android.tradefed.targetprep;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.targetprep.IDeviceFlasher.UserDataFlashOption;
import com.android.tradefed.util.BuildInfoUtil;

import java.io.File;

/**
 * An {@link ITargetPreparer} that stages specified files (bootloader, radio, device image zip) into
 * {@link IDeviceBuildInfo} to get devices flashed with {@link FastbootDeviceFlasher}, then injects
 * post-boot device attributes into the build info for result reporting purposes.
 *
 * <p>This is useful for using <code>fastboot update</code> as device image update mechanism from
 * externally sourced devices and builds, to fit into existing automation infrastructure.
 */
public class FastbootUpdateBootstrapPreparer extends DeviceFlashPreparer {

    @Option(name = "bootloader-image", description = "bootloader image file to be used for update")
    private File mBootloaderImage = null;

    @Option(name = "baseband-image", description = "radio image file to be used for update")
    private File mBasebandImage = null;

    @Option(name = "device-image", description = "device image file to be used for update")
    private File mDeviceImage = null;

    @Option(
        name = "bootstrap-build-info",
        description =
                "whether build info should be"
                        + "bootstrapped based on device attributes after flashing"
    )
    private boolean mBootStrapBuildInfo = true;

    // parameters below are the same as DeviceBuildInfoBootStrapper
    @Option(name = "override-device-build-id", description = "the device buid id to inject.")
    private String mOverrideDeviceBuildId = null;

    @Option(name = "override-device-build-alias", description = "the device buid alias to inject.")
    private String mOverrideDeviceBuildAlias = null;

    @Option(
        name = "override-device-build-flavor",
        description = "the device build flavor to inject."
    )
    private String mOverrideDeviceBuildFlavor = null;

    @Option(
        name = "override-device-build-branch",
        description = "the device build branch to inject."
    )
    private String mOverrideDeviceBuildBranch = null;

    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        ITestDevice device = testInfo.getDevice();
        IBuildInfo buildInfo = testInfo.getBuildInfo();
        if (!(buildInfo instanceof IDeviceBuildInfo)) {
            throw new IllegalArgumentException("Provided build info must be a IDeviceBuildInfo");
        }
        // forcing the wipe mechanism to WIPE because the FLASH* based options are not feasible here
        setUserDataFlashOption(UserDataFlashOption.WIPE);
        IDeviceBuildInfo deviceBuildInfo = (IDeviceBuildInfo) buildInfo;
        deviceBuildInfo.setBootloaderImageFile(mBootloaderImage, "0");
        deviceBuildInfo.setBasebandImage(mBasebandImage, "0");
        deviceBuildInfo.setDeviceImageFile(mDeviceImage, "0");
        setSkipPostFlashBuildIdCheck(true);
        setSkipPostFlashFlavorCheck(true);
        // performs the actual flashing
        super.setUp(testInfo);

        if (mBootStrapBuildInfo) {
            BuildInfoUtil.bootstrapDeviceBuildAttributes(
                    buildInfo,
                    device,
                    mOverrideDeviceBuildId,
                    mOverrideDeviceBuildFlavor,
                    mOverrideDeviceBuildBranch,
                    mOverrideDeviceBuildAlias);
        }
    }

    @Override
    protected IDeviceFlasher createFlasher(ITestDevice device) throws DeviceNotAvailableException {
        return new FastbootDeviceFlasher();
    }
}
