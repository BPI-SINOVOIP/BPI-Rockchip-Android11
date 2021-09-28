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

package com.android.tradefed.device;

import com.android.ddmlib.IDevice;
import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.build.BuildInfoKey.BuildInfoFileKey;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.cloud.GceAvdInfo;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ITestLoggerReceiver;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.TarUtil;
import com.android.tradefed.util.ZipUtil;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Strings;
import com.google.common.net.HostAndPort;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** The class for local virtual devices running on TradeFed host. */
public class LocalAndroidVirtualDevice extends RemoteAndroidDevice implements ITestLoggerReceiver {

    private static final int INVALID_PORT = 0;

    // Environment variables.
    private static final String ANDROID_HOST_OUT = "ANDROID_HOST_OUT";
    private static final String TMPDIR = "TMPDIR";

    // The name of the GZIP file containing launch_cvd and stop_cvd.
    private static final String CVD_HOST_PACKAGE_NAME = "cvd-host_package.tar.gz";

    private static final String ACLOUD_CVD_TEMP_DIR_NAME = "acloud_cvd_temp";
    private static final String CUTTLEFISH_RUNTIME_DIR_NAME = "cuttlefish_runtime";

    private ITestLogger mTestLogger = null;

    // Temporary directories for images and tools.
    private File mImageDir = null;
    private File mHostPackageDir = null;
    private List<File> mTempDirs = new ArrayList<File>();

    private GceAvdInfo mGceAvdInfo = null;

    public LocalAndroidVirtualDevice(
            IDevice device, IDeviceStateMonitor stateMonitor, IDeviceMonitor allocationMonitor) {
        super(device, stateMonitor, allocationMonitor);
    }

    /** Execute common setup procedure and launch the virtual device. */
    @Override
    public void preInvocationSetup(IBuildInfo info)
            throws TargetSetupError, DeviceNotAvailableException {
        // The setup method in super class does not require the device to be online.
        super.preInvocationSetup(info);

        createTempDirs(info);

        CommandResult result = null;
        File report = null;
        try {
            report = FileUtil.createTempFile("report", ".json");
            result = acloudCreate(report, getOptions());
            loadAvdInfo(report);
        } catch (IOException ex) {
            throw new TargetSetupError(
                    "Cannot create acloud report file.", ex, getDeviceDescriptor());
        } finally {
            FileUtil.deleteFile(report);
        }
        if (!CommandStatus.SUCCESS.equals(result.getStatus())) {
            throw new TargetSetupError(
                    String.format("Cannot execute acloud command. stderr:\n%s", result.getStderr()),
                    getDeviceDescriptor());
        }

        HostAndPort hostAndPort = mGceAvdInfo.hostAndPort();
        replaceStubDevice(hostAndPort.toString());

        RecoveryMode previousMode = getRecoveryMode();
        try {
            setRecoveryMode(RecoveryMode.NONE);
            if (!adbTcpConnect(hostAndPort.getHost(), Integer.toString(hostAndPort.getPort()))) {
                throw new TargetSetupError(
                        String.format("Cannot connect to %s.", hostAndPort), getDeviceDescriptor());
            }
            waitForDeviceAvailable();
        } finally {
            setRecoveryMode(previousMode);
        }
    }

