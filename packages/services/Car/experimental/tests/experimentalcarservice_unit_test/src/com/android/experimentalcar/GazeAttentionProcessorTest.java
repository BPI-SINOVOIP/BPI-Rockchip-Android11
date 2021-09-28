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

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertTrue;

import android.car.occupantawareness.GazeDetection;
import android.car.occupantawareness.OccupantAwarenessDetection;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnitRunner;

@RunWith(MockitoJUnitRunner.class)
public class GazeAttentionProcessorTest {

    private static final long FRAME_TIME_MILLIS = 1000; // Milliseconds
    private static final float INITIAL_VALUE = 1.0f;
    private static final float GROWTH_RATE = 0.4f;
    private static final float DECAY_RATE = 0.6f;

    private GazeAttentionProcessor mAttentionProcessor;

    @Before
    public void setUp() {
        mAttentionProcessor =
                new GazeAttentionProcessor(
                        new GazeAttentionProcessor.Configuration(
                                INITIAL_VALUE, DECAY_RATE, GROWTH_RATE));
    }

    @Test
    public void test_initialValue() {
        // Initial attention value should match input configuration.
        assertThat(mAttentionProcessor.getAttention()).isEqualTo(INITIAL_VALUE);
    }

    @Test
    public void testUpdateAttention_neverExceedsOne() {
        // Maximum awareness should never exceed '1'.
        long timestamp = 1234;
        final GazeDetection onRoadDetection =
                buildGazeDetection(GazeDetection.VEHICLE_REGION_FORWARD_ROADWAY);

        // First pass should return initial value.
        assertThat(mAttentionProcessor.updateAttention(onRoadDetection, timestamp))
                .isEqualTo(INITIAL_VALUE);
        timestamp += FRAME_TIME_MILLIS;

        for (int i = 0; i < 100; i++) {
            // Running on-road again with maxed out attention should max out at '1'.
            assertTrue(mAttentionProcessor.updateAttention(onRoadDetection, timestamp) <= 1.0f);
            timestamp += FRAME_TIME_MILLIS;
        }
    }

    @Test
    public void testUpdateAttention_neverBelowZero() {
        // Minimum awareness should never fall below '0'.
        long timestamp = 1234;
        final GazeDetection offRoadDetection =
                buildGazeDetection(GazeDetection.VEHICLE_REGION_HEAD_UNIT_DISPLAY);

        // First pass should return initial value.
        assertThat(mAttentionProcessor.updateAttention(offRoadDetection, timestamp))
                .isEqualTo(INITIAL_VALUE);
        timestamp += FRAME_TIME_MILLIS;

        for (int i = 0; i < 100; i++) {
            // Running on-road again with maxed out attention should max out at '1'.
            assertTrue(mAttentionProcessor.updateAttention(offRoadDetection, timestamp) >= 0);
            timestamp += FRAME_TIME_MILLIS;
        }
    }

    @Test
    public void testUpdateAttention_offRoadAndOnRoad() {

        final GazeDetection onRoadDetection =
                buildGazeDetection(GazeDetection.VEHICLE_REGION_FORWARD_ROADWAY);

        final GazeDetection offRoadDetection =
                buildGazeDetection(GazeDetection.VEHICLE_REGION_HEAD_UNIT_DISPLAY);

        long timestamp = 1234;

        // First pass should return initial value.
        assertThat(mAttentionProcessor.updateAttention(onRoadDetection, timestamp))
                .isEqualTo(INITIAL_VALUE);
        timestamp += FRAME_TIME_MILLIS;

        // Looking off-road should decay the attention.
        assertThat(mAttentionProcessor.updateAttention(offRoadDetection, timestamp))
                .isEqualTo(1.0f - DECAY_RATE);
        timestamp += FRAME_TIME_MILLIS;

        // Looking back up at the roadway should increase the buffer.
        assertThat(mAttentionProcessor.updateAttention(onRoadDetection, timestamp))
                .isEqualTo(1.0f - DECAY_RATE + GROWTH_RATE);
        timestamp += FRAME_TIME_MILLIS;
    }

    /** Builds a {link GazeDetection} with the specified target for testing. */
    private GazeDetection buildGazeDetection(@GazeDetection.VehicleRegion int gazeTarget) {
        return new GazeDetection(
                OccupantAwarenessDetection.CONFIDENCE_LEVEL_HIGH,
                null /*leftEyePosition*/,
                null /*rightEyePosition*/,
                null /*headAngleUnitVector*/,
                null /*gazeAngleUnitVector*/,
                gazeTarget,
                FRAME_TIME_MILLIS);
    }
}
