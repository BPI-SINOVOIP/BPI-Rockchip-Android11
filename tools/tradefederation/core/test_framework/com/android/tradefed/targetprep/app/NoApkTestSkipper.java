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
package com.android.tradefed.targetprep.app;

import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.config.IDeviceConfiguration;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.targetprep.BaseTargetPreparer;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.TargetSetupError;

import java.util.ArrayList;

/**
 * Special preparer that allows to skip an invocation completely (preparation and tests) if there
 * are no apks to tests.
 */
@OptionClass(alias = "no-apk-skipper")
public final class NoApkTestSkipper extends BaseTargetPreparer implements IConfigurationReceiver {

    private IConfiguration mConfiguration;

    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        if (testInfo.getBuildInfo().getAppPackageFiles().isEmpty()) {
            CLog.d("No app to install, skipping the tests");

            for (IDeviceConfiguration deviceConfig : mConfiguration.getDeviceConfig()) {
                for (ITargetPreparer preparer : deviceConfig.getTargetPreparers()) {
                    preparer.setDisable(true);
                }
            }

            mConfiguration.setTests(new ArrayList<>());
        }
    }

    @Override
    public void setConfiguration(IConfiguration configuration) {
        mConfiguration = configuration;
    }
}
