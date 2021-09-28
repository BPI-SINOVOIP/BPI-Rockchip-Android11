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

package com.android.experimentalcar;

import android.annotation.FloatRange;
import android.annotation.NonNull;
import android.car.occupantawareness.GazeDetection;
import android.util.Log;

/** {@link GazeAttentionProcessor} estimates driver attention based on a gaze history. */
class GazeAttentionProcessor {
    private static final String TAG = "Car.OAS.GazeAttentionProcessor";
    private static final int NOT_SET = -1;

    private final Configuration mConfig;

    /** Current attention value. */
    @FloatRange(from = 0.0f, to = 1.0f)
    private float mAttention;

    /** Timestamp of last frame received, in milliseconds since boot. */
    private long mLastTimestamp = NOT_SET;

    GazeAttentionProcessor(Configuration configuration) {
        mConfig = configuration;
        mAttention = mConfig.initialValue;
    }

    /** Gets the current attention awareness value. */
    @FloatRange(from = 0.0f, to = 1.0f)
    public float getAttention() {
        return mAttention;
    }

    /**
     * Updates the current attention with a new gaze detection.
     *
     * @param detection New {@link android.car.occupantawareness.GazeDetection} data.
     * @param timestamp Timestamp that the detection was detected, in milliseconds since boot.
     * @return Attention value [0, 1].
     */
    @FloatRange(from = 0.0f, to = 1.0f)
    public float updateAttention(@NonNull GazeDetection detection, long timestamp) {
        // When the first frame of data is received, no duration can be computed. Just capture the
        // timestamp for the next pass and return the initial buffer value.
        if (mLastTimestamp == NOT_SET) {
            logd("First pass, returning 'initialValue=" + mConfig.initialValue);
            mLastTimestamp = timestamp;
            return mConfig.initialValue;
        }

        float startingAttention = mAttention;

        // Compute the time since the last detection, in seconds.
        float dtSeconds = (float) (timestamp - mLastTimestamp) / 1000.0f;

        if (isOnRoadGaze(detection.gazeTarget)) {
            logd("Handling on-road gaze");
            mAttention += dtSeconds * mConfig.growthRate;
            mAttention = Math.min(mAttention, 1.0f); // Cap value to max of 1.
        } else {
            logd("Handling off-road gaze");
            mAttention -= dtSeconds * mConfig.decayRate;
            mAttention = Math.max(mAttention, 0.0f); // Cap value to min of 0.
        }

        // Save current timestamp for next pass.
        mLastTimestamp = timestamp;

        logd(String.format("updateAttention(): Time=%1.2f. Attention [%f]->[%f]. ",
                dtSeconds, startingAttention, mAttention));

        return mAttention;
    }

    /** Gets whether the gaze target is on-road. */
    private boolean isOnRoadGaze(@GazeDetection.VehicleRegion int gazeTarget) {
        switch (gazeTarget) {
            case GazeDetection.VEHICLE_REGION_FORWARD_ROADWAY:
            case GazeDetection.VEHICLE_REGION_LEFT_ROADWAY:
            case GazeDetection.VEHICLE_REGION_RIGHT_ROADWAY:
                return true;
            default:
                return false;
        }
    }

    private static void logd(String message) {
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, message);
        }
    }

    /** Configuration settings for {@link GazeAttentionProcessor}. */
    public static class Configuration {
        /** Initial value for the attention buffer, [0, 1]. */
        @FloatRange(from = 0.0f, to = 1.0f)
        public final float initialValue;

        /**
         * Rate at which the attention decays when the driver is looking off-road (per second of
         * off-road looks)
         */
        public final float decayRate;

        /**
         * Rate at which the attention grows when the driver is looking on-road (per second of
         * on-road looks).
         */
        public final float growthRate;

        /**
         * Creates an instance of {@link Configuration}.
         *
         * @param initialValue the initial value that the attention buffer starts at.
         * @param decayRate the rate at which the attention buffer decreases when the driver is
         *     looking off-road.
         * @param growthRate the rate at which the attention buffer increases when the driver is
         *     looking on-road.
         */
        Configuration(float initialValue, float decayRate, float growthRate) {
            this.initialValue = initialValue;
            this.decayRate = decayRate;
            this.growthRate = growthRate;
        }

        @Override
        public String toString() {
            return String.format(
                    "Configuration{initialValue=%s, decayRate=%s, growthRate=%s}",
                    initialValue, decayRate, growthRate);
        }
    }
}
