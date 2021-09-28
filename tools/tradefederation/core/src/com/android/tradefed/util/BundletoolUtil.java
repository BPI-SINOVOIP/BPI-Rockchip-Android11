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

package com.android.tradefed.util;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.targetprep.TargetSetupError;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.List;

/**
 * Utility class that uses bundletool command line to install the .apks on deivce. Bundletool doc
 * link: https://developer.android.com/studio/command-line/bundletool The bundletool.jar is
 * downloaded from the unbundled module branch together with the module file.
 */
public class BundletoolUtil {
    private static final String GET_DEVICE_SPEC_OPTION = "get-device-spec";
    private static final String SPLITS_KEYWORD = "Splits";
    private static final String OUTPUT_DIR_FLAG = "--output-dir=";
    private static final String DEVICE_SPEC_OUTPUT_FLAG = "--output=";
    private static final String APKS_TO_EXTRACT_FLAG = "--apks=";
    private static final String DEVICE_SPEC_FLAG = "--device-spec=";
    private static final String DEVICE_ID_FLAG = "--device-id=";
    private static final String EXTRACT_APKS_OPTION = "extract-apks";
    private static final String INSTALL_APKS_OPTION = "install-apks";
    private static final String DEVICE_SPEC_FILE_EXTENSION = ".json";
    private static final long CMD_TIME_OUT = 10 * 6000; // 1 min

    private File mBundleToolFile;
    private IRunUtil mRunUtil;

    public BundletoolUtil(File bundletoolJar) {
        mBundleToolFile = bundletoolJar;
        mRunUtil = new RunUtil();
    }

    protected File getBundletoolFile() {
        return mBundleToolFile;
    }

    /**
     * Generates a JSON file for a connected device configuration.
     *
     * @param device the connected device
     * @return a {@link String} representing the path of the device specification file.
     */
    public String generateDeviceSpecFile(ITestDevice device) throws IOException {
        Path specFilePath =
                Paths.get(
                        getBundletoolFile().getParentFile().getAbsolutePath(),
                        device.getSerialNumber() + DEVICE_SPEC_FILE_EXTENSION);

        Files.deleteIfExists(specFilePath);

        String outputDirArg = DEVICE_SPEC_OUTPUT_FLAG + specFilePath.toString();

        String deviceIdArg = DEVICE_ID_FLAG + device.getSerialNumber();

        List<String> generateDeviceSpecCmd =
                new ArrayList<String>(
                        Arrays.asList(
                                "java",
                                "-jar",
                                getBundletoolFile().getAbsolutePath(),
                                GET_DEVICE_SPEC_OPTION,
                                outputDirArg,
                                deviceIdArg));

        if (getAdbPath() != null) {
            generateDeviceSpecCmd.add("--adb=" + getAdbPath());
        }

        CommandResult res =
                getRunUtil()
                        .runTimedCmd(
                                CMD_TIME_OUT,
                                generateDeviceSpecCmd.toArray(
                                        new String[generateDeviceSpecCmd.size()]));
        if (!CommandStatus.SUCCESS.equals(res.getStatus())) {
            CLog.e(
                    "Failed to generated device spec file. Cmd is %s. Error: %s.",
                    generateDeviceSpecCmd.toString(), res.getStderr());
            return null;
        }
        return specFilePath.toString();
    }

    /**
     * Extracts the split apk/apex from .apks. Renames the splits and stores the splits to the
     * directory where .apks stored. Returns the new directory that the splits stored.
     *
     * @param apks the apks that need to be extracted
     * @param deviceSpecPath the device spec file that bundletool uses to extract the apks
     * @param device the connected device
     * @param buildInfo build artifact information
     * @return a {@link File} that is the directory where the extracted apk(s)/apex live under
     */
    public File extractSplitsFromApks(
            File apks, String deviceSpecPath, ITestDevice device, IBuildInfo buildInfo) {
        // Extracts all split apk(s)/apex to where .apks stored.
        File destDir;
        String destDirPath = apks.getParentFile().getAbsolutePath();
        String[] helperArray = apks.getName().split("\\.");
        // The apks name pattern is "modulename + .apks"
        String moduleName = helperArray[helperArray.length - 2];
        destDir = new File(destDirPath, moduleName + SPLITS_KEYWORD);
        destDir.mkdir();

        // Extracts the splits to destDir.
        String outputPathArg = OUTPUT_DIR_FLAG + destDir.getAbsolutePath();
        String inputPathArg = APKS_TO_EXTRACT_FLAG + apks.getAbsolutePath();
        String deviceSpecArg = DEVICE_SPEC_FLAG + deviceSpecPath;
        String[] extractApkCmd =
                new String[] {
                    "java",
                    "-jar",
                    getBundletoolFile().getAbsolutePath(),
                    EXTRACT_APKS_OPTION,
                    inputPathArg,
                    outputPathArg,
                    deviceSpecArg
                };
        CommandResult res = getRunUtil().runTimedCmd(CMD_TIME_OUT, extractApkCmd);
        if (!CommandStatus.SUCCESS.equals(res.getStatus())) {
            FileUtil.recursiveDelete(destDir);
            CLog.e(
                    "Failed to extract split apk. Cmd: %s. Error: %s.",
                    Arrays.toString(extractApkCmd), res.getStderr());
            return null;
        }
        return destDir;
    }

    /**
     * Installs the apk .apks that using bundletool.
     *
     * @param apks the apks that need to be installed
     * @param device the connected device
     */
    public void installApks(File apks, ITestDevice device) throws TargetSetupError {
        String inputPathArg = "--apks=" + apks.getAbsolutePath();

        String deviceIdArg = DEVICE_ID_FLAG + device.getSerialNumber();

        List<String> installApksCmd =
                new ArrayList<String>(
                        Arrays.asList(
                                "java",
                                "-jar",
                                getBundletoolFile().getAbsolutePath(),
                                INSTALL_APKS_OPTION,
                                inputPathArg,
                                deviceIdArg));

        if (getAdbPath() != null) {
            installApksCmd.add("--adb=" + getAdbPath());
        }

        CommandResult res =
                getRunUtil()
                        .runTimedCmd(
                                CMD_TIME_OUT,
                                installApksCmd.toArray(new String[installApksCmd.size()]));
        if (!CommandStatus.SUCCESS.equals(res.getStatus())) {
            throw new TargetSetupError(
                    String.format(
                            "Failed to install split apk. Cmd: %s. Error: %s.",
                            installApksCmd.toString(), res.getStderr()),
                    device.getDeviceDescriptor());
        }
        CLog.i("%s is installed successfully", apks.getName());
        return;
    }

    @VisibleForTesting
    protected IRunUtil getRunUtil() {
        return mRunUtil;
    }

    @VisibleForTesting
    protected String getAdbPath() {
        String adbPath = GlobalConfiguration.getDeviceManagerInstance().getAdbPath();
        // No explicit adb path passed from device manager.
        if (!new File(adbPath).exists()) {
            return null;
        }
        return adbPath;
    }
}
