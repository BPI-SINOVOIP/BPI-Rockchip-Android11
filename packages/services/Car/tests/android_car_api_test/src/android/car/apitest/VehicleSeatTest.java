/*
 * Copyright (C) 2016 The Android Open Source Project
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
package android.car.apitest;

import android.car.VehicleAreaSeat;
import android.test.suitebuilder.annotation.SmallTest;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;

@SmallTest
public class VehicleSeatTest {

    @Test
    public void testMatchWithVehicleHal() {
        assertThat(VehicleAreaSeat.SEAT_ROW_1_LEFT)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaSeat.ROW_1_LEFT);
        assertThat(VehicleAreaSeat.SEAT_ROW_1_CENTER)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaSeat.ROW_1_CENTER);
        assertThat(VehicleAreaSeat.SEAT_ROW_1_RIGHT)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaSeat.ROW_1_RIGHT);
        assertThat(VehicleAreaSeat.SEAT_ROW_2_LEFT)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaSeat.ROW_2_LEFT);
        assertThat(VehicleAreaSeat.SEAT_ROW_2_CENTER)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaSeat.ROW_2_CENTER);
        assertThat(VehicleAreaSeat.SEAT_ROW_2_RIGHT)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaSeat.ROW_2_RIGHT);
        assertThat(VehicleAreaSeat.SEAT_ROW_3_LEFT)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaSeat.ROW_3_LEFT);
        assertThat(VehicleAreaSeat.SEAT_ROW_3_CENTER)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaSeat.ROW_3_CENTER);
        assertThat(VehicleAreaSeat.SEAT_ROW_3_RIGHT)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaSeat.ROW_3_RIGHT);
    }

    @Test
    public void testFromRowAndSide() {
        assertThat(VehicleAreaSeat.SEAT_UNKNOWN)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(-1, VehicleAreaSeat.SIDE_LEFT));
        assertThat(VehicleAreaSeat.SEAT_UNKNOWN)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(-1, VehicleAreaSeat.SIDE_CENTER));
        assertThat(VehicleAreaSeat.SEAT_UNKNOWN)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(-1, VehicleAreaSeat.SIDE_RIGHT));

        assertThat(VehicleAreaSeat.SEAT_UNKNOWN)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(0, VehicleAreaSeat.SIDE_LEFT));
        assertThat(VehicleAreaSeat.SEAT_UNKNOWN)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(0, VehicleAreaSeat.SIDE_CENTER));
        assertThat(VehicleAreaSeat.SEAT_UNKNOWN)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(0, VehicleAreaSeat.SIDE_RIGHT));

        assertThat(VehicleAreaSeat.SEAT_ROW_1_LEFT)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(1, VehicleAreaSeat.SIDE_LEFT));
        assertThat(VehicleAreaSeat.SEAT_ROW_1_CENTER)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(1, VehicleAreaSeat.SIDE_CENTER));
        assertThat(VehicleAreaSeat.SEAT_ROW_1_RIGHT)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(1, VehicleAreaSeat.SIDE_RIGHT));

        assertThat(VehicleAreaSeat.SEAT_ROW_2_LEFT)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(2, VehicleAreaSeat.SIDE_LEFT));
        assertThat(VehicleAreaSeat.SEAT_ROW_2_CENTER)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(2, VehicleAreaSeat.SIDE_CENTER));
        assertThat(VehicleAreaSeat.SEAT_ROW_2_RIGHT)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(2, VehicleAreaSeat.SIDE_RIGHT));

        assertThat(VehicleAreaSeat.SEAT_ROW_3_LEFT)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(3, VehicleAreaSeat.SIDE_LEFT));
        assertThat(VehicleAreaSeat.SEAT_ROW_3_CENTER)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(3, VehicleAreaSeat.SIDE_CENTER));
        assertThat(VehicleAreaSeat.SEAT_ROW_3_RIGHT)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(3, VehicleAreaSeat.SIDE_RIGHT));

        assertThat(VehicleAreaSeat.SEAT_UNKNOWN)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(4, VehicleAreaSeat.SIDE_LEFT));
        assertThat(VehicleAreaSeat.SEAT_UNKNOWN)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(4, VehicleAreaSeat.SIDE_CENTER));
        assertThat(VehicleAreaSeat.SEAT_UNKNOWN)
                .isEqualTo(VehicleAreaSeat.fromRowAndSide(4, VehicleAreaSeat.SIDE_RIGHT));
    }
}
