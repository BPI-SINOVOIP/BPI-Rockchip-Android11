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

import static com.google.common.base.Preconditions.checkArgument;
import static com.google.common.base.Preconditions.checkNotNull;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.targetprep.TestAppInstallSetup;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;
import java.util.stream.Collectors;

/** A Tradefed preparer that downloads and installs an app on the target device. */
public final class AppSetupPreparer implements ITargetPreparer {

    public static final String OPTION_GCS_APK_DIR = "gcs-apk-dir";

    @Option(name = "package-name", description = "Package name of the app being tested.")
    private String mPackageName;

    private final TestAppInstallSetup mAppInstallSetup;

    public AppSetupPreparer() {
        this(null, new TestAppInstallSetup());
    }

    @VisibleForTesting
    public AppSetupPreparer(String packageName, TestAppInstallSetup appInstallSetup) {
        this.mPackageName = packageName;
        this.mAppInstallSetup = appInstallSetup;
    }

    /** {@inheritDoc} */
    @Override
    public void setUp(ITestDevice device, IBuildInfo buildInfo)
            throws DeviceNotAvailableException, BuildError, TargetSetupError {
        // TODO(b/147159584): Use a utility to get dynamic options.
        String gcsApkDirOption = buildInfo.getBuildAttributes().get(OPTION_GCS_APK_DIR);
        checkNotNull(gcsApkDirOption, "Option %s is not set.", OPTION_GCS_APK_DIR);

        File apkDir = new File(gcsApkDirOption);
        checkArgument(
                apkDir.isDirectory(),
                String.format("GCS Apk Directory %s is not a directory", apkDir));

        File packageDir = new File(apkDir.getPath(), mPackageName);
        checkArgument(
                packageDir.isDirectory(),
                String.format("Package directory %s is not a directory", packageDir));

        mAppInstallSetup.setAltDir(packageDir);

        List<String> apkFilePaths;
        try {
            apkFilePaths = listApkFilePaths(packageDir);
        } catch (IOException e) {
            throw new TargetSetupError(
                    String.format("Failed to access files in %s.", packageDir), e);
        }

        if (apkFilePaths.isEmpty()) {
            throw new TargetSetupError(
                    String.format("Failed to find apk files in %s.", packageDir));
        }

        if (apkFilePaths.size() == 1) {
            mAppInstallSetup.addTestFileName(apkFilePaths.get(0));
        } else {
            mAppInstallSetup.addSplitApkFileNames(String.join(",", apkFilePaths));
        }

        mAppInstallSetup.setUp(device, buildInfo);
    }

    /** {@inheritDoc} */
    @Override
    public void tearDown(TestInformation testInfo, Throwable e) throws DeviceNotAvailableException {
        mAppInstallSetup.tearDown(testInfo, e);
    }

    private List<String> listApkFilePaths(File downloadDir) throws IOException {
        return Files.walk(Paths.get(downloadDir.getPath()))
                .map(x -> x.getFileName().toString())
                .filter(s -> s.endsWith(".apk"))
                .collect(Collectors.toList());
    }
}
