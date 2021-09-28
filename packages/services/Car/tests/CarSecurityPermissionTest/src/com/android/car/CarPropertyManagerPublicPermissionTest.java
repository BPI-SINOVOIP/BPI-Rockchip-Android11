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
import static com.google.common.truth.Truth.assertWithMessage;

import android.car.Car;
import android.car.VehicleAreaType;
import android.car.VehiclePropertyIds;
import android.car.VehiclePropertyType;
import android.car.hardware.property.CarPropertyManager;
import android.os.Handler;
import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.HashSet;

/**
 * This class contains security permission tests for the {@link CarPropertyManager}'s public APIs.
 */
@RunWith(AndroidJUnit4.class)
public class CarPropertyManagerPublicPermissionTest {
    private Car mCar = null;
    private CarPropertyManager mPropertyManager;
    private HashSet<Integer> mProps = new HashSet<>();
    private static final String TAG = CarPropertyManagerPublicPermissionTest.class.getSimpleName();
    private static final Integer DUMMY_AREA_ID = VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL;
    // Dummy values for setter test.
    private static final int DUMMY_PROPERTY_VALUE_INTEGER = 1;
    private static final Integer[] DUMMY_PROPERTY_VALUE_INTEGER_ARRAY = new Integer[]{1};
    private static final float DUMMY_PROPERTY_VALUE_FLOAT = 1.0f;
    private static final Float[] DUMMY_PROPERTY_VALUE_FLOAT_ARRAY = new Float[]{1.0f};
    private static final Long DUMMY_PROPERTY_VALUE_LONG = 1L;
    private static final Long[] DUMMY_PROPERTY_VALUE_LONG_ARRAY = new Long[]{1L};
    private static final boolean DUMMY_PROPERTY_VALUE_BOOLEAN = true;
    private static final String DUMMY_PROPERTY_VALUE_STRING = "test";
    private static final byte[] DUMMY_PROPERTY_VALUE_BYTE_ARRAY = "test".getBytes();
    private static final Object[] DUMMY_PROPERTY_VALUE_OBJECT_ARRAY = new Object[]{1, "test"};

    @Before
    public void setUp() throws Exception  {
        initAllPropertyIds();
        mCar = Car.createCar(
                InstrumentationRegistry.getInstrumentation().getTargetContext(), (Handler) null);
        mPropertyManager = (CarPropertyManager) mCar.getCarManager(Car.PROPERTY_SERVICE);
        assertThat(mPropertyManager).isNotNull();
    }