    /** Execute common tear-down procedure and stop the virtual device. */
    @Override
    public void postInvocationTearDown(Throwable exception) {
        TestDeviceOptions options = getOptions();
        HostAndPort hostAndPort = getHostAndPortFromAvdInfo();
        String instanceName = (mGceAvdInfo != null ? mGceAvdInfo.instanceName() : null);
        try {
            if (!options.shouldSkipTearDown() && hostAndPort != null) {
                if (!adbTcpDisconnect(
                        hostAndPort.getHost(), Integer.toString(hostAndPort.getPort()))) {
                    CLog.e("Cannot disconnect from %s", hostAndPort.toString());
                }
            }

            if (!options.shouldSkipTearDown() && instanceName != null) {
                CommandResult result = acloudDelete(instanceName, options);
                if (!CommandStatus.SUCCESS.equals(result.getStatus())) {
                    CLog.e("Cannot stop the virtual device.");
                }
            } else {
                CLog.i("Skip stopping the virtual device.");
            }

            if (instanceName != null) {
                reportInstanceLogs(instanceName);
            }
        } finally {
            restoreStubDevice();

            if (!options.shouldSkipTearDown()) {
                deleteTempDirs();
            } else {
                CLog.i(
                        "Skip deleting the temporary directories.\n"
                                + "Address: %s\nName: %s\nHost package: %s\nImage: %s",
                        hostAndPort, instanceName, mHostPackageDir, mImageDir);
                mTempDirs.clear();
                mHostPackageDir = null;
                mImageDir = null;
            }

            mGceAvdInfo = null;

            super.postInvocationTearDown(exception);
        }
    }

    @Override
    public void setTestLogger(ITestLogger testLogger) {
        mTestLogger = testLogger;
    }

    /**
     * Extract a file if the format is tar.gz or zip.
     *
     * @param file the file to be extracted.
     * @return a temporary directory containing the extracted content if the file is an archive;
     *     otherwise return the input file.
     * @throws IOException if the file cannot be extracted.
     */
    private File extractArchive(File file) throws IOException {
        if (file.isDirectory()) {
            return file;
        }
        if (TarUtil.isGzip(file)) {
            file = TarUtil.extractTarGzipToTemp(file, file.getName());
            mTempDirs.add(file);
        } else if (ZipUtil.isZipFileValid(file, false)) {
            file = ZipUtil.extractZipToTemp(file, file.getName());
            mTempDirs.add(file);
        } else {
            CLog.w("Cannot extract %s.", file);
        }
        return file;
    }

    /** Find host package in build info and extract to a temporary directory. */
    private File findHostPackage(IBuildInfo buildInfo) throws TargetSetupError {
        File hostPackageDir = null;
        File hostPackage = buildInfo.getFile(CVD_HOST_PACKAGE_NAME);
        if (hostPackage != null) {
            try {
                hostPackageDir = extractArchive(hostPackage);
            } catch (IOException ex) {
                throw new TargetSetupError(
                        "Cannot extract host package.", ex, getDeviceDescriptor());
            }
        }
        if (hostPackageDir == null) {
            String androidHostOut = System.getenv(ANDROID_HOST_OUT);
            if (!Strings.isNullOrEmpty(androidHostOut)) {
                CLog.i(
                        "Use the host tools in %s as the build info does not provide host package.",
                        androidHostOut);
                hostPackageDir = new File(androidHostOut);
            }
        }
        if (hostPackageDir == null || !hostPackageDir.isDirectory()) {
            throw new TargetSetupError(
                    String.format(
                            "Cannot find %s in build info and %s.",
                            CVD_HOST_PACKAGE_NAME, ANDROID_HOST_OUT),
                    getDeviceDescriptor());
        }
        FileUtil.chmodRWXRecursively(new File(hostPackageDir, "bin"));
        return hostPackageDir;
    }

    /** Find device images in build info and extract to a temporary directory. */
    private File findDeviceImages(IBuildInfo buildInfo) throws TargetSetupError {
        File imageZip = buildInfo.getFile(BuildInfoFileKey.DEVICE_IMAGE);
        if (imageZip == null) {
            throw new TargetSetupError(
                    "Cannot find image zip in build info.", getDeviceDescriptor());
        }
        try {
            return extractArchive(imageZip);
        } catch (IOException ex) {
            throw new TargetSetupError("Cannot extract image zip.", ex, getDeviceDescriptor());
        }
    }

