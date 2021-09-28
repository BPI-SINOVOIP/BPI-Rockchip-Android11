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
package android.car;

import android.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * List of enums for vehicle gears.
 */
public final class VehicleGear {

    /**
     *  GEAR_* represents meaning of value for {@link VehiclePropertyIds#GEAR_SELECTION} and
     *  {@link VehiclePropertyIds#CURRENT_GEAR}. {@link VehicleGear#GEAR_PARK} and
     *  {@link VehicleGear#GEAR_DRIVE} only apply to the {@link VehiclePropertyIds#GEAR_SELECTION}
     *  property for a vehicle with an automatic transmission.
     */
    public static final int GEAR_UNKNOWN    = 0x0000;
    public static final int GEAR_NEUTRAL    = 0x0001;
    public static final int GEAR_REVERSE    = 0x0002;
    public static final int GEAR_PARK       = 0x0004;
    public static final int GEAR_DRIVE      = 0x0008;
    public static final int GEAR_FIRST      = 0x0010;
    public static final int GEAR_SECOND     = 0x0020;
    public static final int GEAR_THIRD      = 0x0040;
    public static final int GEAR_FOURTH     = 0x0080;
    public static final int GEAR_FIFTH      = 0x0100;
    public static final int GEAR_SIXTH      = 0x0200;
    public static final int GEAR_SEVENTH    = 0x0400;
    public static final int GEAR_EIGHTH     = 0x0800;
    public static final int GEAR_NINTH      = 0x1000;

    /** @hide */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        GEAR_UNKNOWN,
        GEAR_NEUTRAL,
        GEAR_REVERSE,
        GEAR_PARK,
        GEAR_DRIVE,
        GEAR_FIRST,
        GEAR_SECOND,
        GEAR_THIRD,
        GEAR_FOURTH,
        GEAR_FIFTH,
        GEAR_SIXTH,
        GEAR_SEVENTH,
        GEAR_EIGHTH,
        GEAR_NINTH,
    })
    public @interface Enum {}
    private VehicleGear() {}

    /**
     * @param o Integer
     * @return String
     */
    public static  String toString(int o) {
        if (o == GEAR_UNKNOWN) {
            return "GEAR_UNKNOWN";
        }
        if (o == GEAR_NEUTRAL) {
            return "GEAR_NEUTRAL";
        }
        if (o == GEAR_REVERSE) {
            return "GEAR_REVERSE";
        }
        if (o == GEAR_PARK) {
            return "GEAR_PARK";
        }
        if (o == GEAR_DRIVE) {
            return "GEAR_DRIVE";
        }
        if (o == GEAR_FIRST) {
            return "GEAR_FIRST";
        }
        if (o == GEAR_SECOND) {
            return "GEAR_SECOND";
        }
        if (o == GEAR_THIRD) {
            return "GEAR_THIRD";
        }
        if (o == GEAR_FOURTH) {
            return "GEAR_FOURTH";
        }
        if (o == GEAR_FIFTH) {
            return "GEAR_FIFTH";
        }
        if (o == GEAR_SIXTH) {
            return "GEAR_SIXTH";
        }
        if (o == GEAR_SEVENTH) {
            return "GEAR_SEVENTH";
        }
        if (o == GEAR_EIGHTH) {
            return "GEAR_EIGHTH";
        }
        if (o == GEAR_NINTH) {
            return "GEAR_NINTH";
        }
        return "0x" + Integer.toHexString(o);
    }
}