    private synchronized void initAllPropertyIds() {
        mProps.add(VehiclePropertyIds.DOOR_POS);
        mProps.add(VehiclePropertyIds.DOOR_MOVE);
        mProps.add(VehiclePropertyIds.DOOR_LOCK);
        mProps.add(VehiclePropertyIds.MIRROR_Z_POS);
        mProps.add(VehiclePropertyIds.MIRROR_Z_MOVE);
        mProps.add(VehiclePropertyIds.MIRROR_Y_POS);
        mProps.add(VehiclePropertyIds.MIRROR_Y_MOVE);
        mProps.add(VehiclePropertyIds.MIRROR_LOCK);
        mProps.add(VehiclePropertyIds.MIRROR_FOLD);
        mProps.add(VehiclePropertyIds.SEAT_MEMORY_SELECT);
        mProps.add(VehiclePropertyIds.SEAT_MEMORY_SET);
        mProps.add(VehiclePropertyIds.SEAT_BELT_BUCKLED);
        mProps.add(VehiclePropertyIds.SEAT_BELT_HEIGHT_POS);
        mProps.add(VehiclePropertyIds.SEAT_BELT_HEIGHT_MOVE);
        mProps.add(VehiclePropertyIds.SEAT_FORE_AFT_POS);
        mProps.add(VehiclePropertyIds.SEAT_FORE_AFT_MOVE);
        mProps.add(VehiclePropertyIds.SEAT_BACKREST_ANGLE_1_POS);
        mProps.add(VehiclePropertyIds.SEAT_BACKREST_ANGLE_1_MOVE);
        mProps.add(VehiclePropertyIds.SEAT_BACKREST_ANGLE_2_POS);
        mProps.add(VehiclePropertyIds.SEAT_BACKREST_ANGLE_2_MOVE);
        mProps.add(VehiclePropertyIds.SEAT_HEIGHT_POS);
        mProps.add(VehiclePropertyIds.SEAT_HEIGHT_MOVE);
        mProps.add(VehiclePropertyIds.SEAT_DEPTH_POS);
        mProps.add(VehiclePropertyIds.SEAT_DEPTH_MOVE);
        mProps.add(VehiclePropertyIds.SEAT_TILT_POS);
        mProps.add(VehiclePropertyIds.SEAT_TILT_MOVE);
        mProps.add(VehiclePropertyIds.SEAT_LUMBAR_FORE_AFT_POS);
        mProps.add(VehiclePropertyIds.SEAT_LUMBAR_FORE_AFT_MOVE);
        mProps.add(VehiclePropertyIds.SEAT_LUMBAR_SIDE_SUPPORT_POS);
        mProps.add(VehiclePropertyIds.SEAT_LUMBAR_SIDE_SUPPORT_MOVE);
        mProps.add(VehiclePropertyIds.SEAT_HEADREST_HEIGHT_POS);
        mProps.add(VehiclePropertyIds.SEAT_HEADREST_HEIGHT_MOVE);
        mProps.add(VehiclePropertyIds.SEAT_HEADREST_ANGLE_POS);
        mProps.add(VehiclePropertyIds.SEAT_HEADREST_ANGLE_MOVE);
        mProps.add(VehiclePropertyIds.SEAT_HEADREST_FORE_AFT_POS);
        mProps.add(VehiclePropertyIds.SEAT_HEADREST_FORE_AFT_MOVE);
        mProps.add(VehiclePropertyIds.SEAT_OCCUPANCY);
        mProps.add(VehiclePropertyIds.WINDOW_POS);
        mProps.add(VehiclePropertyIds.WINDOW_MOVE);
        mProps.add(VehiclePropertyIds.WINDOW_LOCK);

        // HVAC properties
        mProps.add(VehiclePropertyIds.HVAC_FAN_SPEED);
        mProps.add(VehiclePropertyIds.HVAC_FAN_DIRECTION);
        mProps.add(VehiclePropertyIds.HVAC_TEMPERATURE_CURRENT);
        mProps.add(VehiclePropertyIds.HVAC_TEMPERATURE_SET);
        mProps.add(VehiclePropertyIds.HVAC_DEFROSTER);
        mProps.add(VehiclePropertyIds.HVAC_ELECTRIC_DEFROSTER_ON);
        mProps.add(VehiclePropertyIds.HVAC_AC_ON);
        mProps.add(VehiclePropertyIds.HVAC_MAX_AC_ON);
        mProps.add(VehiclePropertyIds.HVAC_MAX_DEFROST_ON);
        mProps.add(VehiclePropertyIds.HVAC_RECIRC_ON);
        mProps.add(VehiclePropertyIds.HVAC_DUAL_ON);
        mProps.add(VehiclePropertyIds.HVAC_AUTO_ON);
        mProps.add(VehiclePropertyIds.HVAC_SEAT_TEMPERATURE);
        mProps.add(VehiclePropertyIds.HVAC_SIDE_MIRROR_HEAT);
        mProps.add(VehiclePropertyIds.HVAC_STEERING_WHEEL_HEAT);
        mProps.add(VehiclePropertyIds.HVAC_TEMPERATURE_DISPLAY_UNITS);
        mProps.add(VehiclePropertyIds.HVAC_ACTUAL_FAN_SPEED_RPM);
        mProps.add(VehiclePropertyIds.HVAC_POWER_ON);
        mProps.add(VehiclePropertyIds.HVAC_FAN_DIRECTION_AVAILABLE);
        mProps.add(VehiclePropertyIds.HVAC_AUTO_RECIRC_ON);
        mProps.add(VehiclePropertyIds.HVAC_SEAT_VENTILATION);

        // Info properties
        mProps.add(VehiclePropertyIds.INFO_VIN);
        mProps.add(VehiclePropertyIds.INFO_MAKE);
        mProps.add(VehiclePropertyIds.INFO_MODEL);
        mProps.add(VehiclePropertyIds.INFO_MODEL_YEAR);
        mProps.add(VehiclePropertyIds.INFO_FUEL_CAPACITY);
        mProps.add(VehiclePropertyIds.INFO_FUEL_TYPE);
        mProps.add(VehiclePropertyIds.INFO_EV_BATTERY_CAPACITY);
        mProps.add(VehiclePropertyIds.INFO_EV_CONNECTOR_TYPE);
        mProps.add(VehiclePropertyIds.INFO_FUEL_DOOR_LOCATION);
        mProps.add(VehiclePropertyIds.INFO_MULTI_EV_PORT_LOCATIONS);
        mProps.add(VehiclePropertyIds.INFO_EV_PORT_LOCATION);
        mProps.add(VehiclePropertyIds.INFO_DRIVER_SEAT);
        mProps.add(VehiclePropertyIds.INFO_EXTERIOR_DIMENSIONS);

        // Sensor properties
        mProps.add(VehiclePropertyIds.PERF_ODOMETER);
        mProps.add(VehiclePropertyIds.PERF_VEHICLE_SPEED);
        mProps.add(VehiclePropertyIds.PERF_VEHICLE_SPEED_DISPLAY);
        mProps.add(VehiclePropertyIds.ENGINE_COOLANT_TEMP);
        mProps.add(VehiclePropertyIds.ENGINE_OIL_LEVEL);
        mProps.add(VehiclePropertyIds.ENGINE_OIL_TEMP);
        mProps.add(VehiclePropertyIds.ENGINE_RPM);
        mProps.add(VehiclePropertyIds.WHEEL_TICK);
        mProps.add(VehiclePropertyIds.FUEL_LEVEL);
        mProps.add(VehiclePropertyIds.FUEL_DOOR_OPEN);
        mProps.add(VehiclePropertyIds.EV_BATTERY_LEVEL);
        mProps.add(VehiclePropertyIds.EV_CHARGE_PORT_OPEN);
        mProps.add(VehiclePropertyIds.EV_CHARGE_PORT_CONNECTED);
        mProps.add(VehiclePropertyIds.EV_BATTERY_INSTANTANEOUS_CHARGE_RATE);
        mProps.add(VehiclePropertyIds.RANGE_REMAINING);
        mProps.add(VehiclePropertyIds.TIRE_PRESSURE);
        mProps.add(VehiclePropertyIds.PERF_STEERING_ANGLE);
        mProps.add(VehiclePropertyIds.PERF_REAR_STEERING_ANGLE);
        mProps.add(VehiclePropertyIds.GEAR_SELECTION);
        mProps.add(VehiclePropertyIds.CURRENT_GEAR);
        mProps.add(VehiclePropertyIds.PARKING_BRAKE_ON);
        mProps.add(VehiclePropertyIds.PARKING_BRAKE_AUTO_APPLY);
        mProps.add(VehiclePropertyIds.FUEL_LEVEL_LOW);
        mProps.add(VehiclePropertyIds.NIGHT_MODE);
        mProps.add(VehiclePropertyIds.TURN_SIGNAL_STATE);
        mProps.add(VehiclePropertyIds.IGNITION_STATE);
        mProps.add(VehiclePropertyIds.ABS_ACTIVE);
        mProps.add(VehiclePropertyIds.TRACTION_CONTROL_ACTIVE);
        mProps.add(VehiclePropertyIds.ENV_OUTSIDE_TEMPERATURE);
        mProps.add(VehiclePropertyIds.HEADLIGHTS_STATE);
        mProps.add(VehiclePropertyIds.HIGH_BEAM_LIGHTS_STATE);
        mProps.add(VehiclePropertyIds.FOG_LIGHTS_STATE);
        mProps.add(VehiclePropertyIds.HAZARD_LIGHTS_STATE);
        mProps.add(VehiclePropertyIds.HEADLIGHTS_SWITCH);
        mProps.add(VehiclePropertyIds.HIGH_BEAM_LIGHTS_SWITCH);
        mProps.add(VehiclePropertyIds.FOG_LIGHTS_SWITCH);
        mProps.add(VehiclePropertyIds.HAZARD_LIGHTS_SWITCH);
        mProps.add(VehiclePropertyIds.READING_LIGHTS_STATE);
        mProps.add(VehiclePropertyIds.CABIN_LIGHTS_STATE);
        mProps.add(VehiclePropertyIds.READING_LIGHTS_SWITCH);
        mProps.add(VehiclePropertyIds.CABIN_LIGHTS_SWITCH);
        // Display_Units
        mProps.add(VehiclePropertyIds.DISTANCE_DISPLAY_UNITS);
        mProps.add(VehiclePropertyIds.FUEL_VOLUME_DISPLAY_UNITS);
        mProps.add(VehiclePropertyIds.TIRE_PRESSURE_DISPLAY_UNITS);
        mProps.add(VehiclePropertyIds.EV_BATTERY_DISPLAY_UNITS);
        mProps.add(VehiclePropertyIds.FUEL_CONSUMPTION_UNITS_DISTANCE_OVER_VOLUME);
        mProps.add(VehiclePropertyIds.VEHICLE_SPEED_DISPLAY_UNITS);
    }

