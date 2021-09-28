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

package com.android.car;

import android.annotation.Nullable;
import android.car.occupantawareness.DriverMonitoringDetection;
import android.car.occupantawareness.GazeDetection;
import android.car.occupantawareness.OccupantAwarenessDetection;
import android.car.occupantawareness.OccupantAwarenessDetection.ConfidenceLevel;
import android.car.occupantawareness.Point3D;
import android.car.occupantawareness.SystemStatusEvent;
import android.car.occupantawareness.SystemStatusEvent.DetectionTypeFlags;
import android.car.occupantawareness.SystemStatusEvent.SystemStatus;
import android.hardware.automotive.occupant_awareness.IOccupantAwareness;
import android.hardware.automotive.occupant_awareness.OccupantAwarenessStatus;
import android.util.Log;

/*package*/ final class OccupantAwarenessUtils {
    private static final String TAG = "OccupantAwarenessUtils";

    private OccupantAwarenessUtils() {}

    /** Converts status flags into to a SystemStatusEvent. */
    static SystemStatusEvent convertToStatusEvent(int inputDetectionFlags, byte inputStatus) {
        // Parse the system status from the hardware layer.
        @SystemStatus int outputStatus = SystemStatusEvent.SYSTEM_STATUS_NOT_READY;

        switch (inputStatus) {
            case OccupantAwarenessStatus.READY:
                outputStatus = SystemStatusEvent.SYSTEM_STATUS_READY;
                break;

            case OccupantAwarenessStatus.NOT_SUPPORTED:
                outputStatus = SystemStatusEvent.SYSTEM_STATUS_NOT_SUPPORTED;
                break;

            case OccupantAwarenessStatus.NOT_INITIALIZED:
                outputStatus = SystemStatusEvent.SYSTEM_STATUS_NOT_READY;
                break;

            case OccupantAwarenessStatus.FAILURE:
                outputStatus = SystemStatusEvent.SYSTEM_STATUS_SYSTEM_FAILURE;
                break;

            default:
                Log.e(TAG, "Invalid status code: " + inputStatus);
                break;
        }

        // Parse the detection flags from the hardware layer.
        @DetectionTypeFlags int outputFlags = SystemStatusEvent.DETECTION_TYPE_NONE;

        switch (inputDetectionFlags) {
            case IOccupantAwareness.CAP_NONE:
                outputFlags = SystemStatusEvent.DETECTION_TYPE_NONE;
                break;

            case IOccupantAwareness.CAP_PRESENCE_DETECTION:
                outputFlags = SystemStatusEvent.DETECTION_TYPE_PRESENCE;
                break;

            case IOccupantAwareness.CAP_GAZE_DETECTION:
                outputFlags = SystemStatusEvent.DETECTION_TYPE_GAZE;
                break;

            case IOccupantAwareness.CAP_DRIVER_MONITORING_DETECTION:
                outputFlags = SystemStatusEvent.DETECTION_TYPE_DRIVER_MONITORING;
                break;

            default:
                Log.e(TAG, "Invalid status code: " + inputDetectionFlags);
                break;
        }

        return new SystemStatusEvent(outputStatus, outputFlags);
    }

    /** Converts an input confidence to a ConfidenceLevel. */
    static @ConfidenceLevel int convertToConfidenceScore(
            @android.hardware.automotive.occupant_awareness.ConfidenceLevel int inputConfidence) {
        switch (inputConfidence) {
            case android.hardware.automotive.occupant_awareness.ConfidenceLevel.MAX:
                return OccupantAwarenessDetection.CONFIDENCE_LEVEL_MAX;

            case android.hardware.automotive.occupant_awareness.ConfidenceLevel.HIGH:
                return OccupantAwarenessDetection.CONFIDENCE_LEVEL_HIGH;

            case android.hardware.automotive.occupant_awareness.ConfidenceLevel.LOW:
                return OccupantAwarenessDetection.CONFIDENCE_LEVEL_LOW;

            case android.hardware.automotive.occupant_awareness.ConfidenceLevel.NONE:
            default:
                return OccupantAwarenessDetection.CONFIDENCE_LEVEL_NONE;
        }
    }

    /**
     * Converts an input point to a Point3D. Returns null if the source vector was null or empty.
     */
    static Point3D convertToPoint3D(@Nullable double[] vector) {
        if (vector != null && vector.length == 3) {
            return new Point3D(vector[0], vector[1], vector[2]);
        } else {
            return null;
        }
    }

    /** Converts an input timestamp to milliseconds since boot. */
    static long convertTimestamp(long inputTimestamp) {
        return inputTimestamp;
    }

    /**
     * Converts an input @android.hardware.automotive.occupant_awareness.Role to a
     * VehicleOccupantRole.
     */
    static @OccupantAwarenessDetection.VehicleOccupantRole int convertToRole(int inputRole) {
        if (inputRole == android.hardware.automotive.occupant_awareness.Role.INVALID
                || inputRole == android.hardware.automotive.occupant_awareness.Role.UNKNOWN) {
            return OccupantAwarenessDetection.VEHICLE_OCCUPANT_NONE;
        } else if (inputRole == android.hardware.automotive.occupant_awareness.Role.ALL_OCCUPANTS) {
            return OccupantAwarenessDetection.VEHICLE_OCCUPANT_ALL_OCCUPANTS;
        }

        int outputRole = OccupantAwarenessDetection.VEHICLE_OCCUPANT_NONE;

        if ((inputRole & android.hardware.automotive.occupant_awareness.Role.DRIVER) > 0) {
            outputRole |= OccupantAwarenessDetection.VEHICLE_OCCUPANT_DRIVER;
        }

        if ((inputRole & android.hardware.automotive.occupant_awareness.Role.FRONT_PASSENGER) > 0) {
            outputRole |= OccupantAwarenessDetection.VEHICLE_OCCUPANT_FRONT_PASSENGER;
        }

        if ((inputRole & android.hardware.automotive.occupant_awareness.Role.ROW_2_PASSENGER_LEFT)
                > 0) {
            outputRole |= OccupantAwarenessDetection.VEHICLE_OCCUPANT_ROW_2_PASSENGER_LEFT;
        }

        if ((inputRole & android.hardware.automotive.occupant_awareness.Role.ROW_2_PASSENGER_CENTER)
                > 0) {
            outputRole |= OccupantAwarenessDetection.VEHICLE_OCCUPANT_ROW_2_PASSENGER_CENTER;
        }

        if ((inputRole & android.hardware.automotive.occupant_awareness.Role.ROW_2_PASSENGER_RIGHT)
                > 0) {
            outputRole |= OccupantAwarenessDetection.VEHICLE_OCCUPANT_ROW_2_PASSENGER_RIGHT;
        }

        if ((inputRole & android.hardware.automotive.occupant_awareness.Role.ROW_3_PASSENGER_LEFT)
                > 0) {
            outputRole |= OccupantAwarenessDetection.VEHICLE_OCCUPANT_ROW_3_PASSENGER_LEFT;
        }

        if ((inputRole & android.hardware.automotive.occupant_awareness.Role.ROW_3_PASSENGER_CENTER)
                > 0) {
            outputRole |= OccupantAwarenessDetection.VEHICLE_OCCUPANT_ROW_3_PASSENGER_CENTER;
        }

        if ((inputRole & android.hardware.automotive.occupant_awareness.Role.ROW_3_PASSENGER_RIGHT)
                > 0) {
            outputRole |= OccupantAwarenessDetection.VEHICLE_OCCUPANT_ROW_3_PASSENGER_RIGHT;
        }

        return outputRole;
    }

    /** Converts an input detection to a GazeDetection. */
    static GazeDetection convertToGazeDetection(
            android.hardware.automotive.occupant_awareness.GazeDetection inputDetection) {

        return new GazeDetection(
                convertToConfidenceScore(inputDetection.gazeConfidence),
                convertToPoint3D(inputDetection.headPosition),
                convertToPoint3D(inputDetection.headPosition),
                convertToPoint3D(inputDetection.headAngleUnitVector),
                convertToPoint3D(inputDetection.gazeAngleUnitVector),
                inputDetection.gazeTarget,
                inputDetection.timeOnTargetMillis);
    }

    /** Converts an input detection to a DriverMonitoringDetection. */
    static DriverMonitoringDetection convertToDriverMonitoringDetection(
            android.hardware.automotive.occupant_awareness.DriverMonitoringDetection
                    inputDetection) {
        return new DriverMonitoringDetection(
                convertToConfidenceScore(inputDetection.confidenceScore),
                inputDetection.isLookingOnRoad,
                inputDetection.gazeDurationMillis);
    }

    /** Converts a PresenceDetection to a boolean indicating if an occupant was present. */
    static boolean convertToPresence(
            android.hardware.automotive.occupant_awareness.PresenceDetection inputDetection) {
        return inputDetection.isOccupantDetected;
    }

    /** Converts an OccupantDetection to OccupantAwarenessDetection. */
    static OccupantAwarenessDetection convertToDetectionEvent(
            long timestamp,
            android.hardware.automotive.occupant_awareness.OccupantDetection inputDetection) {
        // Populate presence, if data is available for this frame.
        boolean isPresent = false;
        if (inputDetection.presenceData != null && inputDetection.presenceData.length > 0) {
            isPresent = convertToPresence(inputDetection.presenceData[0]);
        }

        // Populate gaze, if data is available for this frame.
        GazeDetection gazeDetection = null;
        if (inputDetection.gazeData != null && inputDetection.gazeData.length > 0) {
            gazeDetection = convertToGazeDetection(inputDetection.gazeData[0]);
        }

        // Populate driver monitoring, if data is available for this frame.
        DriverMonitoringDetection driverMonitoringDetection = null;
        if (inputDetection.attentionData != null && inputDetection.attentionData.length > 0) {
            driverMonitoringDetection =
                    convertToDriverMonitoringDetection(inputDetection.attentionData[0]);
        }

        return new OccupantAwarenessDetection(
                convertToRole(inputDetection.role),
                timestamp,
                isPresent,
                gazeDetection,
                driverMonitoringDetection);
    }
}
