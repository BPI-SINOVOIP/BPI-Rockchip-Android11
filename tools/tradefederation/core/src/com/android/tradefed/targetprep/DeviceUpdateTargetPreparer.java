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
import com.android.tradefed.device.DeviceUnresponsiveException;
import com.android.tradefed.device.IDeviceManager;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.ITestDevice.RecoveryMode;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.error.InfraErrorIdentifier;

import java.io.File;
import java.util.concurrent.TimeUnit;

/**
 * An abstract {@link ITargetPreparer} that takes care of common steps around updating devices with
 * a device image file from an external source (as opposed to a build service). The actual update
 * mechanism is delegated to implementor of subclasses.
 */
public abstract class DeviceUpdateTargetPreparer extends DeviceBuildInfoBootStrapper {

    @Option(
        name = "device-boot-time",
        description = "max time to wait for device to boot.",
        isTimeVal = true
    )
    private long mDeviceBootTime = 5 * 60 * 1000;

    @Option(
        name = "bootstrap-build-info",
        description =
                "whether build info should be"
                        + "bootstrapped based on device attributes after flashing"
    )
    private boolean mBootStrapBuildInfo = true;

    /** {@inheritDoc} */
    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        ITestDevice device = testInfo.getDevice();
        File deviceUpdateImage = getDeviceUpdateImage();
        if (deviceUpdateImage == null) {
            CLog.i("No device image zip file provided, assuming no-op; skipping ...");
            return;
        }
        if (!deviceUpdateImage.exists()) {
            throw new TargetSetupError(
                    "Device image file not found: " + deviceUpdateImage.getAbsolutePath(),
                    device.getDeviceDescriptor(),
                    InfraErrorIdentifier.ARTIFACT_NOT_FOUND);
        }
        preUpdateActions(deviceUpdateImage, device);
        // flashing concurrency control
        long start = System.currentTimeMillis();
        IDeviceManager deviceManager = GlobalConfiguration.getDeviceManagerInstance();
        deviceManager.takeFlashingPermit();
        CLog.v(
                "Flashing permit obtained after %ds",
                TimeUnit.MILLISECONDS.toSeconds((System.currentTimeMillis() - start)));
        try {
            performDeviceUpdate(deviceUpdateImage, device);
        } finally {
            CLog.v(
                    "Flashing finished after %ds",
                    TimeUnit.MILLISECONDS.toSeconds((System.currentTimeMillis() - start)));
            deviceManager.returnFlashingPermit();
        }
        postUpdateActions(deviceUpdateImage, device);
        CLog.i(
                "Flashing completed successfully on %s, waiting for device to boot up.",
                device.getSerialNumber());
        device.waitForDeviceOnline();
        // device may lose date setting if wiped, update with host side date in case anything on
        // device side malfunction with an invalid date
        if (device.getOptions().isEnableAdbRoot()) {
            boolean rootEnabled = device.enableAdbRoot();
            if (rootEnabled) {
                device.setDate(null);
            }
        }
        try {
            device.setRecoveryMode(RecoveryMode.AVAILABLE);
            device.waitForDeviceAvailable(mDeviceBootTime);
        } catch (DeviceUnresponsiveException e) {
            // assume this is a build problem
            throw new DeviceFailedToBootError(
                    String.format(
                            "Device %s did not become available after flashing %s",
                            device.getSerialNumber(), deviceUpdateImage.getAbsolutePath()),
                    device.getDeviceDescriptor());
        }
        CLog.i("Device update completed on %s", device.getDeviceDescriptor());
        // calling this last because we want to inject device side build info after device boots up
        if (mBootStrapBuildInfo) {
            super.setUp(testInfo);
        }
    }

    /**
     * Provides a {@link File} instance representing the device image file to be used for updating
     */
    protected abstract File getDeviceUpdateImage();

    /**
     * Actions to be performed before the device is updated. This method will be called outside of
     * flashing concurrency control.
     *
     * @param deviceUpdateImage
     * @param device
     * @throws TargetSetupError
     */
    protected abstract void preUpdateActions(File deviceUpdateImage, ITestDevice device)
            throws DeviceNotAvailableException, TargetSetupError;

    /**
     * Performs the device image update on device
     *
     * @param deviceUpdateImage
     * @param device
     * @throws TargetSetupError
     */
    protected abstract void performDeviceUpdate(File deviceUpdateImage, ITestDevice device)
            throws DeviceNotAvailableException, TargetSetupError;

    /**
     * Actions to be performed after the device is updated but before post update setup steps are
     * performed. This method will be called outside of flashing concurrency control.
     *
     * @param deviceUpdateImage
     * @param device
     * @throws TargetSetupError
     */
    protected abstract void postUpdateActions(File deviceUpdateImage, ITestDevice device)
            throws DeviceNotAvailableException, TargetSetupError;
}
