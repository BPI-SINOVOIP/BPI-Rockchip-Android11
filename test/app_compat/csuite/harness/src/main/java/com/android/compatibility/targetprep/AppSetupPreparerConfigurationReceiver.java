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

package com.android.compatibility.targetprep;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.targetprep.ITargetPreparer;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;

/**
 * A Tradefed preparer that receives module preparer options and stores the values in IBuildInfo.
 */
public final class AppSetupPreparerConfigurationReceiver implements ITargetPreparer {

    @Option(
            name = AppSetupPreparer.OPTION_GCS_APK_DIR,
            description = "GCS path where the test apk files are located.")
    private File mOptionGcsApkDir;

    public AppSetupPreparerConfigurationReceiver() {
        this(null);
    }

    @VisibleForTesting
    public AppSetupPreparerConfigurationReceiver(File optionGcsApkDir) {
        mOptionGcsApkDir = optionGcsApkDir;
    }

    /** {@inheritDoc} */
    @Override
    public void setUp(ITestDevice device, IBuildInfo buildInfo) {
        if (mOptionGcsApkDir == null) {
            return;
        }
        buildInfo.addBuildAttribute(
                AppSetupPreparer.OPTION_GCS_APK_DIR, mOptionGcsApkDir.getPath());
    }
}
