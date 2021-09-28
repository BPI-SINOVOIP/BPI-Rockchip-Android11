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
package com.android.tradefed.testtype.suite.params;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IDeviceConfiguration;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.InstallApexModuleTargetPreparer;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.util.AbiUtils;

import java.util.Arrays;
import java.util.List;

/**
 * A simple handler class for Mainline Modules that creates a InstallApexModuleTargetPreparer and
 * injects the dynamic link into it based on the given mainline modules to automatically retrieve
 * those modules.
 */
public final class MainlineModuleHandler {

    private String mDynamicBaseLink = null;
    private IAbi mAbi = null;
    private String mName = null;

    public MainlineModuleHandler(String name, IAbi abi, IInvocationContext context) {
        mName = name;
        mAbi = abi;
        buildDynamicBaseLink(context.getBuildInfos().get(0));
    }

    /** Builds the dynamic base link where the mainline modules would be downloaded. */
    private void buildDynamicBaseLink(IBuildInfo buildInfo) {
        if (buildInfo == null) {
            throw new IllegalArgumentException(
                    "Missing build information when enable-mainline-parameterized-modules is set.");
        }
        CLog.i("Building the dynamic base link based on the build information.");
        String buildBranch = buildInfo.getBuildBranch();
        String buildFlavor = buildInfo.getBuildFlavor();
        String buildId = buildInfo.getBuildId();
        if (buildBranch == null || buildFlavor == null || buildId == null) {
            throw new IllegalArgumentException(
                    "Missing required information to build the dynamic base link.");
        }
        mDynamicBaseLink = String.format("ab://%s/%s/%s", buildBranch, buildFlavor, buildId);
    }


    /** Apply to the module {@link IConfiguration} the parameter specific mainline module setup. */
    public void applySetup(IConfiguration moduleConfiguration) {
        for (IDeviceConfiguration deviceConfig : moduleConfiguration.getDeviceConfig()) {
            List<ITargetPreparer> preparers = deviceConfig.getTargetPreparers();
            preparers.add(0, createMainlineModuleInstaller());
        }
    }

    /** Crete a {@link InstallApexModuleTargetPreparer} with the dynamic link built based on the
     * mainline modules. If a module is like .apk/.apex, the link will be appended with
     * mainline_modules_{abi}.
     */
    private InstallApexModuleTargetPreparer createMainlineModuleInstaller() {
        InstallApexModuleTargetPreparer mainlineModuleInstaller =
                new InstallApexModuleTargetPreparer();
        // Inject the real dynamic link to the target preparer so that it will dynamically download
        // the mainline modules.
        String fullDynamicLink = mDynamicBaseLink;
        for (String mainlineModule : Arrays.asList(mName.split(String.format("\\+")))) {
            if (!mainlineModule.endsWith(".apks")) {
                fullDynamicLink =
                        String.format(
                                "%s/mainline_modules_%s",
                                        mDynamicBaseLink, AbiUtils.getArchForAbi(mAbi.getName()));
            }
            mainlineModuleInstaller.addTestFileName(
                    String.format("%s/%s", fullDynamicLink, mainlineModule)
            );
        }
        return mainlineModuleInstaller;
    }
}
