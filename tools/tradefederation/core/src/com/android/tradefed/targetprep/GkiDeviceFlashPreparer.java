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
package com.android.tradefed.targetprep;

import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.DeviceUnresponsiveException;
import com.android.tradefed.device.IDeviceManager;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.ITestDevice.RecoveryMode;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.ZipUtil2;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.TimeUnit;

/**
 * A target preparer that flash the device with android common kernel generic image. Please see
 * https://source.android.com/devices/architecture/kernel/android-common for details.
 */
@OptionClass(alias = "gki-device-flash-preparer")
public class GkiDeviceFlashPreparer extends BaseTargetPreparer {

    private static final String GKI_BOOT_IMG = "gki_boot.img";
    private static final String MKBOOTIMG = "mkbootimg";
    private static final String OTATOOLS_ZIP = "otatools.zip";
    private static final String RAMDISK_RECOVERY_IMG = "ramdisk-recovery.img";
    private static final String KERNEL_IMAGE = "Image.gz";
    private static final String VENDOR_BOOT_IMG = "vendor_boot.img";
    private static final String DTBO_IMG = "dtbo.img";
    // Wait time for device state to stablize in millisecond
    private static final int STATE_STABLIZATION_WAIT_TIME = 60000;

    @Option(
            name = "device-boot-time",
            description = "max time to wait for device to boot. Set as 5 minutes by default",
            isTimeVal = true)
    private long mDeviceBootTime = 5 * 60 * 1000;

    @Option(
            name = "boot-image-file-name",
            description =
                    "The boot image file name to search for if gki_boot.img is provided in "
                            + "BuildInfo and the provided file is a zip file or directory, for "
                            + "example boot-5.4.img. By default when gki_boot.img is provided in "
                            + "BuildInfo with a zip file or file directory, the target preparer "
                            + "will use the first found file that matches boot(.*).img.")
    private String mBootImageFileName = "boot(.*).img";

    private File mBootImg = null;

    /** {@inheritDoc} */
    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        ITestDevice device = testInfo.getDevice();
        IDeviceBuildInfo buildInfo = (IDeviceBuildInfo) testInfo.getBuildInfo();

