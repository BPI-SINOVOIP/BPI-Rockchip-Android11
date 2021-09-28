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

import android.annotation.NonNull;
import android.car.occupantawareness.OccupantAwarenessDetection.ConfidenceLevel;
import android.os.Parcel;
import android.os.Parcelable;

/**
 * Detection state data for monitoring driver attention.
 *
 * @hide
 */
public final class DriverMonitoringDetection implements Parcelable {

    /** {@link OccupantAwarenessDetection.ConfidenceLevel} for the driver monitoring detection. */
    public final @ConfidenceLevel int confidenceLevel;

    /** Indicates whether the driver is looking on-road. */
    public final boolean isLookingOnRoad;

    /**
     * Duration that the driver has been looking on or off-road, in milliseconds.
     *
     * <p>If 'isLookingOnRoad' is true, this indicates the continuous duration of time that the
     * driver has been looking on-road (in milliseconds). Otherwise, this indicates the continuous
     * duration of time that the driver is looking off-road (in milliseconds).
     */
    public final long gazeDurationMillis;

    public DriverMonitoringDetection() {
        confidenceLevel = OccupantAwarenessDetection.CONFIDENCE_LEVEL_NONE;
        isLookingOnRoad = false;
        gazeDurationMillis = 0;
    }

    public DriverMonitoringDetection(
            @ConfidenceLevel int confidenceLevel,
            boolean isLookingOnRoad,
            long gazeDurationMillis) {
        this.confidenceLevel = confidenceLevel;
        this.isLookingOnRoad = isLookingOnRoad;
        this.gazeDurationMillis = gazeDurationMillis;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public String toString() {
        return "DriverMonitoringDetection{"
                + "confidenceLevel="
                + confidenceLevel
                + ", isLookingOnRoad="
                + isLookingOnRoad
                + ", gazeDurationMillis="
                + gazeDurationMillis;
    }

    @Override
    public void writeToParcel(@NonNull Parcel dest, int flags) {
        dest.writeInt(confidenceLevel);
        dest.writeBoolean(isLookingOnRoad);
        dest.writeLong(gazeDurationMillis);
    }

    public static final @NonNull Parcelable.Creator<DriverMonitoringDetection> CREATOR =
            new Parcelable.Creator<DriverMonitoringDetection>() {
                public DriverMonitoringDetection createFromParcel(Parcel in) {
                    return new DriverMonitoringDetection(in);
                }

                public DriverMonitoringDetection[] newArray(int size) {
                    return new DriverMonitoringDetection[size];
                }
            };

    private DriverMonitoringDetection(Parcel in) {
        confidenceLevel = in.readInt();
        isLookingOnRoad = in.readBoolean();
        gazeDurationMillis = in.readLong();
    }
}