    @After
    public void tearDown() {
        if (mCar != null) {
            mCar.disconnect();
        }
    }

    @Test
    public void testCarPropertyManagerGetter() {
        for (int propertyId : mProps) {
            try {
                switch (propertyId & VehiclePropertyType.MASK) {
                    case VehiclePropertyType.BOOLEAN:
                        // The areaId may not match with it in propertyConfig. CarService
                        // check the permission before checking valid areaId.
                        mPropertyManager.getBooleanProperty(propertyId, DUMMY_AREA_ID);
                        break;
                    case VehiclePropertyType.FLOAT:
                        mPropertyManager.getFloatProperty(propertyId, DUMMY_AREA_ID);
                        break;
                    case VehiclePropertyType.INT32_VEC:
                        mPropertyManager.getIntArrayProperty(propertyId, DUMMY_AREA_ID);
                        break;
                    case VehiclePropertyType.INT32:
                        mPropertyManager.getIntProperty(propertyId, DUMMY_AREA_ID);
                        break;
                    default:
                        mPropertyManager.getProperty(propertyId, DUMMY_AREA_ID);
                }
            } catch (Exception e) {
                assertWithMessage("Get property: 0x" + Integer.toHexString(propertyId)
                        + " cause an unexpected exception: " + e)
                        .that(e).isInstanceOf(SecurityException.class);
                continue;
            }
            assertPropertyNotImplementedInVhal(propertyId);
        }
    }

