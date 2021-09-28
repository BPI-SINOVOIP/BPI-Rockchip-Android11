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
 * Object used to indicate area types for car properties.
 * <p>
 * The constants defined by {@link VehicleAreaType} indicate the area types for properties.  A
 * property only has one area type. Developers can query the area type using
 * {@link CarPropertyConfig#getPropertyType()}.
 * </p><p>
 * Refer to {@link VehicleAreaSeat} and {@link VehicleAreaWheel} for more information about areaId.
 * </p>
 */

// This class is only designed to provide constants for VehicleAreaType. The constants should
// exactly be same as VehicleAreaType in /hardware/interfaces/automotive/vehicle/2.0/types.hal.
public final class VehicleAreaType {
    /** Used for global properties */
    public static final int VEHICLE_AREA_TYPE_GLOBAL = 0;
    /** Area type is Window */
    public static final int VEHICLE_AREA_TYPE_WINDOW = 2;
    /** Area type is Seat */
    public static final int VEHICLE_AREA_TYPE_SEAT = 3;
    /** Area type is Door */
    public static final int VEHICLE_AREA_TYPE_DOOR = 4;
    /** Area type is Mirror */
    public static final int VEHICLE_AREA_TYPE_MIRROR = 5;
    /** Area type is Wheel */
    public static final int VEHICLE_AREA_TYPE_WHEEL = 6;
    private VehicleAreaType() {}

    /** @hide */
    @IntDef(prefix = {"VEHICLE_AREA_TYPE_"}, value = {
        VEHICLE_AREA_TYPE_GLOBAL,
        VEHICLE_AREA_TYPE_WINDOW,
        VEHICLE_AREA_TYPE_SEAT,
        VEHICLE_AREA_TYPE_DOOR,
        VEHICLE_AREA_TYPE_MIRROR,
        VEHICLE_AREA_TYPE_WHEEL
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface VehicleAreaTypeValue {}
}