        File tmpDir = null;
        try {
            tmpDir = FileUtil.createTempDir("gki_preparer");
            validateGkiBootImg(device, buildInfo, tmpDir);
            flashGki(device, buildInfo);
        } catch (IOException ioe) {
            throw new TargetSetupError(ioe.getMessage(), ioe, device.getDeviceDescriptor());
        } finally {
            FileUtil.recursiveDelete(tmpDir);
        }
        // Wait some time after flashing the image.
        getRunUtil().sleep(STATE_STABLIZATION_WAIT_TIME);
        device.rebootUntilOnline();
        if (device.enableAdbRoot()) {
            device.setDate(null);
        }
        try {
            device.setRecoveryMode(RecoveryMode.AVAILABLE);
            device.waitForDeviceAvailable(mDeviceBootTime);
        } catch (DeviceUnresponsiveException e) {
            // assume this is a build problem
            throw new DeviceFailedToBootError(
                    String.format(
                            "Device %s did not become available after flashing GKI. Exception: %s",
                            device.getSerialNumber(), e),
                    device.getDeviceDescriptor());
        }
        device.postBootSetup();
        CLog.i("Device update completed on %s", device.getDeviceDescriptor());
    }

    /**
     * Get a reference to the {@link IDeviceManager}
     *
     * @return the {@link IDeviceManager} to use
     */
    @VisibleForTesting
    IDeviceManager getDeviceManager() {
        return GlobalConfiguration.getDeviceManagerInstance();
    }

    /**
     * Get the {@link IRunUtil} instance to use.
     *
     * @return the {@link IRunUtil} to use
     */
    @VisibleForTesting
    protected IRunUtil getRunUtil() {
        return RunUtil.getDefault();
    }

    /**
     * Flash GKI images.
     *
     * @param device the {@link ITestDevice}
     * @param buildInfo the {@link IDeviceBuildInfo} the device build info
     * @throws TargetSetupError, DeviceNotAvailableException, IOException
     */
    private void flashGki(ITestDevice device, IDeviceBuildInfo buildInfo)
            throws TargetSetupError, DeviceNotAvailableException {
        IDeviceManager deviceManager = getDeviceManager();
        device.waitForDeviceOnline();
        device.rebootIntoBootloader();
        long start = System.currentTimeMillis();
        deviceManager.takeFlashingPermit();
        CLog.v(
                "Flashing permit obtained after %ds",
                TimeUnit.MILLISECONDS.toSeconds((System.currentTimeMillis() - start)));
        // Don't allow interruptions during flashing operations.
        getRunUtil().allowInterrupt(false);
        try {
            if (buildInfo.getFile(VENDOR_BOOT_IMG) != null) {
                executeFastbootCmd(
                        device,
                        "flash",
                        "vendor_boot",
                        buildInfo.getFile(VENDOR_BOOT_IMG).getAbsolutePath());
            }
            if (buildInfo.getFile(DTBO_IMG) != null) {
                executeFastbootCmd(
                        device, "flash", "dtbo", buildInfo.getFile(DTBO_IMG).getAbsolutePath());
            }
            executeFastbootCmd(device, "flash", "boot", mBootImg.getAbsolutePath());
        } finally {
            deviceManager.returnFlashingPermit();
            // Allow interruption at the end no matter what.
            getRunUtil().allowInterrupt(true);
            CLog.v(
                    "Flashing permit returned after %ds",
                    TimeUnit.MILLISECONDS.toSeconds((System.currentTimeMillis() - start)));
        }
    }

    /**
     * Validate GKI boot image is expected. (Obsoleted. Please call with tmpDir provided)
     *
     * @param device the {@link ITestDevice}
     * @param buildInfo the {@link IDeviceBuildInfo} the device build info
     * @throws TargetSetupError if there is no valid gki boot.img
     */
    public void validateGkiBootImg(ITestDevice device, IDeviceBuildInfo buildInfo)
            throws TargetSetupError {
        throw new TargetSetupError(
                "Obsoleted. Please use validateGkiBootImg(ITestDevice, IDeviceBuildInfo, File)",
                device.getDeviceDescriptor());
    }

    /**
     * Validate GKI boot image is expected. Throw exception if there is no valid boot.img.
     *
     * @param device the {@link ITestDevice}
     * @param buildInfo the {@link IDeviceBuildInfo} the device build info
     * @param tmpDir the temporary directory {@link File}
     * @throws TargetSetupError if there is no valid gki boot.img
     */
    @VisibleForTesting
    protected void validateGkiBootImg(ITestDevice device, IDeviceBuildInfo buildInfo, File tmpDir)
            throws TargetSetupError {
        if (buildInfo.getFile(GKI_BOOT_IMG) != null && mBootImageFileName != null) {
            mBootImg =
                    getRequestedFile(
                            device, mBootImageFileName, buildInfo.getFile(GKI_BOOT_IMG), tmpDir);
            return;
        }
        if (buildInfo.getFile(KERNEL_IMAGE) == null) {
            throw new TargetSetupError(
                    KERNEL_IMAGE + " is not provided. Can not generate GKI boot.img.",
                    device.getDeviceDescriptor());
        }
        if (buildInfo.getFile(RAMDISK_RECOVERY_IMG) == null) {
            throw new TargetSetupError(
                    RAMDISK_RECOVERY_IMG + " is not provided. Can not generate GKI boot.img.",
                    device.getDeviceDescriptor());
        }
        if (buildInfo.getFile(OTATOOLS_ZIP) == null) {
            throw new TargetSetupError(
                    OTATOOLS_ZIP + " is not provided. Can not generate GKI boot.img.",
                    device.getDeviceDescriptor());
        }
        try {
            File mkbootimg =
                    getRequestedFile(device, MKBOOTIMG, buildInfo.getFile(OTATOOLS_ZIP), tmpDir);
            mBootImg = FileUtil.createTempFile("boot", ".img", tmpDir);
            String cmd =
                    String.format(
                            "%s --kernel %s --header_version 3 --base 0x00000000 "
                                    + "--pagesize 4096 --ramdisk %s -o %s",
                            mkbootimg.getAbsolutePath(),
                            buildInfo.getFile(KERNEL_IMAGE),
                            buildInfo.getFile(RAMDISK_RECOVERY_IMG),
                            mBootImg.getAbsolutePath());
            executeHostCommand(device, cmd);
            CLog.i("The GKI boot.img is of size %d", mBootImg.length());
            if (mBootImg.length() == 0) {
                throw new TargetSetupError(
                        "The mkbootimg tool didn't generate a valid boot.img.",
                        device.getDeviceDescriptor());
            }
        } catch (IOException e) {
            throw new TargetSetupError(
                    "Fail to generate GKI boot.img.", e, device.getDeviceDescriptor());
        }
    }

    /**
     * Flash device images.
     *
     * @param device the {@link ITestDevice}
     * @param buildInfo the {@link IDeviceBuildInfo} the device build info
     * @throws TargetSetupError, DeviceNotAvailableException
     */
    private void flashDeviceImage(ITestDevice device, IDeviceBuildInfo buildInfo)
            throws TargetSetupError, DeviceNotAvailableException {
        IDeviceManager deviceManager = getDeviceManager();
        long start = System.currentTimeMillis();
        deviceManager.takeFlashingPermit();
        CLog.v(
                "Flashing permit obtained after %ds",
                TimeUnit.MILLISECONDS.toSeconds((System.currentTimeMillis() - start)));
        // don't allow interruptions during flashing operations.
        getRunUtil().allowInterrupt(false);
        try {
            executeFastbootCmd(
                    device,
                    "--skip-reboot",
                    "--disable-verity",
                    "update",
                    buildInfo.getDeviceImageFile().getAbsolutePath());
        } finally {
            // Allow interruption at the end no matter what.
            getRunUtil().allowInterrupt(true);
            deviceManager.returnFlashingPermit();
            CLog.v(
                    "Flashing permit returned after %ds",
                    TimeUnit.MILLISECONDS.toSeconds((System.currentTimeMillis() - start)));
        }
    }

    /**
     * Helper method to execute host command.
     *
     * @param device the {@link ITestDevice}
     * @param command the command string
     * @throws TargetSetupError, DeviceNotAvailableException
     */
    private void executeHostCommand(ITestDevice device, final String command)
            throws TargetSetupError {
        final CommandResult result = getRunUtil().runTimedCmd(300000L, command.split("\\s+"));
        switch (result.getStatus()) {
            case SUCCESS:
                CLog.i(
                        "Command %s finished successfully, stdout = [%s].",
                        command, result.getStdout());
                break;
            case FAILED:
                throw new TargetSetupError(
                        String.format(
                                "Command %s failed, stdout = [%s], stderr = [%s].",
                                command, result.getStdout(), result.getStderr()),
                        device.getDeviceDescriptor());
            case TIMED_OUT:
                throw new TargetSetupError(
                        String.format("Command %s timed out.", command),
                        device.getDeviceDescriptor());
            case EXCEPTION:
                throw new TargetSetupError(
                        String.format("Exception occurred when running command %s.", command),
                        device.getDeviceDescriptor());
        }
    }

    /**
     * Get the requested file from the source file (zip or folder) by requested file name.
     *
     * <p>The provided source file can be a zip file. The method will unzip it to tempary directory
     * and find the requested file by the provided file name.
     *
     * <p>The provided source file can be a file folder. The method will find the requestd file by
     * the provided file name.
     *
     * @param device the {@link ITestDevice}
     * @param requestedFileName the requeste file name String
     * @param sourceFile the source file
     * @return the file that is specified by the requested file name
     * @throws TargetSetupError
     */
    private File getRequestedFile(
            ITestDevice device, String requestedFileName, File sourceFile, File tmpDir)
            throws TargetSetupError {
        File requestedFile = null;
        if (sourceFile.getName().endsWith(".zip")) {
            try {
                File destDir =
                        FileUtil.createTempDir(FileUtil.getBaseName(sourceFile.getName()), tmpDir);
                ZipUtil2.extractZip(sourceFile, destDir);
                requestedFile = FileUtil.findFile(destDir, requestedFileName);
            } catch (IOException e) {
                throw new TargetSetupError(e.getMessage(), e, device.getDeviceDescriptor());
            }
        } else if (sourceFile.isDirectory()) {
            requestedFile = FileUtil.findFile(sourceFile, requestedFileName);
        } else {
            requestedFile = sourceFile;
        }
        if (requestedFile == null || !requestedFile.exists()) {
            throw new TargetSetupError(
                    String.format(
                            "Requested file with file_name %s does not exist in provided %s.",
                            requestedFileName, sourceFile),
                    device.getDeviceDescriptor());
        }
        return requestedFile;
    }

    /**
     * Helper method to execute a fastboot command.
     *
     * @param device the {@link ITestDevice} to execute command on
     * @param cmdArgs the arguments to provide to fastboot
     * @return String the stderr output from command if non-empty. Otherwise returns the stdout Some
     *     fastboot commands are weird in that they dump output to stderr on success case
     * @throws DeviceNotAvailableException if device is not available
     * @throws TargetSetupError if fastboot command fails
     */
    private String executeFastbootCmd(ITestDevice device, String... cmdArgs)
            throws DeviceNotAvailableException, TargetSetupError {
        CLog.i("Execute fastboot command %s on %s", cmdArgs, device.getSerialNumber());
        CommandResult result = device.executeLongFastbootCommand(cmdArgs);
        CLog.v("fastboot stdout: " + result.getStdout());
        CLog.v("fastboot stderr: " + result.getStderr());
        CommandStatus cmdStatus = result.getStatus();
        // fastboot command line output is in stderr even for successful run
        if (result.getStderr().contains("FAILED")) {
            // if output contains "FAILED", just override to failure
            cmdStatus = CommandStatus.FAILED;
        }
        if (cmdStatus != CommandStatus.SUCCESS) {
            throw new TargetSetupError(
                    String.format(
                            "fastboot command %s failed in device %s. stdout: %s, stderr: %s",
                            cmdArgs[0],
                            device.getSerialNumber(),
                            result.getStdout(),
                            result.getStderr()),
                    device.getDeviceDescriptor());
        }
        if (result.getStderr().length() > 0) {
            return result.getStderr();
        } else {
            return result.getStdout();
        }
    }
}
