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

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Object used to indicate the area value for car properties which have area type
 * {@link VehicleAreaType#VEHICLE_AREA_TYPE_SEAT}.
 * <p>
 * The constants defined by {@link VehicleAreaSeat} indicate the position for area type
 * {@link VehicleAreaType#VEHICLE_AREA_TYPE_SEAT}. A property can have a single or a combination of
 * positions. Developers can query the position using
 * {@link android.car.hardware.property.CarPropertyManager#getAreaId(int, int)}.
 * </p><p>
 * Refer to {@link android.car.hardware.CarPropertyConfig#getAreaIds()} for more information about
 * areaId.
 * </p>
 */

// This class is only designed to provide constants for VehicleAreaSeat. The constants should
// be same as VehicleAreaSeat in /hardware/interfaces/automotive/vehicle/2.0/types.hal.
public final class VehicleAreaSeat {
    /** List of vehicle's seats. */
    public static final int SEAT_UNKNOWN = 0;
    /** Row 1 left side seat*/
    public static final int SEAT_ROW_1_LEFT = 0x0001;
    /** Row 1 center seat*/
    public static final int SEAT_ROW_1_CENTER = 0x0002;
    /** Row 1 right side seat*/
    public static final int SEAT_ROW_1_RIGHT = 0x0004;
    /** Row 2 left side seat*/
    public static final int SEAT_ROW_2_LEFT = 0x0010;
    /** Row 2 center seat*/
    public static final int SEAT_ROW_2_CENTER = 0x0020;
    /** Row 2 right side seat*/
    public static final int SEAT_ROW_2_RIGHT = 0x0040;
    /** Row 3 left side seat*/
    public static final int SEAT_ROW_3_LEFT = 0x0100;
    /** Row 3 center seat*/
    public static final int SEAT_ROW_3_CENTER = 0x0200;
    /** Row 3 right side seat*/
    public static final int SEAT_ROW_3_RIGHT = 0x0400;

    /** @hide */
    @IntDef(prefix = {"SEAT_"}, value = {
        SEAT_UNKNOWN,
        SEAT_ROW_1_LEFT,
        SEAT_ROW_1_CENTER,
        SEAT_ROW_1_RIGHT,
        SEAT_ROW_2_LEFT,
        SEAT_ROW_2_CENTER,
        SEAT_ROW_2_RIGHT,
        SEAT_ROW_3_LEFT,
        SEAT_ROW_3_CENTER,
        SEAT_ROW_3_RIGHT
    })
    @Retention(RetentionPolicy.SOURCE)

    public @interface Enum {}
    private VehicleAreaSeat() {}

    /** @hide */
    public static final int SIDE_LEFT = -1;
    /** @hide */
    public static final int SIDE_CENTER = 0;
    /** @hide */
    public static final int SIDE_RIGHT = 1;
    /**
     * Convert row number and side into {@link Enum}.
     *
     * @param rowNumber should be 1, 2 or 3
     * @param side {@link #SIDE_LEFT}. {@link #SIDE_CENTER}, {@link #SIDE_RIGHT}.
     *
     * @hide */
    @Enum
    public static int fromRowAndSide(int rowNumber, int side) {
        if (rowNumber < 1 || rowNumber > 3) {
            return SEAT_UNKNOWN;
        }
        if (side < -1 || side > 1) {
            return SEAT_UNKNOWN;
        }
        int seat = 0x1;
        seat = seat << ((rowNumber - 1) * 4);
        seat = seat << (side + 1);
        return seat;
    }

}
