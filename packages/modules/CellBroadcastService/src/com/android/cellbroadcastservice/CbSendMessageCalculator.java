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

package com.android.cellbroadcastservice;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.content.Context;
import android.telephony.CbGeoUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.stream.Collectors;

/**
 * Calculates whether or not to send the message according to the inputted geofence.
 *
 * Designed to be run multiple times with different calls to #addCoordinate
 *
 * @hide
 *
 */
public class CbSendMessageCalculator {

    @NonNull
    private final List<CbGeoUtils.Geometry> mFences;

    private final double mThresholdMeters;
    private int mAction = SEND_MESSAGE_ACTION_NO_COORDINATES;

    /*
    When false, we only check to see if a given coordinate falls within a geo or not.
    Put another way:
        1. The threshold is ignored
        2. Ambiguous results are never given
     */
    private final boolean mDoNewWay;

    public CbSendMessageCalculator(@NonNull final Context context,
            @NonNull final List<CbGeoUtils.Geometry> fences) {
        this(context, fences, context.getResources().getInteger(R.integer.geo_fence_threshold));
    }

    public CbSendMessageCalculator(@NonNull final Context context,
            @NonNull final List<CbGeoUtils.Geometry> fences, final double thresholdMeters) {

        mFences = fences.stream().filter(Objects::nonNull).collect(Collectors.toList());
        mThresholdMeters = thresholdMeters;
        mDoNewWay = context.getResources().getBoolean(R.bool.use_new_geo_fence_calculation);
    }

    /**
     * The given threshold the given coordinates can be outside the geo fence and still receive
     * {@code SEND_MESSAGE_ACTION_SEND}.
     *
     * @return the threshold in meters
     */
    public double getThreshold() {
        return mThresholdMeters;
    }

    /**
     * Gets the last action calculated
     *
     * @return last action
     */
    @SendMessageAction
    public int getAction() {
        if (mFences.size() == 0) {
            return SEND_MESSAGE_ACTION_SEND;
        }

        return mAction;
    }

    /**
     * Translates the action to a readable equivalent
     * @return readable version of action
     */
    String getActionString() {
        if (mAction == SEND_MESSAGE_ACTION_SEND) {
            return "SEND";
        } else if (mAction == SEND_MESSAGE_ACTION_AMBIGUOUS) {
            return "AMBIGUOUS";
        } else if (mAction == SEND_MESSAGE_ACTION_DONT_SEND) {
            return "DONT_SEND";
        } else if (mAction == SEND_MESSAGE_ACTION_NO_COORDINATES) {
            return "NO_COORDINATES";
        } else {
            return "!BAD_VALUE!";
        }
    }

    /** No Coordinates */
    public static final int SEND_MESSAGE_ACTION_NO_COORDINATES = 0;

    /** Send right away */
    public static final int SEND_MESSAGE_ACTION_SEND = 1;

    /** Stop waiting for results */
    public static final int SEND_MESSAGE_ACTION_DONT_SEND = 2;

    /** Continue polling */
    public static final int SEND_MESSAGE_ACTION_AMBIGUOUS = 3;

    /**
     * Send Message Action annotation
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({SEND_MESSAGE_ACTION_NO_COORDINATES, SEND_MESSAGE_ACTION_SEND,
            SEND_MESSAGE_ACTION_DONT_SEND, SEND_MESSAGE_ACTION_AMBIGUOUS,
    })
    public @interface SendMessageAction {}

    /**
     * Calculate action based off of the send reason.
     * @return
     */
    public void addCoordinate(CbGeoUtils.LatLng coordinate, double accuracyMeters) {
        if (mFences.size() == 0) {
            //No fences mean we shouldn't bother
            return;
        }

        calculatePersistentAction(coordinate, accuracyMeters);
    }

