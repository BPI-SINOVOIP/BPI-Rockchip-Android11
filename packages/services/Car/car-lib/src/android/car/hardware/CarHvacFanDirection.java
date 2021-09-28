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

package android.car.hardware;

import android.annotation.IntDef;
import android.annotation.SystemApi;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * CarHvacFanDirection is an abstraction for car's fan directions.
 * <p>
 * {@link android.car.VehiclePropertyIds#HVAC_FAN_DIRECTION} and
 * {@link android.car.VehiclePropertyIds#HVAC_FAN_DIRECTION_AVAILABLE} use constants in
 * {@link CarHvacFanDirection} as their property value.
 * Developers can compare the property value with constants in this class to know which fan
 * direction is used in cars.
 * </p>
 * @hide
 */
// This class is only designed to provide constants for car's fan direction. The constants should
// exactly be same as VehicleHvacFanDirection in file
// hardware/interfaces/automotive/vehicle/2.0/types.hal.
@SystemApi
public final class CarHvacFanDirection {
    /** Constant for unknown fan direction. */
    public static final int UNKNOWN = 0x0;
    /** Constant for face direction. */
    public static final int FACE = 0x01;
    /** Constant for floor direction. */
    public static final int FLOOR = 0x02;
    /** Constant for face and floor direction. */
    public static final int FACE_AND_FLOOR = 0x03; // FACE_AND_FLOOR = FACE | FLOOR
    /** Constant for defrost direction. */
    public static final int DEFROST = 0x04;
    /** Constant for defrost and floor direction.*/
    public static final int DEFROST_AND_FLOOR = 0x06; // DEFROST_AND_FLOOR = DEFROST | FLOOR

    /**@hide*/
    @IntDef(value = {
            UNKNOWN,
            FACE,
            FLOOR,
            FACE_AND_FLOOR,
            DEFROST,
            DEFROST_AND_FLOOR
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface Enum {}
    private CarHvacFanDirection() {}
}
