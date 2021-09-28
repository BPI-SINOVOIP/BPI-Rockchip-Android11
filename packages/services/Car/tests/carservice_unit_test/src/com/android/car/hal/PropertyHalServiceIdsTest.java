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

import android.car.Car;
import android.car.VehicleHvacFanDirection;
import android.car.VehiclePropertyIds;
import android.hardware.automotive.vehicle.V2_0.VehicleGear;
import android.hardware.automotive.vehicle.V2_0.VehiclePropValue;
import android.hardware.automotive.vehicle.V2_0.VehicleProperty;
import android.hardware.automotive.vehicle.V2_0.VehicleUnit;
import android.hardware.automotive.vehicle.V2_0.VehicleVendorPermission;
import android.os.SystemClock;
import android.util.Log;

import androidx.test.runner.AndroidJUnit4;

import com.android.car.vehiclehal.VehiclePropValueBuilder;

import com.google.common.truth.Truth;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import java.util.ArrayList;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class PropertyHalServiceIdsTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private PropertyHalServiceIds mPropertyHalServiceIds;

    private static final String TAG = PropertyHalServiceIdsTest.class.getSimpleName();
    private static final int VENDOR_PROPERTY_1 = 0x21e01111;
    private static final int VENDOR_PROPERTY_2 = 0x21e01112;
    private static final int VENDOR_PROPERTY_3 = 0x21e01113;
    private static final int VENDOR_PROPERTY_4 = 0x21e01114;
    private static final int[] VENDOR_PROPERTY_IDS = {
            VENDOR_PROPERTY_1, VENDOR_PROPERTY_2, VENDOR_PROPERTY_3, VENDOR_PROPERTY_4};
    private static final int[] SYSTEM_PROPERTY_IDS = {VehiclePropertyIds.ENGINE_OIL_LEVEL,
            VehiclePropertyIds.CURRENT_GEAR, VehiclePropertyIds.NIGHT_MODE,
            VehiclePropertyIds.HVAC_FAN_SPEED, VehiclePropertyIds.DOOR_LOCK};
    private static final List<Integer> CONFIG_ARRAY = new ArrayList<>();
    private static final List<Integer> CONFIG_ARRAY_INVALID = new ArrayList<>();
    //payload test
    private static final VehiclePropValue GEAR_WITH_VALID_VALUE =
            VehiclePropValueBuilder.newBuilder(VehicleProperty.GEAR_SELECTION)
            .addIntValue(VehicleGear.GEAR_DRIVE)
            .setTimestamp(SystemClock.elapsedRealtimeNanos()).build();
    private static final VehiclePropValue GEAR_WITH_EXTRA_VALUE =
            VehiclePropValueBuilder.newBuilder(VehicleProperty.GEAR_SELECTION)
                    .addIntValue(VehicleGear.GEAR_DRIVE)
                    .addIntValue(VehicleGear.GEAR_1)
                    .setTimestamp(SystemClock.elapsedRealtimeNanos()).build();
    private static final VehiclePropValue GEAR_WITH_INVALID_VALUE =
            VehiclePropValueBuilder.newBuilder(VehicleProperty.GEAR_SELECTION)
                    .addIntValue(VehicleUnit.KILOPASCAL)
                    .setTimestamp(SystemClock.elapsedRealtimeNanos()).build();
    private static final VehiclePropValue GEAR_WITH_INVALID_TYPE_VALUE =
            VehiclePropValueBuilder.newBuilder(VehicleProperty.GEAR_SELECTION)
                    .addFloatValue(1.0f)
                    .setTimestamp(SystemClock.elapsedRealtimeNanos()).build();
    private static final VehiclePropValue HVAC_FAN_DIRECTIONS_VALID =
            VehiclePropValueBuilder.newBuilder(VehicleProperty.HVAC_FAN_DIRECTION)
                    .addIntValue(VehicleHvacFanDirection.FACE | VehicleHvacFanDirection.FLOOR)
                    .setTimestamp(SystemClock.elapsedRealtimeNanos()).build();
    private static final VehiclePropValue HVAC_FAN_DIRECTIONS_INVALID =
            VehiclePropValueBuilder.newBuilder(VehicleProperty.HVAC_FAN_DIRECTION)
                    .addIntValue(VehicleHvacFanDirection.FACE | 0x100)
                    .setTimestamp(SystemClock.elapsedRealtimeNanos()).build();
    @Before
    public void setUp() {
        mPropertyHalServiceIds = new PropertyHalServiceIds();
        // set up read permission and write permission to VENDOR_PROPERTY_1
        CONFIG_ARRAY.add(VENDOR_PROPERTY_1);
        CONFIG_ARRAY.add(VehicleVendorPermission.PERMISSION_DEFAULT);
        CONFIG_ARRAY.add(VehicleVendorPermission.PERMISSION_NOT_ACCESSIBLE);
        // set up read permission and write permission to VENDOR_PROPERTY_2
        CONFIG_ARRAY.add(VENDOR_PROPERTY_2);
        CONFIG_ARRAY.add(VehicleVendorPermission.PERMISSION_GET_VENDOR_CATEGORY_ENGINE);
        CONFIG_ARRAY.add(VehicleVendorPermission.PERMISSION_SET_VENDOR_CATEGORY_ENGINE);
        // set up read permission and write permission to VENDOR_PROPERTY_3
        CONFIG_ARRAY.add(VENDOR_PROPERTY_3);
        CONFIG_ARRAY.add(VehicleVendorPermission.PERMISSION_GET_VENDOR_CATEGORY_INFO);
        CONFIG_ARRAY.add(VehicleVendorPermission.PERMISSION_DEFAULT);

        // set a invalid config
        CONFIG_ARRAY_INVALID.add(VehiclePropertyIds.CURRENT_GEAR);
        CONFIG_ARRAY_INVALID.add(VehicleVendorPermission.PERMISSION_GET_VENDOR_CATEGORY_ENGINE);
        CONFIG_ARRAY_INVALID.add(VehicleVendorPermission.PERMISSION_SET_VENDOR_CATEGORY_ENGINE);
    }

    /**
     * Test {@link PropertyHalServiceIds#getReadPermission(int)}
     * and {@link PropertyHalServiceIds#getWritePermission(int)} for system properties
     */
    @Test
    public void checkPermissionForSystemProperty() {
        Assert.assertEquals(Car.PERMISSION_CAR_ENGINE_DETAILED,
                mPropertyHalServiceIds.getReadPermission(VehiclePropertyIds.ENGINE_OIL_LEVEL));
        Assert.assertNull(
                mPropertyHalServiceIds.getWritePermission(VehiclePropertyIds.ENGINE_OIL_LEVEL));
        Assert.assertEquals(Car.PERMISSION_CONTROL_CAR_CLIMATE,
                mPropertyHalServiceIds.getReadPermission(VehiclePropertyIds.HVAC_FAN_SPEED));
        Assert.assertEquals(Car.PERMISSION_CONTROL_CAR_CLIMATE,
                mPropertyHalServiceIds.getWritePermission(VehiclePropertyIds.HVAC_FAN_SPEED));
    }
    /**
     * Test {@link PropertyHalServiceIds#customizeVendorPermission(List)}
     */
    @Test
    public void checkPermissionForVendorProperty() {
        // test insert a valid config
        mPropertyHalServiceIds.customizeVendorPermission(CONFIG_ARRAY);

        Assert.assertEquals(Car.PERMISSION_VENDOR_EXTENSION,
                mPropertyHalServiceIds.getReadPermission(VENDOR_PROPERTY_1));
        Assert.assertNull(mPropertyHalServiceIds.getWritePermission(VENDOR_PROPERTY_1));

        Assert.assertEquals(android.car.hardware.property
                        .VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_ENGINE,
                mPropertyHalServiceIds.getReadPermission(VENDOR_PROPERTY_2));
        Assert.assertEquals(android.car.hardware.property
                        .VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_ENGINE,
                mPropertyHalServiceIds.getWritePermission(VENDOR_PROPERTY_2));

        Assert.assertEquals(android.car.hardware.property
                        .VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_INFO,
                mPropertyHalServiceIds.getReadPermission(VENDOR_PROPERTY_3));
        Assert.assertEquals(Car.PERMISSION_VENDOR_EXTENSION,
                mPropertyHalServiceIds.getWritePermission(VENDOR_PROPERTY_3));

        Assert.assertEquals(Car.PERMISSION_VENDOR_EXTENSION,
                mPropertyHalServiceIds.getReadPermission(VENDOR_PROPERTY_4));
        Assert.assertEquals(Car.PERMISSION_VENDOR_EXTENSION,
                mPropertyHalServiceIds.getWritePermission(VENDOR_PROPERTY_4));

        // test insert invalid config
        try {
            mPropertyHalServiceIds.customizeVendorPermission(CONFIG_ARRAY_INVALID);
            Assert.fail("Insert system properties with vendor permissions");
        } catch (IllegalArgumentException e) {
            Log.v(TAG, e.getMessage());
        }
    }

    /**
     * Test {@link PropertyHalServiceIds#isSupportedProperty(int)}
     */
    @Test
    public void checkVendorPropertyId() {
        for (int vendorProp : VENDOR_PROPERTY_IDS) {
            Assert.assertTrue(mPropertyHalServiceIds.isSupportedProperty(vendorProp));
        }
        for (int systemProp : SYSTEM_PROPERTY_IDS) {
            Assert.assertTrue(mPropertyHalServiceIds.isSupportedProperty(systemProp));
        }
    }

    /**
     * Test {@link PropertyHalServiceIds#checkPayload(VehiclePropValue)}
     */
    @Test
    public void testPayload() {
        Truth.assertThat(mPropertyHalServiceIds.checkPayload(GEAR_WITH_VALID_VALUE)).isTrue();
        Truth.assertThat(mPropertyHalServiceIds.checkPayload(GEAR_WITH_EXTRA_VALUE)).isFalse();
        Truth.assertThat(mPropertyHalServiceIds.checkPayload(GEAR_WITH_INVALID_VALUE)).isFalse();
        Truth.assertThat(mPropertyHalServiceIds.checkPayload(GEAR_WITH_INVALID_TYPE_VALUE))
                .isFalse();

        Truth.assertThat(mPropertyHalServiceIds.checkPayload(HVAC_FAN_DIRECTIONS_VALID)).isTrue();
        Truth.assertThat(mPropertyHalServiceIds.checkPayload(HVAC_FAN_DIRECTIONS_INVALID))
                .isFalse();
    }
}
