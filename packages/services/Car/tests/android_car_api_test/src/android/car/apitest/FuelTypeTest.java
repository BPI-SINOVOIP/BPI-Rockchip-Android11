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

package android.car.apitest;

import android.car.FuelType;
import android.test.suitebuilder.annotation.SmallTest;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;

@SmallTest
public final class FuelTypeTest {
    @Test
    public void testMatchWithVehicleHal() {
        assertThat(FuelType.UNKNOWN)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.FuelType.FUEL_TYPE_UNKNOWN);

        assertThat(FuelType.UNLEADED)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.FuelType.FUEL_TYPE_UNLEADED);

        assertThat(FuelType.LEADED)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.FuelType.FUEL_TYPE_LEADED);

        assertThat(FuelType.DIESEL_1)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.FuelType.FUEL_TYPE_DIESEL_1);

        assertThat(FuelType.DIESEL_2)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.FuelType.FUEL_TYPE_DIESEL_2);

        assertThat(FuelType.BIODIESEL)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.FuelType.FUEL_TYPE_BIODIESEL);

        assertThat(FuelType.E85)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.FuelType.FUEL_TYPE_E85);

        assertThat(FuelType.LPG)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.FuelType.FUEL_TYPE_LPG);

        assertThat(FuelType.CNG)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.FuelType.FUEL_TYPE_CNG);

        assertThat(FuelType.LNG)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.FuelType.FUEL_TYPE_LNG);

        assertThat(FuelType.ELECTRIC)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.FuelType.FUEL_TYPE_ELECTRIC);

        assertThat(FuelType.HYDROGEN)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.FuelType.FUEL_TYPE_HYDROGEN);

        assertThat(FuelType.OTHER)
                .isEqualTo(android.hardware.automotive.vehicle.V2_0.FuelType.FUEL_TYPE_OTHER);
    }
}
