/*
 * Copyright (C) 2016 The Android Open Source Project
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
package android.car;

import android.annotation.IntDef;
import android.car.hardware.CarPropertyConfig;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Object used to indicate area value for car properties which have area type
 * {@link VehicleAreaType#VEHICLE_AREA_TYPE_WHEEL}.
 * <p>
 * The constants defined by {@link VehicleAreaWheel} indicate the position for area type
 * {@link VehicleAreaType#VEHICLE_AREA_TYPE_WHEEL}. A property can have a single or a combination of
 * positions. Developers can query the position using
 * {@link android.car.hardware.property.CarPropertyManager#getAreaId(int, int)}.
 * </p><p>
 * Refer to {@link CarPropertyConfig#getAreaIds()} for more information about areaId.
 * </p>
 */

// This class is only designed to provide constants for VehicleAreaWheel. The constants should
// exactly be same as VehicleAreaWheel in /hardware/interfaces/automotive/vehicle/2.0/types.hal.
public final class VehicleAreaWheel {
    /** Unknown wheel*/
    public static final int WHEEL_UNKNOWN = 0x00;
    /** Constant for left front wheel.*/
    public static final int WHEEL_LEFT_FRONT = 0x01;
    /** Constant for right front wheel.*/
    public static final int WHEEL_RIGHT_FRONT = 0x02;
    /** Constant for left rear wheel.*/
    public static final int WHEEL_LEFT_REAR = 0x04;
    /** Constant for right rear wheel.*/
    public static final int WHEEL_RIGHT_REAR = 0x08;

    /** @hide */
    @IntDef(prefix = {"WHEEL_"}, value = {
            WHEEL_UNKNOWN,
            WHEEL_LEFT_FRONT,
            WHEEL_RIGHT_FRONT,
            WHEEL_LEFT_REAR,
            WHEEL_RIGHT_REAR
    })
    @Retention(RetentionPolicy.SOURCE)

    public  @interface Enum {}
    private VehicleAreaWheel() {}
}

