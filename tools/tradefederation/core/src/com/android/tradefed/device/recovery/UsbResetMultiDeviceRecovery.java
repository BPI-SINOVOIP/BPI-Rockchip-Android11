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
package com.android.tradefed.device.recovery;

import com.android.ddmlib.IDevice;
import com.android.helper.aoa.UsbDevice;
import com.android.helper.aoa.UsbException;
import com.android.helper.aoa.UsbHelper;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceAllocationState;
import com.android.tradefed.device.DeviceManager;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.FastbootHelper;
import com.android.tradefed.device.IManagedTestDevice;
import com.android.tradefed.device.IMultiDeviceRecovery;
import com.android.tradefed.device.INativeDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.RunUtil;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.Maps;

import java.util.List;
import java.util.Map;
import java.util.Set;

/** A {@link IMultiDeviceRecovery} which resets USB buses for offline devices. */
@OptionClass(alias = "usb-recovery")
public class UsbResetMultiDeviceRecovery implements IMultiDeviceRecovery {

    @Option(name = "disable", description = "Disable the device recoverer.")
    private boolean mDisable = false;

    @Option(
            name = "only-reset-unmanaged",
            description = "Only reset the device that are not currently managed by Tradefed.")
    private boolean mOnlyResetUnmanaged = false;

    private String mFastbootPath = "fastboot";

    @Override
    public void setFastbootPath(String fastbootPath) {
        mFastbootPath = fastbootPath;
    }

    @Override
    public void recoverDevices(List<IManagedTestDevice> devices) {
        if (mDisable) {
            return; // Skip device recovery
        }

        Map<String, IManagedTestDevice> managedDeviceMap =
                Maps.uniqueIndex(devices, INativeDevice::getSerialNumber);

        try (UsbHelper usb = getUsbHelper()) {
            // Find USB-connected Android devices, using AOA compatibility to differentiate between
            // Android and non-Android devices (supported in 4.1+)
            Set<String> deviceSerials = usb.getSerialNumbers(/* AOAv2-compatible */ true);

            // Find devices currently in fastboot
            FastbootHelper fastboot = getFastbootHelper();
            Set<String> fastbootSerials = fastboot.getDevices();

            // AOA check fails on Fastboot devices, so add them to the set of connected devices
            deviceSerials.addAll(fastbootSerials);

            // Filter out managed devices in a valid state
            for (IManagedTestDevice device : devices) {
                if (!shouldReset(device)) {
                    deviceSerials.remove(device.getSerialNumber());
                }
            }

            // Perform a USB port reset on the remaining devices
            for (String serial : deviceSerials) {
                try (UsbDevice device = usb.getDevice(serial)) {
                    if (mOnlyResetUnmanaged && managedDeviceMap.containsKey(serial)) {
                        continue;
                    }
                    if (device == null) {
                        CLog.w("Device '%s' not found during USB reset.", serial);
                        continue;
                    }

                    CLog.d("Resetting USB port for device '%s'", serial);
                    device.reset();
                    // If device was managed, attempt to reboot it
                    if (managedDeviceMap.containsKey(serial)) {
                        tryReboot(managedDeviceMap.get(serial));
                    }
                }
            }
        } catch (UsbException e) {
            CLog.w("Failed to reset USB ports.");
            CLog.e(e);
        }
    }

    // Determines whether a device is a candidate for recovery
    private boolean shouldReset(IManagedTestDevice device) {
        // Skip stub devices, but make sure not to skip those in fastboot
        IDevice iDevice = device.getIDevice();
        if (iDevice instanceof StubDevice && !(iDevice instanceof DeviceManager.FastbootDevice)) {
            return false;
        }

        // Recover all devices that are neither available nor allocated
        DeviceAllocationState state = device.getAllocationState();
        return !DeviceAllocationState.Allocated.equals(state)
                && !DeviceAllocationState.Available.equals(state);
    }

    // Attempt to reboot a managed device
    private void tryReboot(IManagedTestDevice device) {
        try {
            device.reboot();
        } catch (DeviceNotAvailableException e) {
            CLog.w(
                    "Device '%s' did not come back online after USB reset.",
                    device.getSerialNumber());
            CLog.e(e);
        }
    }

    @VisibleForTesting
    FastbootHelper getFastbootHelper() {
        return new FastbootHelper(RunUtil.getDefault(), mFastbootPath);
    }

    @VisibleForTesting
    UsbHelper getUsbHelper() {
        return new UsbHelper();
    }
}