    private void assertPropertyNotImplementedInVhal(int propertyId) {
        assertWithMessage("Get property : 0x " + Integer.toHexString(propertyId)
                + " without permission.")
                .that(mPropertyManager.getProperty(propertyId, DUMMY_AREA_ID)).isNull();
        Log.w(TAG, "Property id: 0x" + Integer.toHexString(propertyId)
                + " does not exist in the VHAL implementation.");
    }

    @Test
    public void testCarPropertyManagerSetter() {
        for (int propertyId : mProps) {
            try {
                // Dummy value may not in the valid range. CarService checks permission
                // and sends request to VHAL. VHAL checks specific property range.
                switch (propertyId & VehiclePropertyType.MASK) {
                    case VehiclePropertyType.BOOLEAN:
                        mPropertyManager.setBooleanProperty(propertyId, DUMMY_AREA_ID,
                                DUMMY_PROPERTY_VALUE_BOOLEAN);
                        break;
                    case VehiclePropertyType.FLOAT:
                        mPropertyManager.setFloatProperty(propertyId, DUMMY_AREA_ID,
                                DUMMY_PROPERTY_VALUE_FLOAT);
                        break;
                    case VehiclePropertyType.FLOAT_VEC:
                        mPropertyManager.setProperty(Float[].class, propertyId, DUMMY_AREA_ID,
                                DUMMY_PROPERTY_VALUE_FLOAT_ARRAY);
                        break;
                    case VehiclePropertyType.INT32:
                        mPropertyManager.setIntProperty(propertyId, DUMMY_AREA_ID,
                                DUMMY_PROPERTY_VALUE_INTEGER);
                        break;
                    case VehiclePropertyType.INT32_VEC:
                        mPropertyManager.setProperty(Integer[].class, propertyId, DUMMY_AREA_ID,
                                DUMMY_PROPERTY_VALUE_INTEGER_ARRAY);
                        break;
                    case VehiclePropertyType.INT64:
                        mPropertyManager.setProperty(Long.class, propertyId, DUMMY_AREA_ID,
                                DUMMY_PROPERTY_VALUE_LONG);
                        break;
                    case VehiclePropertyType.INT64_VEC:
                        mPropertyManager.setProperty(Long[].class, propertyId, DUMMY_AREA_ID,
                                DUMMY_PROPERTY_VALUE_LONG_ARRAY);
                        break;
                    case VehiclePropertyType.BYTES:
                        mPropertyManager.setProperty(byte[].class, propertyId, DUMMY_AREA_ID,
                                DUMMY_PROPERTY_VALUE_BYTE_ARRAY);
                        break;
                    case VehiclePropertyType.MIXED:
                        mPropertyManager.setProperty(Object[].class, propertyId, DUMMY_AREA_ID,
                                DUMMY_PROPERTY_VALUE_OBJECT_ARRAY);
                        break;
                    case VehiclePropertyType.STRING:
                        mPropertyManager.setProperty(String.class, propertyId, DUMMY_AREA_ID,
                                DUMMY_PROPERTY_VALUE_STRING);
                        break;
                    default:
                        throw new IllegalArgumentException("Invalid value type for property: 0x"
                                + Integer.toHexString(propertyId));
                }
            } catch (IllegalArgumentException e) {
                assertWithMessage("Set property: 0x" + Integer.toHexString(propertyId)
                        + " cause an unexpected exception: " + e)
                        .that(e.getMessage()).contains("does not exist in the vehicle");
            } catch (Exception e) {
                assertWithMessage("Get property: 0x" + Integer.toHexString(propertyId)
                        + " cause an unexpected exception: " + e)
                        .that(e).isInstanceOf(SecurityException.class);
            }
        }
    }
}
