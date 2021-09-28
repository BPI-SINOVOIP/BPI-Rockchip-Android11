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
import android.os.Parcel;
import android.os.Parcelable;

import java.lang.annotation.Retention;

/**
 * Detection result for gaze detection for the respective {@link VehicleOccupantRole}.
 *
 * @hide
 */
public final class GazeDetection implements Parcelable {

    /** A unknown gaze region, not otherwise specified. */
    public static final int VEHICLE_REGION_UNKNOWN = 0;

    /** Center instrument cluster in front of the driver. */
    public static final int VEHICLE_REGION_CENTER_INSTRUMENT_CLUSTER = 1;

    /** The rear-view mirror. */
    public static final int VEHICLE_REGION_REAR_VIEW_MIRROR = 2;

    /** The left side mirror. */
    public static final int VEHICLE_REGION_LEFT_SIDE_MIRROR = 3;

    /** The right side mirror. */
    public static final int VEHICLE_REGION_RIGHT_SIDE_MIRROR = 4;

    /** The forward roadway. */
    public static final int VEHICLE_REGION_FORWARD_ROADWAY = 5;

    /** Out-the-window to the right. */
    public static final int VEHICLE_REGION_LEFT_ROADWAY = 6;

    /** Out-the-window to the right. */
    public static final int VEHICLE_REGION_RIGHT_ROADWAY = 7;

    /** Center head-unit display. */
    public static final int VEHICLE_REGION_HEAD_UNIT_DISPLAY = 8;

    /**
     * Vehicle regions
     *
     * @hide
     */
    @Retention(SOURCE)
    @IntDef({
        VEHICLE_REGION_UNKNOWN,
        VEHICLE_REGION_CENTER_INSTRUMENT_CLUSTER,
        VEHICLE_REGION_REAR_VIEW_MIRROR,
        VEHICLE_REGION_LEFT_SIDE_MIRROR,
        VEHICLE_REGION_RIGHT_SIDE_MIRROR,
        VEHICLE_REGION_FORWARD_ROADWAY,
        VEHICLE_REGION_LEFT_ROADWAY,
        VEHICLE_REGION_RIGHT_ROADWAY,
        VEHICLE_REGION_HEAD_UNIT_DISPLAY
    })
    public @interface VehicleRegion {}

    /** {@link OccupantAwarenessDetection.ConfidenceLevel} for the gaze detection. */
    @OccupantAwarenessDetection.ConfidenceLevel public final int confidenceLevel;

    /**
     * Location of the subject's left eye, in millimeters.
     *
     * <p>Vehicle origin is defined as part of the Cabin Space API. All units in millimeters. +x is
     * right (from driver perspective), +y is up, -z is forward.
     *
     * <p>May be {@code null} if the underlying detection system does not export eye position data.
     */
    public final @Nullable Point3D leftEyePosition;

    /**
     * Location of the subject's right eye, in millimeters.
     *
     * <p>Vehicle origin is defined as part of the Cabin Space API. All units in millimeters. +x is
     * right (from driver perspective), +y is up, -z is forward.
     *
     * <p>May be {@code null} if the underlying detection system does not export eye position data.
     */
    public final @Nullable Point3D rightEyePosition;

    /**
     * Direction of the subject's head orientation, as a <a
     * href="https://en.wikipedia.org/wiki/Unit_vector">unit-vector</a>.
     *
     * <p>May be {@code null} if the underlying system does not support head orientation vectors.
     */
    public final @Nullable Point3D headAngleUnitVector;

    /**
     * Direction of the gaze angle, as a <a
     * href="https://en.wikipedia.org/wiki/Unit_vector">unit-vector</a>.
     *
     * <p>May be {@code null} if the underlying system does not support vectors.
     */
    public final @Nullable Point3D gazeAngleUnitVector;

    /** {@link VehicleRegion} where the subject is currently looking. */
    @VehicleRegion public final int gazeTarget;

    /** Duration on the current gaze target, in milliseconds. */
    public final long durationOnTargetMillis;

    public GazeDetection(
            @OccupantAwarenessDetection.ConfidenceLevel int confidenceLevel,
            @Nullable Point3D leftEyePosition,
            @Nullable Point3D rightEyePosition,
            @Nullable Point3D headAngleUnitVector,
            @Nullable Point3D gazeAngleUnitVector,
            @VehicleRegion int gazeTarget,
            long durationOnTargetMillis) {

        this.confidenceLevel = confidenceLevel;
        this.leftEyePosition = leftEyePosition;
        this.rightEyePosition = rightEyePosition;
        this.headAngleUnitVector = headAngleUnitVector;
        this.gazeAngleUnitVector = gazeAngleUnitVector;
        this.gazeTarget = gazeTarget;
        this.durationOnTargetMillis = durationOnTargetMillis;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(@NonNull Parcel dest, int flags) {
        dest.writeInt(confidenceLevel);
        dest.writeParcelable(leftEyePosition, flags);
        dest.writeParcelable(rightEyePosition, flags);
        dest.writeParcelable(headAngleUnitVector, flags);
        dest.writeParcelable(gazeAngleUnitVector, flags);
        dest.writeInt(gazeTarget);
        dest.writeLong(durationOnTargetMillis);
    }

    @Override
    public String toString() {
        return "GazeDetection{"
                + "confidenceLevel=" + confidenceLevel
                + ", leftEyePosition=" + (leftEyePosition == null ? "(null)" : leftEyePosition)
                + ", rightEyePosition=" + (rightEyePosition == null ? "(null)" : rightEyePosition)
                + ", headAngleUnitVector="
                + (headAngleUnitVector == null ? "(null)" : headAngleUnitVector)
                + ", gazeAngleUnitVector="
                + (gazeAngleUnitVector == null ? "(null)" : gazeAngleUnitVector)
                + ", gazeTarget=" + gazeTarget
                + ", durationOnTargetMillis=" + durationOnTargetMillis
                + "}";
    }

    public static final @NonNull Parcelable.Creator<GazeDetection> CREATOR =
            new Parcelable.Creator<GazeDetection>() {
                public GazeDetection createFromParcel(Parcel in) {
                    return new GazeDetection(in);
                }

                public GazeDetection[] newArray(int size) {
                    return new GazeDetection[size];
                }
            };

    private GazeDetection(Parcel in) {
        confidenceLevel = in.readInt();
        leftEyePosition = in.readParcelable(Point3D.class.getClassLoader());
        rightEyePosition = in.readParcelable(Point3D.class.getClassLoader());
        headAngleUnitVector = in.readParcelable(Point3D.class.getClassLoader());
        gazeAngleUnitVector = in.readParcelable(Point3D.class.getClassLoader());
        gazeTarget = in.readInt();
        durationOnTargetMillis = in.readLong();
    }
}
