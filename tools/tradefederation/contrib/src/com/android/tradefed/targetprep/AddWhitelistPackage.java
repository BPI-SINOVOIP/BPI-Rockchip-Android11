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

import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;

import java.util.ArrayList;
import java.util.List;

/** Add packages to whitelist to allow it to run in the background. */
@OptionClass(alias = "add-whitelist-package")
public class AddWhitelistPackage extends BaseTargetPreparer {
    @Option(
            name = "whitelist-package-name",
            description = "Name of package to put in whitelist"
    )
    private List<String> mPackages = new ArrayList<>();

    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        for (String pkg : mPackages) {
            testInfo.getDevice()
                    .executeShellCommand(String.format("dumpsys deviceidle whitelist +%s", pkg));
        }

        CLog.d(testInfo.getDevice().executeShellCommand("dumpsys deviceidle whitelist"));
    }

    @Override
    public void tearDown(TestInformation testInfo, Throwable e) throws DeviceNotAvailableException {
        for (String pkg : mPackages) {
            testInfo.getDevice()
                    .executeShellCommand(String.format("dumpsys deviceidle whitelist -%s", pkg));
        }
    }
}
