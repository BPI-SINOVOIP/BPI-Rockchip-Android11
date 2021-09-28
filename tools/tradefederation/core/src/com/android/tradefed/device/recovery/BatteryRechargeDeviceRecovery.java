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

import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.IManagedTestDevice;

/** Allow to trigger a command when the battery level of the device goes under a given threshold. */
@OptionClass(alias = "battery-level-recovery")
public class BatteryRechargeDeviceRecovery extends RunConfigDeviceRecovery {

    @Option(
            name = "min-battery",
            description =
                    "only run this test on a device whose battery level is lower than the given "
                            + "amount. Scale: 0-100")
    private Integer mMinBattery = null;

    @Override
    public boolean shouldSkip(IManagedTestDevice device) {
        if (mMinBattery == null) {
            return true;
        }
        Integer level = device.getBattery();
        // Skip zero battery since some devices use that as 'no-battery'
        if (level == null || level == 0 || level >= mMinBattery) {
            return true;
        }
        return false;
    }
}
