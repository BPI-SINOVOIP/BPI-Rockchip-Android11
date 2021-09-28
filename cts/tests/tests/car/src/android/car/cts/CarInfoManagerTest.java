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
package android.car.cts;

import static com.google.common.truth.Truth.assertThat;

import android.car.Car;
import android.car.CarInfoManager;
import android.car.EvConnectorType;
import android.car.FuelType;
import android.car.PortLocationType;
import android.car.VehicleAreaSeat;
import android.platform.test.annotations.RequiresDevice;
import android.test.suitebuilder.annotation.SmallTest;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.List;

@SmallTest
@RequiresDevice
@RunWith(AndroidJUnit4.class)
public class CarInfoManagerTest extends CarApiTestBase {

    private CarInfoManager mCarInfoManager;
    // SCAME EvConnectorType in VHAL
    private static final int SCAME = 11;
    // GBT_DC EvConnectorType in VHAL
    private static final int GBT_DC = 10;

    @Before
    public void setUp() throws Exception {
        super.setUp();
        mCarInfoManager = (CarInfoManager) getCar().getCarManager(Car.INFO_SERVICE);
    }

    @Test
    public void testVehicleId() throws Exception {
        // Not support to get vehicle Id.
        assertThat(mCarInfoManager.getVehicleId()).isEmpty();
    }

    @Test
    public void testGetManufacturer() throws Exception {
        assertThat(mCarInfoManager.getManufacturer()).isNotNull();;
    }

    @Test
    public void testGetModel() throws Exception {
        assertThat(mCarInfoManager.getModel()).isNotNull();
    }

    @Test
    public void testGetModelYear() throws Exception {
        assertThat(mCarInfoManager.getModelYear()).isNotNull();
        assertThat(mCarInfoManager.getModelYearInInteger()).isAtLeast(0);
    }

    @Test
    public void testGetFuelCapacity() throws Exception {
        assertThat(mCarInfoManager.getFuelCapacity()).isAtLeast(0f);
    }

    /**
     * Ensure return value is always in {@link FuelType}.
     * @throws Exception
     */
    @Test
    public void testGetFuelTypes() throws Exception {
        assertThat(mCarInfoManager.getFuelTypes()).isNotNull();
        int[] actualResults = mCarInfoManager.getFuelTypes();
        for (int result : actualResults) {
            assertThat(result).isIn(EXPECTED_FUEL_TYPES);
        }

    }


    @Test
    public void testGetEvBatteryCapacity() throws Exception {
        assertThat(mCarInfoManager.getEvBatteryCapacity()).isAtLeast(0f);
    }

    /**
     * Ensure return value is always in {@link EvConnectorType}.
     * @throws Exception
     */
    @Test
    public void testGetEvConnectorTypes() throws Exception {
        assertThat(mCarInfoManager.getEvConnectorTypes()).isNotNull();

        int[] actualResults = mCarInfoManager.getEvConnectorTypes();
        List<Integer> expectedResults =
            Arrays.asList(EvConnectorType.UNKNOWN, EvConnectorType.J1772, EvConnectorType.MENNEKES,
                EvConnectorType.CHADEMO, EvConnectorType.COMBO_1, EvConnectorType.COMBO_2,
                EvConnectorType.TESLA_ROADSTER, EvConnectorType.TESLA_HPWC,
                EvConnectorType.TESLA_SUPERCHARGER, EvConnectorType.GBT, EvConnectorType.OTHER,
                SCAME, GBT_DC);

        for (int result : actualResults) {
            assertThat(result).isIn(expectedResults);
        }
    }

    /**
     * Ensure return value is always in {@link VehicleAreaSeat}.
     * @throws Exception
     */
    @Test
    public void testGetDriverSeat() throws Exception {
        List<Integer> expectedResult =
            Arrays.asList(VehicleAreaSeat.SEAT_UNKNOWN, VehicleAreaSeat.SEAT_ROW_1_LEFT,
                VehicleAreaSeat.SEAT_ROW_1_CENTER, VehicleAreaSeat.SEAT_ROW_1_RIGHT,
                VehicleAreaSeat.SEAT_ROW_2_LEFT, VehicleAreaSeat.SEAT_ROW_2_CENTER,
                VehicleAreaSeat.SEAT_ROW_2_RIGHT, VehicleAreaSeat.SEAT_ROW_3_LEFT,
                VehicleAreaSeat.SEAT_ROW_3_CENTER, VehicleAreaSeat.SEAT_ROW_1_RIGHT);
        assertThat(mCarInfoManager.getDriverSeat()).isIn(expectedResult);
    }

    /**
     * Ensure return value is always in {@link PortLocationType}.
     * @throws Exception
     */
    @Test
    public void testGetEvPortLocation() throws Exception {
        assertThat(mCarInfoManager.getEvPortLocation()).isIn(EXPECTED_PORT_LOCATIONS);
    }

    /**
     * Ensure return value is always in {@link PortLocationType}.
     * @throws Exception
     */
    @Test
    public void testGetFuelDoorLocation() throws Exception {
        assertThat(mCarInfoManager.getFuelDoorLocation()).isIn(EXPECTED_PORT_LOCATIONS);
    }
}
