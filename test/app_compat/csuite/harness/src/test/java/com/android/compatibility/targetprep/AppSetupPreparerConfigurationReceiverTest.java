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

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.IBuildInfo;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;

@RunWith(JUnit4.class)
public final class AppSetupPreparerConfigurationReceiverTest {

    @Test
    public void setUp_noneNullGcsApkDirOption_putsInBuildInfo() {
        File optionGcsApkDir = new File("dir");
        AppSetupPreparerConfigurationReceiver preparer =
                new AppSetupPreparerConfigurationReceiver(optionGcsApkDir);
        IBuildInfo buildInfo = new BuildInfo();

        preparer.setUp(null, buildInfo);

        assertThat(buildInfo.getBuildAttributes())
                .containsEntry(AppSetupPreparer.OPTION_GCS_APK_DIR, optionGcsApkDir.getPath());
    }

    @Test
    public void setUp_nullGcsApkDirOption_doesNotPutInBuildInfo() {
        File optionGcsApkDir = null;
        AppSetupPreparerConfigurationReceiver preparer =
                new AppSetupPreparerConfigurationReceiver(optionGcsApkDir);
        IBuildInfo buildInfo = new BuildInfo();

        preparer.setUp(null, buildInfo);

        assertThat(buildInfo.getBuildAttributes())
                .doesNotContainKey(AppSetupPreparer.OPTION_GCS_APK_DIR);
    }
}
