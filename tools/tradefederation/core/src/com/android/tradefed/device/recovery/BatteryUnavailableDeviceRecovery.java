/*
 * Copyright (C) 2018 The Android Open Source Project
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

import com.android.ddmlib.IDevice.DeviceState;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.IManagedTestDevice;

/** Recovery checker that will trigger a configuration if the battery level is not available. */
@OptionClass(alias = "battery-recovery")
public class BatteryUnavailableDeviceRecovery extends RunConfigDeviceRecovery {

    @Override
    public boolean shouldSkip(IManagedTestDevice device) {
        if (device.isStateBootloaderOrFastbootd()) {
            return true;
        }
        if (DeviceState.OFFLINE.equals(device.getIDevice().getState())) {
            return true;
        }
        // Check the battery level of the device and if it's NA continue recovery.
        return device.getBattery() != null;
    }
}
