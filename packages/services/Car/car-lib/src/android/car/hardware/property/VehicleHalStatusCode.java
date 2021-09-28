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

package android.car.hardware.property;

/**
 * Error codes used in vehicle HAL interface.
 * @hide
 */
public final class VehicleHalStatusCode {

    /** No error detected in HAL.*/
    public static final int STATUS_OK = 0;
    /** Try again. */
    public static final int STATUS_TRY_AGAIN = 1;
    /** Invalid argument provide. */
    public static final int STATUS_INVALID_ARG = 2;
    /**
     * This code must be returned when device that associated with the vehicle
     * property is not available. For example, when client tries to set HVAC
     * temperature when the whole HVAC unit is turned OFF.
     */
    public static final int STATUS_NOT_AVAILABLE = 3;
    /** Access denied */
    public static final int STATUS_ACCESS_DENIED = 4;
    /** Something unexpected has happened in Vehicle HAL */
    public static final int STATUS_INTERNAL_ERROR = 5;

    private VehicleHalStatusCode() {}
}
