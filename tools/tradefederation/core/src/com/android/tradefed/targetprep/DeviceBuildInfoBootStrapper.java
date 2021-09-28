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

import com.android.tradefed.build.BootstrapBuildProvider;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.util.BuildInfoUtil;

/**
 * A {@link ITargetPreparer} that replaces build info fields with attributes read from device
 *
 * <p>This is useful for testing devices with builds generated from an external source (e.g.
 * external partner devices)
 *
 * @see DeviceBuildInfoInjector
 * @see BootstrapBuildProvider
 */
public class DeviceBuildInfoBootStrapper extends BaseTargetPreparer {

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
        BuildInfoUtil.bootstrapDeviceBuildAttributes(
                testInfo.getBuildInfo(),
                testInfo.getDevice(),
                mOverrideDeviceBuildId,
                mOverrideDeviceBuildFlavor,
                mOverrideDeviceBuildBranch,
                mOverrideDeviceBuildAlias);
    }
}