    /** Get the necessary files to create the instance. */
    void createTempDirs(IBuildInfo info) throws TargetSetupError {
        try {
            mHostPackageDir = findHostPackage(info);
            mImageDir = findDeviceImages(info);
        } catch (TargetSetupError ex) {
            deleteTempDirs();
            throw ex;
        }
    }

    /** Delete all temporary directories. */
    @VisibleForTesting
    void deleteTempDirs() {
        for (File tempDir : mTempDirs) {
            FileUtil.recursiveDelete(tempDir);
        }
        mTempDirs.clear();
        mImageDir = null;
        mHostPackageDir = null;
    }

    /**
     * Change the initial serial number of {@link StubLocalAndroidVirtualDevice}.
     *
     * @param newSerialNumber the serial number of the new stub device.
     * @throws TargetSetupError if the original device type is not expected.
     */
    private void replaceStubDevice(String newSerialNumber) throws TargetSetupError {
        IDevice device = getIDevice();
        if (!StubLocalAndroidVirtualDevice.class.equals(device.getClass())) {
            throw new TargetSetupError(
                    "Unexpected device type: " + device.getClass(), getDeviceDescriptor());
        }
        setIDevice(new StubLocalAndroidVirtualDevice(newSerialNumber));
        setFastbootEnabled(false);
    }

    /** Restore the {@link StubLocalAndroidVirtualDevice} with the initial serial number. */
    private void restoreStubDevice() {
        setIDevice(new StubLocalAndroidVirtualDevice(getInitialSerial()));
        setFastbootEnabled(false);
    }

    private static void addLogLevelToAcloudCommand(List<String> command, LogLevel logLevel) {
        if (LogLevel.VERBOSE.equals(logLevel)) {
            command.add("-v");
        } else if (LogLevel.DEBUG.equals(logLevel)) {
            command.add("-vv");
        }
    }

    private CommandResult acloudCreate(File report, TestDeviceOptions options) {
        CommandResult result = null;

        File acloud = options.getAvdDriverBinary();
        if (acloud == null || !acloud.isFile()) {
            CLog.e("Specified AVD driver binary is not a file.");
            result = new CommandResult(CommandStatus.EXCEPTION);
            result.setStderr("Specified AVD driver binary is not a file.");
            return result;
        }
        acloud.setExecutable(true);

        for (int attempt = 0; attempt < options.getGceMaxAttempt(); attempt++) {
            result =
                    acloudCreate(
                            options.getGceCmdTimeout(),
                            acloud,
                            report,
                            options.getGceDriverLogLevel(),
                            options.getGceDriverParams());
            if (CommandStatus.SUCCESS.equals(result.getStatus())) {
                break;
            }
            CLog.w(
                    "Failed to start local virtual instance with attempt: %d; command status: %s",
                    attempt, result.getStatus());
        }
        return result;
    }

    private CommandResult acloudCreate(
            long timeout,
            File acloud,
            File report,
            LogLevel logLevel,
            List<String> args) {
        IRunUtil runUtil = createRunUtil();
        // The command creates the instance directory under TMPDIR.
        runUtil.setEnvVariable(TMPDIR, getTmpDir().getAbsolutePath());

        List<String> command =
                new ArrayList<String>(
                        Arrays.asList(
                                acloud.getAbsolutePath(),
                                "create",
                                "--local-instance",
                                "--local-image",
                                mImageDir.getAbsolutePath(),
                                "--local-tool",
                                mHostPackageDir.getAbsolutePath(),
                                "--report_file",
                                report.getAbsolutePath(),
                                "--no-autoconnect",
                                "--yes",
                                "--skip-pre-run-check"));
        addLogLevelToAcloudCommand(command, logLevel);
        command.addAll(args);

        CommandResult result = runUtil.runTimedCmd(timeout, command.toArray(new String[0]));
        CLog.i("acloud create stdout:\n%s", result.getStdout());
        CLog.i("acloud create stderr:\n%s", result.getStderr());
        return result;
    }

