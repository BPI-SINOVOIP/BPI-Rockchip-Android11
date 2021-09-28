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
package com.android.tradefed.invoker.shard;

import com.android.tradefed.build.BuildInfoKey.BuildInfoFileKey;
import com.android.tradefed.build.ExistingBuildProvider;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.VersionedFile;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IDeviceConfiguration;
import com.android.tradefed.invoker.ExecutionFiles;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;

/**
 * Helper class that handles cloning a build info from the command line. Shard will get the build
 * info directly by using {@link ExistingBuildProvider} instead of downloading anything.
 */
public class ShardBuildCloner {

    /**
     * Helper to set the Sharded configuration build provider to the {@link ExistingBuildProvider}.
     *
     * @param fromConfig Original configuration
     * @param toConfig cloned configuration recreated from the command line.
     * @param testInfo The {@link TestInformation} of the parent shard
     */
    public static void cloneBuildInfos(
            IConfiguration fromConfig, IConfiguration toConfig, TestInformation testInfo) {
        IInvocationContext context = testInfo.getContext();
        IBuildInfo primaryClone = null;
        for (String deviceName : context.getDeviceConfigNames()) {
            IBuildInfo toBuild = context.getBuildInfo(deviceName).clone();
            if (primaryClone == null) {
                primaryClone = toBuild;
            }
            try {
                IDeviceConfiguration deviceConfig = toConfig.getDeviceConfigByName(deviceName);
                if (deviceConfig == null) {
                    throw new RuntimeException(
                            String.format(
                                    "Configuration doesn't have device '%s' while context "
                                            + "does [%s].",
                                    deviceName, context.getDeviceConfigNames()));
                }
                deviceConfig.addSpecificConfig(
                        new ExistingBuildProvider(
                                toBuild,
                                fromConfig.getDeviceConfigByName(deviceName).getBuildProvider()));
            } catch (ConfigurationException e) {
                // Should never happen, no action taken
                CLog.e(e);
            }
        }

        IBuildInfo primaryBuild = context.getBuildInfos().get(0);
        // Ensure that files that were shared by IBuildInfo and TestInformation that were cloned
        // are relinked to the new TestInformation.
        TestInformation newInfo = TestInformation.createCopyTestInfo(testInfo, context);
        ExecutionFiles execFiles = newInfo.executionFiles();
        if (execFiles.get(FilesKey.TESTS_DIRECTORY) != null) {
            if (execFiles
                    .get(FilesKey.TESTS_DIRECTORY)
                    .equals(primaryBuild.getFile(BuildInfoFileKey.TESTDIR_IMAGE))) {
                execFiles.put(
                        FilesKey.TESTS_DIRECTORY,
                        primaryClone.getFile(BuildInfoFileKey.TESTDIR_IMAGE));
            }
        }
        if (execFiles.get(FilesKey.HOST_TESTS_DIRECTORY) != null) {
            if (execFiles
                    .get(FilesKey.HOST_TESTS_DIRECTORY)
                    .equals(primaryBuild.getFile(BuildInfoFileKey.HOST_LINKED_DIR))) {
                execFiles.put(
                        FilesKey.HOST_TESTS_DIRECTORY,
                        primaryClone.getFile(BuildInfoFileKey.HOST_LINKED_DIR));
            }
        }
        if (execFiles.get(FilesKey.TARGET_TESTS_DIRECTORY) != null) {
            if (execFiles
                    .get(FilesKey.TARGET_TESTS_DIRECTORY)
                    .equals(primaryBuild.getFile(BuildInfoFileKey.TARGET_LINKED_DIR))) {
                execFiles.put(
                        FilesKey.TARGET_TESTS_DIRECTORY,
                        primaryClone.getFile(BuildInfoFileKey.TARGET_LINKED_DIR));
            }
        }
        // Link the remaining buildInfo files.
        for (String key : primaryBuild.getVersionedFileKeys()) {
            VersionedFile versionedFile = primaryBuild.getVersionedFile(key);
            if (versionedFile.getFile().equals(execFiles.get(key))) {
                execFiles.put(key, primaryClone.getFile(key));
            }
        }
        try {
            toConfig.setConfigurationObject(ShardHelper.SHARED_TEST_INFORMATION, newInfo);
        } catch (ConfigurationException e) {
            // Should never happen, no action taken
            CLog.e(e);
        }
    }
}
