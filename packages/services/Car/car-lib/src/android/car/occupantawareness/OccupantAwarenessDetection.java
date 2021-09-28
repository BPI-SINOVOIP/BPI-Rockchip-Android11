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

package android.car.occupantawareness;

import static java.lang.annotation.RetentionPolicy.SOURCE;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.car.Car;
import android.car.annotation.RequiredFeature;
import android.os.Parcel;
import android.os.Parcelable;

import java.lang.annotation.Retention;

/**
 * Complete detection result for a single detected person. Includes presence detection, {@link
 * GazeDetection} and {@link DriverMonitoringDetection}.
 *
 * <p>Register to listen to events via {@link OccupantAwarenessManager}.
 *
 * @hide
 */
@RequiredFeature(Car.OCCUPANT_AWARENESS_SERVICE)
public final class OccupantAwarenessDetection implements Parcelable {
    /** Empty occupant flag. */
    public static final int VEHICLE_OCCUPANT_NONE = 0;

    /** Occupants that the system detects as the driver. */
    public static final int VEHICLE_OCCUPANT_DRIVER = 1 << 2;

    /** Occupants that the system detects as front seat passengers. */
    public static final int VEHICLE_OCCUPANT_FRONT_PASSENGER = 1 << 1;

    /** Occupants that the system detects in the second vehicle row, on the left. */
    public static final int VEHICLE_OCCUPANT_ROW_2_PASSENGER_LEFT = 1 << 3;

    /** Occupants that the system detects in the second vehicle row, in the center. */
    public static final int VEHICLE_OCCUPANT_ROW_2_PASSENGER_CENTER = 1 << 4;

    /** Occupants that the system detects in the second vehicle row, on the right. */
    public static final int VEHICLE_OCCUPANT_ROW_2_PASSENGER_RIGHT = 1 << 5;

    /** Occupants that the system detects in the third vehicle row, on the left. */
    public static final int VEHICLE_OCCUPANT_ROW_3_PASSENGER_LEFT = 1 << 6;

    /** Occupants that the system detects in the third vehicle row, in the middle. */
    public static final int VEHICLE_OCCUPANT_ROW_3_PASSENGER_CENTER = 1 << 7;

    /** Occupants that the system detects in the third vehicle row, on the right. */
    public static final int VEHICLE_OCCUPANT_ROW_3_PASSENGER_RIGHT = 1 << 8;

    /** All occupants that the system detects in the front row of the vehicle. */
    public static final int VEHICLE_OCCUPANT_ALL_FRONT_OCCUPANTS =
            VEHICLE_OCCUPANT_DRIVER | VEHICLE_OCCUPANT_FRONT_PASSENGER;

    /** All occupants that the system detects in the second row of the vehicle. */
    public static final int VEHICLE_OCCUPANT_ALL_ROW_2_OCCUPANTS =
            VEHICLE_OCCUPANT_ROW_2_PASSENGER_LEFT
                    | VEHICLE_OCCUPANT_ROW_2_PASSENGER_RIGHT
                    | VEHICLE_OCCUPANT_ROW_2_PASSENGER_CENTER;

    /** All occupants that the system detects in the third row of the vehicle. */
    public static final int VEHICLE_OCCUPANT_ALL_ROW_3_OCCUPANTS =
            VEHICLE_OCCUPANT_ROW_3_PASSENGER_LEFT
                    | VEHICLE_OCCUPANT_ROW_3_PASSENGER_RIGHT
                    | VEHICLE_OCCUPANT_ROW_3_PASSENGER_CENTER;

    /** All occupants that the system detects in the vehicle. */
    public static final int VEHICLE_OCCUPANT_ALL_OCCUPANTS =
            VEHICLE_OCCUPANT_ALL_FRONT_OCCUPANTS
                    | VEHICLE_OCCUPANT_ALL_ROW_2_OCCUPANTS
                    | VEHICLE_OCCUPANT_ALL_ROW_3_OCCUPANTS;

