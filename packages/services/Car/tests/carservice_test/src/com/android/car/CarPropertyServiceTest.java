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

import static com.android.dx.mockito.inline.extended.ExtendedMockito.spyOn;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.verify;

import static java.lang.Integer.toHexString;

import android.hardware.automotive.vehicle.V2_0.VehicleGear;
import android.hardware.automotive.vehicle.V2_0.VehiclePropValue;
import android.hardware.automotive.vehicle.V2_0.VehicleProperty;
import android.os.SystemClock;
import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.MediumTest;

import com.android.car.vehiclehal.VehiclePropValueBuilder;
import com.android.car.vehiclehal.test.MockedVehicleHal.VehicleHalPropertyHandler;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Test for {@link com.android.car.CarPropertyService}
 */
@RunWith(AndroidJUnit4.class)
@MediumTest
public class CarPropertyServiceTest extends MockedCarTestBase {
    private static final String TAG = CarPropertyServiceTest.class.getSimpleName();

    private final Map<Integer, VehiclePropValue> mDefaultPropValues = new HashMap<>();

    private CarPropertyService mService;

    public CarPropertyServiceTest() {
        // Unusual default values for the vehicle properties registered to listen via
        // CarPropertyService.registerListener. Unusual default values like the car is in motion,
        // night mode is on, or the car is low on fuel.
        mDefaultPropValues.put(VehicleProperty.GEAR_SELECTION,
                VehiclePropValueBuilder.newBuilder(VehicleProperty.GEAR_SELECTION)
                .addIntValue(VehicleGear.GEAR_DRIVE)
                .setTimestamp(SystemClock.elapsedRealtimeNanos()).build());
        mDefaultPropValues.put(VehicleProperty.PARKING_BRAKE_ON,
                VehiclePropValueBuilder.newBuilder(VehicleProperty.PARKING_BRAKE_ON)
                .setBooleanValue(false)
                .setTimestamp(SystemClock.elapsedRealtimeNanos()).build());
        mDefaultPropValues.put(VehicleProperty.PERF_VEHICLE_SPEED,
                VehiclePropValueBuilder.newBuilder(VehicleProperty.PERF_VEHICLE_SPEED)
                .addFloatValue(30.0f)
                .setTimestamp(SystemClock.elapsedRealtimeNanos()).build());
        mDefaultPropValues.put(VehicleProperty.NIGHT_MODE,
                VehiclePropValueBuilder.newBuilder(VehicleProperty.NIGHT_MODE)
                .setBooleanValue(true)
                .setTimestamp(SystemClock.elapsedRealtimeNanos()).build());
    }

    @Override
    protected synchronized void configureMockedHal() {
        PropertyHandler handler = new PropertyHandler();
        for (VehiclePropValue value : mDefaultPropValues.values()) {
            handler.onPropertySet(value);
            addProperty(value.prop, handler);
        }
    }

    @Override
    protected synchronized void spyOnBeforeCarImplInit() {
        mService = getCarPropertyService();
        assertThat(mService).isNotNull();
        spyOn(mService);
    }

    @Test
    public void testMatchesDefaultPropertyValues() {
        Set<Integer> expectedPropIds = mDefaultPropValues.keySet();
        ArgumentCaptor<Integer> propIdCaptor = ArgumentCaptor.forClass(Integer.class);
        verify(mService, atLeast(expectedPropIds.size())).registerListener(
                propIdCaptor.capture(), anyFloat(), any());

        Set<Integer> actualPropIds = new HashSet<Integer>(propIdCaptor.getAllValues());
        assertWithMessage("Should assign default values for missing property IDs")
                .that(expectedPropIds).containsAllIn(actualPropIds.toArray());
        assertWithMessage("Missing registerListener for property IDs")
                .that(actualPropIds).containsAllIn(expectedPropIds.toArray());
    }

    private static final class PropertyHandler implements VehicleHalPropertyHandler {
        private final Map<Integer, VehiclePropValue> mMap = new HashMap<>();

        @Override
        public synchronized void onPropertySet(VehiclePropValue value) {
            mMap.put(value.prop, value);
        }

        @Override
        public synchronized VehiclePropValue onPropertyGet(VehiclePropValue value) {
            assertWithMessage("onPropertyGet missing property: %s", toHexString(value.prop))
                    .that(mMap).containsKey(value.prop);
            VehiclePropValue currentValue = mMap.get(value.prop);
            return currentValue != null ? currentValue : value;
        }

        @Override
        public synchronized void onPropertySubscribe(int property, float sampleRate) {
            assertWithMessage("onPropertySubscribe missing property: %s", toHexString(property))
                    .that(mMap).containsKey(property);
            Log.d(TAG, "onPropertySubscribe property "
                    + property + " sampleRate " + sampleRate);
        }

        @Override
        public synchronized void onPropertyUnsubscribe(int property) {
            assertWithMessage("onPropertyUnsubscribe missing property: %s", toHexString(property))
                    .that(mMap).containsKey(property);
            Log.d(TAG, "onPropertyUnSubscribe property " + property);
        }
    }
}