    /** Calculates the state of the next action based off of the new coordinate and the current
     * action state. According to the rules:
     * 1. SEND always wins
     * 2. Outside always trumps an overlap with DONT_SEND
     * 3. Otherwise we keep an overlap with AMBIGUOUS
     * @param coordinate the geo location
     * @param accuracyMeters the accuracy from location manager
     * @return the action
     */
    @SendMessageAction
    private void calculatePersistentAction(CbGeoUtils.LatLng coordinate, double accuracyMeters) {
        // If we already marked this as a send, we don't need to check anything.
        if (this.mAction != SEND_MESSAGE_ACTION_SEND) {
            @SendMessageAction int newAction =
                    calculateActionFromFences(coordinate, accuracyMeters);

            if (newAction == SEND_MESSAGE_ACTION_SEND) {
                /* If the new action is in SEND, it doesn't matter what the old action is is. */
                this.mAction = newAction;
            } else if (mAction != SEND_MESSAGE_ACTION_DONT_SEND) {
                /* If the old action is in DONT_SEND, then always overwrite it with ambiguous. */
                this.mAction = newAction;
            } else {
                /* No-op because if we are in a don't send state, we don't want to overwrite
                with an ambiguous state. */
            }
        }
    }

    /**
     * Calculates the proposed action state from the fences according to the rules:
     * 1. Any coordinate with a SEND always wins.
     * 2. If a coordinate \ accuracy overlaps any fence, go with AMBIGUOUS.
     * 3. Otherwise, the coordinate is very far outside every fence and we move to DONT_SEND.
     * @param coordinate the geo location
     * @param accuracyMeters the accuracy from location manager
     * @return the action
     */
    @SendMessageAction
    private int calculateActionFromFences(CbGeoUtils.LatLng coordinate, double accuracyMeters) {

        // If everything is outside, then we stick with outside
        int totalAction = SEND_MESSAGE_ACTION_DONT_SEND;
        for (int i = 0; i < mFences.size(); i++) {
            CbGeoUtils.Geometry fence = mFences.get(i);
            @SendMessageAction
            final int action = calculateSingleFence(coordinate, accuracyMeters, fence);

            if (action == SEND_MESSAGE_ACTION_SEND) {
                // The send action means we always go for it.
                return action;
            } else if (action == SEND_MESSAGE_ACTION_AMBIGUOUS) {
                // If we are outside a geo, but then find that the accuracies overlap,
                // we stick to overlap while still seeing if there are any cases where we are
                // inside
                totalAction = SEND_MESSAGE_ACTION_AMBIGUOUS;
            }
        }
        return totalAction;
    }

    @SendMessageAction
    private int calculateSingleFence(CbGeoUtils.LatLng coordinate, double accuracyMeters,
            CbGeoUtils.Geometry fence) {
        if (fence.contains(coordinate)) {
            return SEND_MESSAGE_ACTION_SEND;
        }

        if (mDoNewWay) {
            return calculateSysSingleFence(coordinate, accuracyMeters, fence);
        } else {
            return SEND_MESSAGE_ACTION_DONT_SEND;
        }
    }

    private int calculateSysSingleFence(CbGeoUtils.LatLng coordinate, double accuracyMeters,
            CbGeoUtils.Geometry fence) {
        Optional<Double> maybeDistance =
                com.android.cellbroadcastservice.CbGeoUtils.distance(fence, coordinate);
        if (!maybeDistance.isPresent()) {
            return SEND_MESSAGE_ACTION_DONT_SEND;
        }

        double distance = maybeDistance.get();
        if (accuracyMeters <= mThresholdMeters && distance <= mThresholdMeters) {
            // The accuracy is precise and we are within the threshold of the boundary, send
            return SEND_MESSAGE_ACTION_SEND;
        }

        if (distance <= accuracyMeters) {
            // Ambiguous case
            return SEND_MESSAGE_ACTION_AMBIGUOUS;
        } else {
            return SEND_MESSAGE_ACTION_DONT_SEND;
        }
    }
}
