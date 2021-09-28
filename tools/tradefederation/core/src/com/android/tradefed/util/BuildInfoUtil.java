/*
 * Copyright (C) 2014 The Android Open Source Project
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
package com.android.tradefed.util;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.DeviceProperties;
import com.android.tradefed.device.ITestDevice;

/** A util class to help manipulate {@link IBuildInfo} */
public class BuildInfoUtil {

    /**
     * Reads build attributes from device and use them to override the relevant build info fields
     *
     * <p>Note: because branch information is not stored on device as build attributes, the injected
     * branch info will be the following fields concatenated via dashes:
     *
     * <ul>
     *   <li><code>ro.product.brand</code>
     *   <li><code>ro.product.name</code>
     *   <li><code>ro.product.vendor.device</code> (maybe different on older API levels)
     *   <li><code>ro.build.version.release</code>
     * </ul>
     *
     * @param buildInfo the build info where device build attributes will be injected
     * @param device the device to read build attributes from
     * @param overrideBuildId instead of reading from device, override build id to this value;
     *     <code>null</code> for no override
     * @param overrideBuildFlavor instead of reading from device, override build flavor to this
     *     value; <code>null</code> for no override
     * @param overrideBuildBranch instead of concatenating device attributes as substitute for
     *     branch, override it to this value; <code>null</code> for no override
     * @param overrideBuildAlias instead of reading from device, override build alias to this value;
     *     <code>null</code> for no override
     * @throws DeviceNotAvailableException
     */
    public static void bootstrapDeviceBuildAttributes(
            IBuildInfo buildInfo,
            ITestDevice device,
            String overrideBuildId,
            String overrideBuildFlavor,
            String overrideBuildBranch,
            String overrideBuildAlias)
            throws DeviceNotAvailableException {
        String buildId, buildAlias, buildFlavor, branch;
        // inject build id
        if (overrideBuildId != null) {
            buildId = overrideBuildId;
        } else {
            buildId = device.getBuildId();
        }
        buildInfo.setBuildId(buildId);

        // inject build alias
        if (overrideBuildAlias != null) {
            buildAlias = overrideBuildAlias;
        } else {
            buildAlias = device.getBuildAlias();
        }
        buildInfo.addBuildAttribute("build_alias", buildAlias);

        // inject build flavor
        if (overrideBuildFlavor != null) {
            buildFlavor = overrideBuildFlavor;
        } else {
            buildFlavor = device.getBuildFlavor();
        }
        buildInfo.setBuildFlavor(buildFlavor);

        // generate branch information, either via parameter override, or via concatenating fields
        if (overrideBuildBranch != null) {
            branch = overrideBuildBranch;
        } else {
            branch =
                    String.format(
                            "%s-%s-%s-%s",
                            device.getProperty(DeviceProperties.BRAND),
                            device.getProperty(DeviceProperties.PRODUCT),
                            device.getProductVariant(),
                            device.getProperty(DeviceProperties.RELEASE_VERSION));
        }
        buildInfo.setBuildBranch(branch);
    }
}
