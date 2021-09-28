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

package com.android.tradefed.device.battery;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;

/** The interface defining the interaction with a battery of a device. */
public interface IBatteryInfo {

    /** Describes the current battery charging state. */
    public enum BatteryState {
        NOT_CHARGING,
        CHARGING,
        UNDEFINED,
        INFEASIBLE
    }

    /** Enable the battery charging of a device. */
    public void enableCharging(ITestDevice device) throws DeviceNotAvailableException;

    /** Disable the battery charging of a device. */
    public void disableCharging(ITestDevice device) throws DeviceNotAvailableException;

    /** Returns the current state of a device battery. */
    public BatteryState checkBatteryState(ITestDevice device) throws DeviceNotAvailableException;
}
