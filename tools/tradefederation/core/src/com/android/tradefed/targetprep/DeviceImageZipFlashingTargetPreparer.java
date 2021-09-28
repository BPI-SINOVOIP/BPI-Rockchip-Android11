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
package com.android.tradefed.targetprep;

import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.ZipUtil2;

import org.apache.commons.compress.archivers.zip.ZipFile;

import java.io.File;
import java.io.IOException;

/**
 * A target preparer that flashes the device with device images provided via a specific format.
 *
 * <p>High level requirements for the device image format:
 *
 * <ul>
 *   <li>Device image file must be a zip file
 *   <li>The zip file must include a flash-all.sh script at the root
 *   <li>The script must assume that the device is in userspace visible to <code>adb devices</code>
 *   <li>The rest of the zip file will be extracted into the same location as script with the same
 *       directory layout, and the script may make reference to any files packaged in the zip via
 *       relative path
 *   <li>After flashing, the script must return the device to the same state
 *   <li>An environment variable <code>ANDROID_SERIAL</code> will be set to device serial number as
 *       part of the execution environment
 *   <li>The script may assume that it has <code>adb</code> and <code>fastboot</code> on PATH
 * </ul>
 *
 * This target preparer will unpack the device image zip file and execute the enclosed <code>flash-
 * all.sh</code> under the assumptions outline in requirements above.
 */
public class DeviceImageZipFlashingTargetPreparer extends DeviceUpdateTargetPreparer {

    private static final String ANDROID_SERIAL_ENV = "ANDROID_SERIAL";

    @Option(name = "device-image-zip", description = "the device image zip file to be flashed")
    private File mDeviceImageZip = null;

    @Option(
        name = "flashing-timeout",
        description = "timeout for flashing the device images",
        isTimeVal = true
    )
    // defaults to 10m: assuming USB 2.0 transfer speed, concurrency and some buffer
    private long mFlashingTimeout = 10 * 60 * 1000;

    @Option(
        name = "flashing-script",
        description =
                "the name of the flashing script bundled within " + "the device image zip file"
    )
    private String mFlashingScript = "flash-all.sh";

    /** {@inheritDoc} */
    @Override
    protected File getDeviceUpdateImage() {
        return mDeviceImageZip;
    }

    /** No-op */
    @Override
    protected void preUpdateActions(File deviceUpdateImage, ITestDevice device)
            throws DeviceNotAvailableException, TargetSetupError {}

    /** No-op */
    @Override
    protected void postUpdateActions(File deviceUpdateImage, ITestDevice device)
            throws DeviceNotAvailableException, TargetSetupError {}

    /** Expands the device image update zip and calls the enclosed flashing script */
    @Override
    protected void performDeviceUpdate(File deviceUpdateImage, ITestDevice device)
            throws DeviceNotAvailableException, TargetSetupError {
        // first unzip the package
        File extractedImage = null;
        try {
            extractedImage = extractZip(device, getDeviceUpdateImage());
            File flashingScript = new File(extractedImage, mFlashingScript);
            if (!flashingScript.exists()) {
                throw new TargetSetupError(
                        String.format(
                                "Flashing script \"%s\" not found inside " + "the device image zip",
                                mFlashingScript),
                        device.getDeviceDescriptor());
            }
            IRunUtil runUtil = new RunUtil();
            runUtil.setEnvVariable(ANDROID_SERIAL_ENV, device.getSerialNumber());
            runUtil.setWorkingDir(extractedImage);
            CLog.i("Starting flashing on %s", device.getSerialNumber());
            CommandResult result =
                    runUtil.runTimedCmd(
                            mFlashingTimeout, "bash", "-x", flashingScript.getAbsolutePath());
            CommandStatus status = result.getStatus();
            StringBuilder sb = new StringBuilder();
            sb.append(
                    String.format(
                            "Flashing command finished with status: %s\n", status.toString()));
            sb.append(String.format("Flashing command stdout:\n%s\n", result.getStdout()));
            sb.append(String.format("Flashing command stderr:\n%s\n", result.getStderr()));
            if (!CommandStatus.SUCCESS.equals(status)) {
                CLog.w(sb.toString());
            } else {
                CLog.v(sb.toString());
            }
            String message =
                    String.format(
                            "Flashing script failed (status: %s), "
                                    + "check host logs above for details",
                            status.toString());
            switch (status) {
                case SUCCESS:
                    break;
                case FAILED:
                    throw new TargetSetupError(message, device.getDeviceDescriptor());
                case EXCEPTION:
                    throw new TargetSetupError(message, device.getDeviceDescriptor());
                case TIMED_OUT:
                    throw new TargetSetupError(message, device.getDeviceDescriptor());
                default:
                    throw new IllegalStateException("Failsafe: not expected");
            }
        } finally {
            FileUtil.recursiveDelete(extractedImage);
        }
    }

    /**
     * Extract a zip file and return temporary directory with contents.
     *
     * @param device the {@link ITestDevice}
     * @param zip {@link File} to unzip
     * @throws TargetSetupError if any operation fails
     */
    private static File extractZip(ITestDevice device, File zip) throws TargetSetupError {
        ZipFile zFile = null;
        File outputDir;
        try {
            zFile = new ZipFile(zip);
            File fastbootTmpDir =
                    GlobalConfiguration.getInstance().getHostOptions().getFastbootTmpDir();
            outputDir =
                    FileUtil.createTempDir(
                            DeviceImageZipFlashingTargetPreparer.class.getSimpleName()
                                    + "-tmp-files",
                            fastbootTmpDir);
            ZipUtil2.extractZip(zFile, outputDir);
        } catch (IOException | IllegalStateException exception) {
            throw new TargetSetupError(
                    exception.getMessage(), exception, device.getDeviceDescriptor());
        } finally {
            ZipUtil2.closeZip(zFile);
        }
        return outputDir;
    }
}
