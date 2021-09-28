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

package com.android.tradefed.targetprep.suite;

import com.android.tradefed.config.OptionClass;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.result.error.InfraErrorIdentifier;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.targetprep.TestAppInstallSetup;

import java.io.File;
import java.io.FileNotFoundException;

/**
 * Installs specified APKs for Suite configuration: either from $ANDROID_TARGET_OUT_TESTCASES
 * variable or the ROOT_DIR in build info.
 */
@OptionClass(alias = "apk-installer")
public class SuiteApkInstaller extends TestAppInstallSetup {

    /** {@inheritDoc} */
    @Override
    protected File getLocalPathForFilename(TestInformation testInfo, String apkFileName)
            throws TargetSetupError {
        File apkFile = null;
        try {
            apkFile = testInfo.getDependencyFile(apkFileName, true);
        } catch (FileNotFoundException e) {
            throw new TargetSetupError(
                    String.format("%s not found", apkFileName),
                    e,
                    testInfo.getDevice().getDeviceDescriptor(),
                    InfraErrorIdentifier.ARTIFACT_NOT_FOUND);
        }
        return apkFile;
    }
}