    /**
     * Vehicle occupant roles based on their location in the vehicle.
     *
     * @hide
     */
    @Retention(SOURCE)
    @IntDef(
            flag = true,
            value = {
                VEHICLE_OCCUPANT_NONE,
                VEHICLE_OCCUPANT_DRIVER,
                VEHICLE_OCCUPANT_FRONT_PASSENGER,
                VEHICLE_OCCUPANT_ROW_2_PASSENGER_LEFT,
                VEHICLE_OCCUPANT_ROW_2_PASSENGER_CENTER,
                VEHICLE_OCCUPANT_ROW_2_PASSENGER_RIGHT,
                VEHICLE_OCCUPANT_ROW_3_PASSENGER_LEFT,
                VEHICLE_OCCUPANT_ROW_3_PASSENGER_CENTER,
                VEHICLE_OCCUPANT_ROW_3_PASSENGER_RIGHT,
                VEHICLE_OCCUPANT_ALL_FRONT_OCCUPANTS,
                VEHICLE_OCCUPANT_ALL_ROW_2_OCCUPANTS,
                VEHICLE_OCCUPANT_ALL_ROW_3_OCCUPANTS
            })
    public @interface VehicleOccupantRole {}

    /** No prediction could be made. */
    public static final int CONFIDENCE_LEVEL_NONE = 0;

    /**
     * Best-guess, low-confidence prediction. Predictions exceeding this threshold are adequate for
     * non-critical applications.
     */
    public static final int CONFIDENCE_LEVEL_LOW = 1;

    /**
     * High-confidence prediction. Predictions exceeding this threshold are adequate for
     * applications that require reliable predictions.
     */
    public static final int CONFIDENCE_LEVEL_HIGH = 2;

    /** Highest confidence rate achievable. */
    public static final int CONFIDENCE_LEVEL_MAX = 3;

    /**
     * Confidence scores for predictions.
     *
     * @hide
     */
    @Retention(SOURCE)
    @IntDef(
            value = {
                CONFIDENCE_LEVEL_NONE,
                CONFIDENCE_LEVEL_LOW,
                CONFIDENCE_LEVEL_HIGH,
                CONFIDENCE_LEVEL_MAX
            })
    public @interface ConfidenceLevel {}

    /** The {@link VehicleOccupantRole} of the face associated with this event. */
    public final @VehicleOccupantRole int role;

    /** Timestamp when the underlying detection data was detected, in milliseconds since boot. */
    public final long timestampMillis;

    /** Indicates whether any person was detected for the given role. */
    public final boolean isPresent;

    /**
     * {@link GazeDetection} data for the requested role, or {@code null} if no person was found.
     */
    public final @Nullable GazeDetection gazeDetection;

    /**
     * {@link DriverMonitoringDetection} data for the driver, or {@code null} if the role was
     * non-driver or if the detection could not be computed.
     */
    public final @Nullable DriverMonitoringDetection driverMonitoringDetection;

    public OccupantAwarenessDetection(
            @VehicleOccupantRole int role,
            long timestampMillis,
            boolean isPresent,
            @Nullable GazeDetection gazeDetection,
            @Nullable DriverMonitoringDetection driverMonitoringDetection) {
        this.role = role;
        this.timestampMillis = timestampMillis;
        this.isPresent = isPresent;
        this.gazeDetection = gazeDetection;
        this.driverMonitoringDetection = driverMonitoringDetection;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(@NonNull Parcel dest, int flags) {
        dest.writeInt(role);
        dest.writeLong(timestampMillis);
        dest.writeBoolean(isPresent);
        dest.writeParcelable(gazeDetection, flags);
        dest.writeParcelable(driverMonitoringDetection, flags);
    }

    @Override
    public String toString() {
        return "OccupantAwarenessDetection{"
                + "role="  + role
                + ", timestampMillis=" + timestampMillis
                + ", isPresent=" + isPresent
                + ", gazeDetection="
                + (gazeDetection == null ? "(null)" : gazeDetection.toString())
                + ", driverMonitoringDetection="
                + (driverMonitoringDetection == null
                        ? "(null)" : driverMonitoringDetection.toString())
                + "}";
    }

    public static final @NonNull Parcelable.Creator<OccupantAwarenessDetection> CREATOR =
            new Parcelable.Creator<OccupantAwarenessDetection>() {
                public OccupantAwarenessDetection createFromParcel(Parcel in) {
                    return new OccupantAwarenessDetection(in);
                }

                public OccupantAwarenessDetection[] newArray(int size) {
                    return new OccupantAwarenessDetection[size];
                }
            };

    private OccupantAwarenessDetection(Parcel in) {
        role = in.readInt();
        timestampMillis = in.readLong();
        isPresent = in.readBoolean();
        gazeDetection = in.readParcelable(GazeDetection.class.getClassLoader());
        driverMonitoringDetection =
                in.readParcelable(DriverMonitoringDetection.class.getClassLoader());
    }
}
