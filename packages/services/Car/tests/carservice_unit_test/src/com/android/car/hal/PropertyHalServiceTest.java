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

package com.android.car.hal;

import android.hardware.automotive.vehicle.V2_0.VehicleProperty;

import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(AndroidJUnit4.class)
public class PropertyHalServiceTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private VehicleHal mVehicleHal;

    private PropertyHalService mPropertyHalService;
    private static final int[] UNITS_PROPERTY_ID = {
            VehicleProperty.DISTANCE_DISPLAY_UNITS,
            VehicleProperty.FUEL_CONSUMPTION_UNITS_DISTANCE_OVER_VOLUME,
            VehicleProperty.FUEL_VOLUME_DISPLAY_UNITS,
            VehicleProperty.TIRE_PRESSURE_DISPLAY_UNITS,
            VehicleProperty.EV_BATTERY_DISPLAY_UNITS,
            VehicleProperty.VEHICLE_SPEED_DISPLAY_UNITS};

    @Before
    public void setUp() {
        mPropertyHalService = new PropertyHalService(mVehicleHal);
        mPropertyHalService.init();
    }

    @After
    public void tearDown() {
        mPropertyHalService.release();
        mPropertyHalService = null;
    }

    @Test
    public void checkDisplayUnitsProperty() {
        for (int propId : UNITS_PROPERTY_ID) {
            Assert.assertTrue(mPropertyHalService.isDisplayUnitsProperty(propId));
        }
    }
}