    /**
     * Get valid host and port from mGceAvdInfo.
     *
     * @return {@link HostAndPort} if the port is valid; null otherwise.
     */
    private HostAndPort getHostAndPortFromAvdInfo() {
        if (mGceAvdInfo == null) {
            return null;
        }
        HostAndPort hostAndPort = mGceAvdInfo.hostAndPort();
        if (hostAndPort == null
                || !hostAndPort.hasPort()
                || hostAndPort.getPort() == INVALID_PORT) {
            return null;
        }
        return hostAndPort;
    }

    /** Initialize instance name, host address, and port from an acloud report file. */
    private void loadAvdInfo(File report) throws TargetSetupError {
        mGceAvdInfo = GceAvdInfo.parseGceInfoFromFile(report, getDeviceDescriptor(), INVALID_PORT);
        if (mGceAvdInfo == null) {
            throw new TargetSetupError("Cannot read acloud report file.", getDeviceDescriptor());
        }

        if (Strings.isNullOrEmpty(mGceAvdInfo.instanceName())) {
            throw new TargetSetupError("No instance name in acloud report.", getDeviceDescriptor());
        }

        if (getHostAndPortFromAvdInfo() == null) {
            throw new TargetSetupError("No port in acloud report.", getDeviceDescriptor());
        }

        if (!GceAvdInfo.GceStatus.SUCCESS.equals(mGceAvdInfo.getStatus())) {
            throw new TargetSetupError(
                    "Cannot launch virtual device: " + mGceAvdInfo.getErrors(),
                    getDeviceDescriptor());
        }
    }

    private CommandResult acloudDelete(String instanceName, TestDeviceOptions options) {
        File acloud = options.getAvdDriverBinary();
        if (acloud == null || !acloud.isFile()) {
            CLog.e("Specified AVD driver binary is not a file.");
            return new CommandResult(CommandStatus.EXCEPTION);
        }
        acloud.setExecutable(true);

        IRunUtil runUtil = createRunUtil();
        runUtil.setEnvVariable(TMPDIR, getTmpDir().getAbsolutePath());

        List<String> command =
                new ArrayList<String>(
                        Arrays.asList(
                                acloud.getAbsolutePath(),
                                "delete",
                                "--local-only",
                                "--instance-names",
                                instanceName));
        addLogLevelToAcloudCommand(command, options.getGceDriverLogLevel());

        CommandResult result =
                runUtil.runTimedCmd(options.getGceCmdTimeout(), command.toArray(new String[0]));
        CLog.i("acloud delete stdout:\n%s", result.getStdout());
        CLog.i("acloud delete stderr:\n%s", result.getStderr());
        return result;
    }

    private void reportInstanceLogs(String instanceName) {
        if (mTestLogger == null) {
            return;
        }
        File instanceDir =
                FileUtil.getFileForPath(
                        getTmpDir(),
                        ACLOUD_CVD_TEMP_DIR_NAME,
                        instanceName,
                        CUTTLEFISH_RUNTIME_DIR_NAME);
        reportInstanceLog(new File(instanceDir, "kernel.log"), LogDataType.KERNEL_LOG);
        reportInstanceLog(new File(instanceDir, "logcat"), LogDataType.LOGCAT);
        reportInstanceLog(new File(instanceDir, "launcher.log"), LogDataType.TEXT);
        reportInstanceLog(new File(instanceDir, "cuttlefish_config.json"), LogDataType.TEXT);
    }

    private void reportInstanceLog(File file, LogDataType type) {
        if (file.exists()) {
            try (InputStreamSource source = new FileInputStreamSource(file)) {
                mTestLogger.testLog(file.getName(), type, source);
            }
        } else {
            CLog.w("%s doesn't exist.", file.getAbsolutePath());
        }
    }

    @VisibleForTesting
    IRunUtil createRunUtil() {
        return new RunUtil();
    }

    @VisibleForTesting
    File getTmpDir() {
        return new File(System.getProperty("java.io.tmpdir"));
    }
}
