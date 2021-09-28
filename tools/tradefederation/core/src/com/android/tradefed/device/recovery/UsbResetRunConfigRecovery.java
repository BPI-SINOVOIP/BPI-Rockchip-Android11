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
package com.android.tradefed.device.recovery;

import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceAllocationState;
import com.android.tradefed.device.IManagedTestDevice;

import java.util.HashSet;
import java.util.Set;

/** Allow to trigger a command to reset the USB of a device */
@OptionClass(alias = "usb-reset-recovery")
public class UsbResetRunConfigRecovery extends RunConfigDeviceRecovery {

    private Set<String> mLastInvoked = new HashSet<>();

    @Override
    public boolean shouldSkip(IManagedTestDevice device) {
        boolean res = device.isStateBootloaderOrFastbootd();
        // Do not reset available devices
        if (!res && DeviceAllocationState.Available.equals(device.getAllocationState())) {
            res = true;
        }
        if (!res) {
            return checkRanBefore(device);
        }
        // Skip since device is available or in fastboot
        return true;
    }

    private boolean checkRanBefore(IManagedTestDevice device) {
        String serial = device.getSerialNumber();
        // Avoid running the same device twice in row of recovery request to give a chance to
        // others recovery to start too.
        if (!mLastInvoked.add(serial)) {
            mLastInvoked.remove(serial);
            return true;
        }
        return false;
    }
}
