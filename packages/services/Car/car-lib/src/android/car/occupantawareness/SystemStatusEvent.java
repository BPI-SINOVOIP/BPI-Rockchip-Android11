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
import android.os.Parcel;
import android.os.Parcelable;

import java.lang.annotation.Retention;

/**
 * Current system status.
 *
 * <p>Detection system may be ready, not-supported, in a failure mode or not-yet-ready.
 *
 * @hide
 */
public final class SystemStatusEvent implements Parcelable {
    /** The system is ready to provide data. */
    public static final int SYSTEM_STATUS_READY = 0;

    /**
     * Detection is not supported in this vehicle due to a permanent lack of capabilities. Clients
     * need not retry.
     */
    public static final int SYSTEM_STATUS_NOT_SUPPORTED = 1;

    /** The system is not yet ready to serve requests. Clients should check back again later. */
    public static final int SYSTEM_STATUS_NOT_READY = 2;

    /**
     * A permanent hardware failure has occurred. Clients need not retry until the underlying
     * hardware has been fixed.
     */
    public static final int SYSTEM_STATUS_SYSTEM_FAILURE = 3;

    /**
     * System state status values.
     *
     * @hide
     */
    @Retention(SOURCE)
    @IntDef({
        SYSTEM_STATUS_READY,
        SYSTEM_STATUS_NOT_SUPPORTED,
        SYSTEM_STATUS_SYSTEM_FAILURE,
        SYSTEM_STATUS_NOT_READY
    })
    public @interface SystemStatus {}

    /** No detection types are supported. */
    public static final int DETECTION_TYPE_NONE = 0;

    /** Presence detection for occupants in the vehicle. */
    public static final int DETECTION_TYPE_PRESENCE = 1 << 0;

    /** Gaze data for occupant in the vehicle. */
    public static final int DETECTION_TYPE_GAZE = 1 << 1;

    /** Driver monitoring state for the driver in the vehicle. */
    public static final int DETECTION_TYPE_DRIVER_MONITORING = 1 << 2;

    /**
     * Detection types.
     *
     * @hide
     */
    @Retention(SOURCE)
    @IntDef(
            flag = true,
            value = {
                DETECTION_TYPE_PRESENCE,
                DETECTION_TYPE_GAZE,
                DETECTION_TYPE_DRIVER_MONITORING
            })
    public @interface DetectionTypeFlags {}

    public final @SystemStatus int systemStatus;
    public final @DetectionTypeFlags int detectionType;

    public SystemStatusEvent(@SystemStatus int status, @DetectionTypeFlags int type) {
        systemStatus = status;
        detectionType = type;
    }

    public SystemStatusEvent() {
        systemStatus = SYSTEM_STATUS_NOT_READY;
        detectionType = DETECTION_TYPE_NONE;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(@NonNull Parcel dest, int flags) {
        dest.writeInt(systemStatus);
        dest.writeInt(detectionType);
    }

    @Override
    public String toString() {
        return "SystemStatusEvent{"
                + "systemStatus="
                + systemStatus
                + ", detectionType="
                + detectionType
                + "}";
    }

    public static final @NonNull Parcelable.Creator<SystemStatusEvent> CREATOR =
            new Parcelable.Creator<SystemStatusEvent>() {
                public SystemStatusEvent createFromParcel(Parcel in) {
                    return new SystemStatusEvent(in);
                }

                public SystemStatusEvent[] newArray(int size) {
                    return new SystemStatusEvent[size];
                }
            };

    private SystemStatusEvent(Parcel in) {
        systemStatus = in.readInt();
        detectionType = in.readInt();
    }
}
