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
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.targetprep.TestAppInstallSetup;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.testng.Assert.assertThrows;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;

@RunWith(JUnit4.class)
public class AppSetupPreparerTest {

    private static final String OPTION_GCS_APK_DIR = "gcs-apk-dir";
    public static final ITestDevice NULL_DEVICE = null;

    @Rule public final TemporaryFolder tempFolder = new TemporaryFolder();

    private final IBuildInfo mBuildInfo = new BuildInfo();
    private final TestAppInstallSetup mMockAppInstallSetup = mock(TestAppInstallSetup.class);
    private final AppSetupPreparer mPreparer =
            new AppSetupPreparer("package_name", mMockAppInstallSetup);

    @Test
    public void setUp_gcsApkDirIsNull_throwsException()
            throws DeviceNotAvailableException, TargetSetupError {
        mBuildInfo.addBuildAttribute(OPTION_GCS_APK_DIR, null);

        assertThrows(NullPointerException.class, () -> mPreparer.setUp(NULL_DEVICE, mBuildInfo));
    }

    @Test
    public void setUp_gcsApkDirIsNotDir_throwsException()
            throws IOException, DeviceNotAvailableException, TargetSetupError {
        File tempFile = tempFolder.newFile("temp_file_name");
        mBuildInfo.addBuildAttribute(OPTION_GCS_APK_DIR, tempFile.getPath());

        assertThrows(
                IllegalArgumentException.class, () -> mPreparer.setUp(NULL_DEVICE, mBuildInfo));
    }

    @Test
    public void setUp_packageDirDoesNotExist_throwsError()
            throws IOException, DeviceNotAvailableException, TargetSetupError {
        File gcsApkDir = tempFolder.newFolder("gcs_apk_dir");
        mBuildInfo.addBuildAttribute(OPTION_GCS_APK_DIR, gcsApkDir.getPath());

        assertThrows(
                IllegalArgumentException.class, () -> mPreparer.setUp(NULL_DEVICE, mBuildInfo));
    }

    @Test
    public void setUp_apkDoesNotExist() throws Exception {
        File gcsApkDir = tempFolder.newFolder("gcs_apk_dir");
        createPackageFile(gcsApkDir, "package_name", "non_apk_file");
        mBuildInfo.addBuildAttribute(OPTION_GCS_APK_DIR, gcsApkDir.getPath());

        assertThrows(TargetSetupError.class, () -> mPreparer.setUp(NULL_DEVICE, mBuildInfo));
    }

    @Test
    public void setUp_installSplitApk() throws Exception {
        File gcsApkDir = tempFolder.newFolder("gcs_apk_dir");
        File packageDir = new File(gcsApkDir.getPath(), "package_name");
        createPackageFile(gcsApkDir, "package_name", "apk_name_1.apk");
        createPackageFile(gcsApkDir, "package_name", "apk_name_2.apk");
        mBuildInfo.addBuildAttribute(OPTION_GCS_APK_DIR, gcsApkDir.getPath());

        mPreparer.setUp(NULL_DEVICE, mBuildInfo);

        verify(mMockAppInstallSetup).setAltDir(packageDir);
        verify(mMockAppInstallSetup).addSplitApkFileNames("apk_name_2.apk,apk_name_1.apk");
        verify(mMockAppInstallSetup).setUp(any(), any());
    }

    @Test
    public void setUp_installNonSplitApk() throws Exception {
        File gcsApkDir = tempFolder.newFolder("gcs_apk_dir");
        File packageDir = new File(gcsApkDir.getPath(), "package_name");
        createPackageFile(gcsApkDir, "package_name", "apk_name_1.apk");
        mBuildInfo.addBuildAttribute(OPTION_GCS_APK_DIR, gcsApkDir.getPath());

        mPreparer.setUp(NULL_DEVICE, mBuildInfo);

        verify(mMockAppInstallSetup).setAltDir(packageDir);
        verify(mMockAppInstallSetup).addTestFileName("apk_name_1.apk");
        verify(mMockAppInstallSetup).setUp(any(), any());
    }

    @Test
    public void tearDown() throws Exception {
        TestInformation testInfo = TestInformation.newBuilder().build();

        mPreparer.tearDown(testInfo, null);

        verify(mMockAppInstallSetup, times(1)).tearDown(testInfo, null);
    }

    private File createPackageFile(File parentDir, String packageName, String apkName)
            throws IOException {
        File packageDir =
                Files.createDirectories(Paths.get(parentDir.getAbsolutePath(), packageName))
                        .toFile();

        return Files.createFile(Paths.get(packageDir.getAbsolutePath(), apkName)).toFile();
    }
}
