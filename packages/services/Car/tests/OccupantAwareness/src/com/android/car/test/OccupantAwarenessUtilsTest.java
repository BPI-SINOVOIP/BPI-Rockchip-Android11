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
package com.android.car;

import static com.google.common.truth.Truth.assertThat;

import android.car.occupantawareness.OccupantAwarenessDetection;
import android.car.occupantawareness.SystemStatusEvent;
import android.hardware.automotive.occupant_awareness.IOccupantAwareness;
import android.hardware.automotive.occupant_awareness.OccupantAwarenessStatus;
import android.test.suitebuilder.annotation.MediumTest;

import androidx.test.runner.AndroidJUnit4;

import junit.framework.TestCase;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@MediumTest
public class OccupantAwarenessUtilsTest extends TestCase {

    @Test
    public void test_convertToStatusEvent() {
        SystemStatusEvent event;

        event =
                OccupantAwarenessUtils.convertToStatusEvent(
                        IOccupantAwareness.CAP_NONE, OccupantAwarenessStatus.READY);
        assertThat(event.systemStatus).isEqualTo(SystemStatusEvent.SYSTEM_STATUS_READY);
        assertThat(event.detectionType).isEqualTo(SystemStatusEvent.DETECTION_TYPE_NONE);

        event =
                OccupantAwarenessUtils.convertToStatusEvent(
                        IOccupantAwareness.CAP_PRESENCE_DETECTION,
                        OccupantAwarenessStatus.NOT_SUPPORTED);
        assertThat(event.systemStatus).isEqualTo(SystemStatusEvent.SYSTEM_STATUS_NOT_SUPPORTED);
        assertThat(event.detectionType).isEqualTo(SystemStatusEvent.DETECTION_TYPE_PRESENCE);

        event =
                OccupantAwarenessUtils.convertToStatusEvent(
                        IOccupantAwareness.CAP_GAZE_DETECTION,
                        OccupantAwarenessStatus.NOT_INITIALIZED);
        assertThat(event.systemStatus).isEqualTo(SystemStatusEvent.SYSTEM_STATUS_NOT_READY);
        assertThat(event.detectionType).isEqualTo(SystemStatusEvent.DETECTION_TYPE_GAZE);

        event =
                OccupantAwarenessUtils.convertToStatusEvent(
                        IOccupantAwareness.CAP_DRIVER_MONITORING_DETECTION,
                        OccupantAwarenessStatus.FAILURE);
        assertThat(event.systemStatus).isEqualTo(SystemStatusEvent.SYSTEM_STATUS_SYSTEM_FAILURE);
        assertThat(event.detectionType)
                .isEqualTo(SystemStatusEvent.DETECTION_TYPE_DRIVER_MONITORING);
    }

    @Test
    public void test_convertToConfidenceScore() {
        assertThat(
                        OccupantAwarenessUtils.convertToConfidenceScore(
                                android.hardware.automotive.occupant_awareness.ConfidenceLevel.MAX))
                .isEqualTo(OccupantAwarenessDetection.CONFIDENCE_LEVEL_MAX);

        assertThat(
                        OccupantAwarenessUtils.convertToConfidenceScore(
                                android.hardware.automotive.occupant_awareness.ConfidenceLevel
                                        .HIGH))
                .isEqualTo(OccupantAwarenessDetection.CONFIDENCE_LEVEL_HIGH);

        assertThat(
                        OccupantAwarenessUtils.convertToConfidenceScore(
                                android.hardware.automotive.occupant_awareness.ConfidenceLevel.LOW))
                .isEqualTo(OccupantAwarenessDetection.CONFIDENCE_LEVEL_LOW);

        assertThat(
                        OccupantAwarenessUtils.convertToConfidenceScore(
                                android.hardware.automotive.occupant_awareness.ConfidenceLevel
                                        .NONE))
                .isEqualTo(OccupantAwarenessDetection.CONFIDENCE_LEVEL_NONE);
    }

    @Test
    public void test_convertToPoint3D() {
        assertThat(OccupantAwarenessUtils.convertToPoint3D(null)).isNull();
        assertThat(OccupantAwarenessUtils.convertToPoint3D(new double[0])).isNull();
        assertThat(OccupantAwarenessUtils.convertToPoint3D(new double[2])).isNull();
        assertThat(OccupantAwarenessUtils.convertToPoint3D(new double[] {1, 2, 3})).isNotNull();
    }

    @Test
    public void test_convertToRole() {
        assertThat(
                        OccupantAwarenessUtils.convertToRole(
                                android.hardware.automotive.occupant_awareness.Role.INVALID))
                .isEqualTo(OccupantAwarenessDetection.VEHICLE_OCCUPANT_NONE);

        assertThat(
                        OccupantAwarenessUtils.convertToRole(
                                android.hardware.automotive.occupant_awareness.Role.UNKNOWN))
                .isEqualTo(OccupantAwarenessDetection.VEHICLE_OCCUPANT_NONE);

        assertThat(
                        OccupantAwarenessUtils.convertToRole(
                                android.hardware.automotive.occupant_awareness.Role.DRIVER
                                        | android.hardware.automotive.occupant_awareness.Role
                                                .FRONT_PASSENGER
                                        | android.hardware.automotive.occupant_awareness.Role
                                                .ROW_2_PASSENGER_CENTER))
                .isEqualTo(
                        OccupantAwarenessDetection.VEHICLE_OCCUPANT_DRIVER
                                | OccupantAwarenessDetection.VEHICLE_OCCUPANT_FRONT_PASSENGER
                                | OccupantAwarenessDetection
                                        .VEHICLE_OCCUPANT_ROW_2_PASSENGER_CENTER);

        assertThat(
                        OccupantAwarenessUtils.convertToRole(
                                android.hardware.automotive.occupant_awareness.Role.ALL_OCCUPANTS))
                .isEqualTo(OccupantAwarenessDetection.VEHICLE_OCCUPANT_ALL_OCCUPANTS);
    }
}
