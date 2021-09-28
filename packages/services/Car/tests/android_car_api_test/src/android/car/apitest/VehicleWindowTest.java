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

import android.car.VehicleAreaWindow;
import android.test.suitebuilder.annotation.SmallTest;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;

@SmallTest
public class VehicleWindowTest {

    @Test
    public void testMatchWithVehicleHal() {
        assertThat(VehicleAreaWindow.WINDOW_FRONT_WINDSHIELD).isEqualTo(
                android.hardware.automotive.vehicle.V2_0.VehicleAreaWindow.FRONT_WINDSHIELD);
        assertThat(VehicleAreaWindow.WINDOW_REAR_WINDSHIELD).isEqualTo(
                android.hardware.automotive.vehicle.V2_0.VehicleAreaWindow.REAR_WINDSHIELD);
        assertThat(VehicleAreaWindow.WINDOW_ROW_1_LEFT)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaWindow.ROW_1_LEFT);
        assertThat(VehicleAreaWindow.WINDOW_ROW_1_RIGHT)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaWindow.ROW_1_RIGHT);
        assertThat(VehicleAreaWindow.WINDOW_ROW_2_LEFT)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaWindow.ROW_2_LEFT);
        assertThat(VehicleAreaWindow.WINDOW_ROW_2_RIGHT)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaWindow.ROW_2_RIGHT);
        assertThat(VehicleAreaWindow.WINDOW_ROW_3_LEFT)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaWindow.ROW_3_LEFT);
        assertThat(VehicleAreaWindow.WINDOW_ROW_3_RIGHT)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaWindow.ROW_3_RIGHT);
        assertThat(VehicleAreaWindow.WINDOW_ROOF_TOP_1)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaWindow.ROOF_TOP_1);
        assertThat(VehicleAreaWindow.WINDOW_ROOF_TOP_2)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.VehicleAreaWindow.ROOF_TOP_2);
    }
}
