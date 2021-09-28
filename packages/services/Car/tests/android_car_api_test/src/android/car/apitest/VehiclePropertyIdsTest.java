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
package android.car.apitest;

import android.car.VehiclePropertyIds;
import android.hardware.automotive.vehicle.V2_0.VehicleProperty;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class VehiclePropertyIdsTest extends AndroidTestCase {
    private static final List<String> MISSING_VEHICLE_PROPERTY_IDS =
            new ArrayList<>(
                Arrays.asList(
                    "DISABLED_OPTIONAL_FEATURES",
                    "HW_ROTARY_INPUT",
                    "SUPPORT_CUSTOMIZE_VENDOR_PERMISSION"));
    private static final List<Integer> MISSING_VEHICLE_PROPERTY_ID_VALUES =
            new ArrayList<>(
                Arrays.asList(
                    /*DISABLED_OPTIONAL_FEATURES=*/286265094,
                    /*HW_ROTARY_INPUT=*/289475104,
                    /*SUPPORT_CUSTOMIZE_VENDOR_PERMISSION=*/287313669));


    @Test
    public void testMatchingVehiclePropertyNamesInVehicleHal() {
        List<String> vehiclePropertyIdNames = getListOfConstantNames(VehiclePropertyIds.class);
        List<String> vehiclePropertyNames = getListOfConstantNames(VehicleProperty.class);
        assertEquals(vehiclePropertyNames.size(),
                vehiclePropertyIdNames.size() + MISSING_VEHICLE_PROPERTY_IDS.size());
        for (String vehiclePropertyName: vehiclePropertyNames) {
            if (MISSING_VEHICLE_PROPERTY_IDS.contains(vehiclePropertyName)) {
                continue;
            }
            assertTrue(vehiclePropertyIdNames.contains(vehiclePropertyName));
        }
    }

    @Test
    public void testMatchingVehiclePropertyValuesInVehicleHal() {
        List<Integer> vehiclePropertyIds = getListOfConstantValues(VehiclePropertyIds.class);
        List<Integer> vehicleProperties = getListOfConstantValues(VehicleProperty.class);
        assertEquals(vehicleProperties.size(),
                vehiclePropertyIds.size() + MISSING_VEHICLE_PROPERTY_ID_VALUES.size());
        for (int vehicleProperty: vehicleProperties) {
            if (MISSING_VEHICLE_PROPERTY_ID_VALUES.contains(vehicleProperty)) {
                continue;
            }
            // TODO(b/151168399): VEHICLE_SPEED_DISPLAY_UNITS mismatch between java and hal.
            if (vehicleProperty == VehicleProperty.VEHICLE_SPEED_DISPLAY_UNITS) {
                continue;
            }
            assertTrue(vehiclePropertyIds.contains(vehicleProperty));
        }
    }

    @Test
    public void testToString() {
        assertEquals("INVALID", VehiclePropertyIds.toString(VehiclePropertyIds.INVALID));
        assertEquals("INFO_VIN", VehiclePropertyIds.toString(VehiclePropertyIds.INFO_VIN));
        assertEquals("INFO_MAKE", VehiclePropertyIds.toString(VehiclePropertyIds.INFO_MAKE));
        assertEquals("INFO_MODEL", VehiclePropertyIds.toString(VehiclePropertyIds.INFO_MODEL));
        assertEquals("INFO_MODEL_YEAR",
                VehiclePropertyIds.toString(VehiclePropertyIds.INFO_MODEL_YEAR));
        assertEquals("INFO_FUEL_CAPACITY",
                VehiclePropertyIds.toString(VehiclePropertyIds.INFO_FUEL_CAPACITY));
        assertEquals("INFO_FUEL_TYPE",
                VehiclePropertyIds.toString(VehiclePropertyIds.INFO_FUEL_TYPE));
        assertEquals("INFO_EV_BATTERY_CAPACITY",
                VehiclePropertyIds.toString(VehiclePropertyIds.INFO_EV_BATTERY_CAPACITY));
        assertEquals("INFO_MULTI_EV_PORT_LOCATIONS",
                VehiclePropertyIds.toString(VehiclePropertyIds.INFO_MULTI_EV_PORT_LOCATIONS));
        assertEquals("INFO_EV_CONNECTOR_TYPE",
                VehiclePropertyIds.toString(VehiclePropertyIds.INFO_EV_CONNECTOR_TYPE));
        assertEquals("INFO_FUEL_DOOR_LOCATION",
                VehiclePropertyIds.toString(VehiclePropertyIds.INFO_FUEL_DOOR_LOCATION));
        assertEquals("INFO_EV_PORT_LOCATION",
                VehiclePropertyIds.toString(VehiclePropertyIds.INFO_EV_PORT_LOCATION));
        assertEquals("INFO_DRIVER_SEAT",
                VehiclePropertyIds.toString(VehiclePropertyIds.INFO_DRIVER_SEAT));
        assertEquals("INFO_EXTERIOR_DIMENSIONS",
                VehiclePropertyIds.toString(VehiclePropertyIds.INFO_EXTERIOR_DIMENSIONS));
        assertEquals("PERF_ODOMETER",
                VehiclePropertyIds.toString(VehiclePropertyIds.PERF_ODOMETER));
        assertEquals("PERF_VEHICLE_SPEED",
                VehiclePropertyIds.toString(VehiclePropertyIds.PERF_VEHICLE_SPEED));
        assertEquals("PERF_VEHICLE_SPEED_DISPLAY",
                VehiclePropertyIds.toString(VehiclePropertyIds.PERF_VEHICLE_SPEED_DISPLAY));
        assertEquals("PERF_STEERING_ANGLE",
                VehiclePropertyIds.toString(VehiclePropertyIds.PERF_STEERING_ANGLE));
        assertEquals("PERF_REAR_STEERING_ANGLE",
                VehiclePropertyIds.toString(VehiclePropertyIds.PERF_REAR_STEERING_ANGLE));
        assertEquals("ENGINE_COOLANT_TEMP",
                VehiclePropertyIds.toString(VehiclePropertyIds.ENGINE_COOLANT_TEMP));
        assertEquals("ENGINE_OIL_LEVEL",
                VehiclePropertyIds.toString(VehiclePropertyIds.ENGINE_OIL_LEVEL));
        assertEquals("ENGINE_OIL_TEMP",
                VehiclePropertyIds.toString(VehiclePropertyIds.ENGINE_OIL_TEMP));
        assertEquals("ENGINE_RPM", VehiclePropertyIds.toString(VehiclePropertyIds.ENGINE_RPM));
        assertEquals("WHEEL_TICK", VehiclePropertyIds.toString(VehiclePropertyIds.WHEEL_TICK));
        assertEquals("FUEL_LEVEL", VehiclePropertyIds.toString(VehiclePropertyIds.FUEL_LEVEL));
        assertEquals("FUEL_DOOR_OPEN",
                VehiclePropertyIds.toString(VehiclePropertyIds.FUEL_DOOR_OPEN));
        assertEquals("EV_BATTERY_LEVEL",
                VehiclePropertyIds.toString(VehiclePropertyIds.EV_BATTERY_LEVEL));
        assertEquals("EV_CHARGE_PORT_OPEN",
                VehiclePropertyIds.toString(VehiclePropertyIds.EV_CHARGE_PORT_OPEN));
        assertEquals("EV_CHARGE_PORT_CONNECTED",
                VehiclePropertyIds.toString(VehiclePropertyIds.EV_CHARGE_PORT_CONNECTED));
        assertEquals("EV_BATTERY_INSTANTANEOUS_CHARGE_RATE",
                VehiclePropertyIds.toString(
                        VehiclePropertyIds.EV_BATTERY_INSTANTANEOUS_CHARGE_RATE));
        assertEquals("RANGE_REMAINING",
                VehiclePropertyIds.toString(VehiclePropertyIds.RANGE_REMAINING));
        assertEquals("TIRE_PRESSURE",
                VehiclePropertyIds.toString(VehiclePropertyIds.TIRE_PRESSURE));
        assertEquals("GEAR_SELECTION",
                VehiclePropertyIds.toString(VehiclePropertyIds.GEAR_SELECTION));
        assertEquals("CURRENT_GEAR", VehiclePropertyIds.toString(VehiclePropertyIds.CURRENT_GEAR));
        assertEquals("PARKING_BRAKE_ON",
                VehiclePropertyIds.toString(VehiclePropertyIds.PARKING_BRAKE_ON));
        assertEquals("PARKING_BRAKE_AUTO_APPLY",
                VehiclePropertyIds.toString(VehiclePropertyIds.PARKING_BRAKE_AUTO_APPLY));
        assertEquals("FUEL_LEVEL_LOW",
                VehiclePropertyIds.toString(VehiclePropertyIds.FUEL_LEVEL_LOW));
        assertEquals("NIGHT_MODE", VehiclePropertyIds.toString(VehiclePropertyIds.NIGHT_MODE));
        assertEquals("TURN_SIGNAL_STATE",
                VehiclePropertyIds.toString(VehiclePropertyIds.TURN_SIGNAL_STATE));
        assertEquals("IGNITION_STATE",
                VehiclePropertyIds.toString(VehiclePropertyIds.IGNITION_STATE));
        assertEquals("ABS_ACTIVE", VehiclePropertyIds.toString(VehiclePropertyIds.ABS_ACTIVE));
        assertEquals("TRACTION_CONTROL_ACTIVE",
                VehiclePropertyIds.toString(VehiclePropertyIds.TRACTION_CONTROL_ACTIVE));
        assertEquals("HVAC_FAN_SPEED",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_FAN_SPEED));
        assertEquals("HVAC_FAN_DIRECTION",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_FAN_DIRECTION));
        assertEquals("HVAC_TEMPERATURE_CURRENT",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_TEMPERATURE_CURRENT));
        assertEquals("HVAC_TEMPERATURE_SET",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_TEMPERATURE_SET));
        assertEquals("HVAC_DEFROSTER",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_DEFROSTER));
        assertEquals("HVAC_AC_ON", VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_AC_ON));
        assertEquals("HVAC_MAX_AC_ON",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_MAX_AC_ON));
        assertEquals("HVAC_MAX_DEFROST_ON",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_MAX_DEFROST_ON));
        assertEquals("HVAC_RECIRC_ON",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_RECIRC_ON));
        assertEquals("HVAC_DUAL_ON", VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_DUAL_ON));
        assertEquals("HVAC_AUTO_ON", VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_AUTO_ON));
        assertEquals("HVAC_SEAT_TEMPERATURE",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_SEAT_TEMPERATURE));
        assertEquals("HVAC_SIDE_MIRROR_HEAT",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_SIDE_MIRROR_HEAT));
        assertEquals("HVAC_STEERING_WHEEL_HEAT",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_STEERING_WHEEL_HEAT));
        assertEquals("HVAC_TEMPERATURE_DISPLAY_UNITS",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_TEMPERATURE_DISPLAY_UNITS));
        assertEquals("HVAC_ACTUAL_FAN_SPEED_RPM",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_ACTUAL_FAN_SPEED_RPM));
        assertEquals("HVAC_POWER_ON",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_POWER_ON));
        assertEquals("HVAC_FAN_DIRECTION_AVAILABLE",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_FAN_DIRECTION_AVAILABLE));
        assertEquals("HVAC_AUTO_RECIRC_ON",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_AUTO_RECIRC_ON));
        assertEquals("HVAC_SEAT_VENTILATION",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_SEAT_VENTILATION));
        assertEquals("HVAC_ELECTRIC_DEFROSTER_ON",
                VehiclePropertyIds.toString(VehiclePropertyIds.HVAC_ELECTRIC_DEFROSTER_ON));
        assertEquals("DISTANCE_DISPLAY_UNITS",
                VehiclePropertyIds.toString(VehiclePropertyIds.DISTANCE_DISPLAY_UNITS));
        assertEquals("FUEL_VOLUME_DISPLAY_UNITS",
                VehiclePropertyIds.toString(VehiclePropertyIds.FUEL_VOLUME_DISPLAY_UNITS));
        assertEquals("TIRE_PRESSURE_DISPLAY_UNITS",
                VehiclePropertyIds.toString(VehiclePropertyIds.TIRE_PRESSURE_DISPLAY_UNITS));
        assertEquals("EV_BATTERY_DISPLAY_UNITS",
                VehiclePropertyIds.toString(VehiclePropertyIds.EV_BATTERY_DISPLAY_UNITS));
        assertEquals("FUEL_CONSUMPTION_UNITS_DISTANCE_OVER_VOLUME",
                VehiclePropertyIds.toString(
                        VehiclePropertyIds.FUEL_CONSUMPTION_UNITS_DISTANCE_OVER_VOLUME));
        assertEquals("ENV_OUTSIDE_TEMPERATURE",
                VehiclePropertyIds.toString(VehiclePropertyIds.ENV_OUTSIDE_TEMPERATURE));
        assertEquals("AP_POWER_STATE_REQ",
                VehiclePropertyIds.toString(VehiclePropertyIds.AP_POWER_STATE_REQ));
        assertEquals("AP_POWER_STATE_REPORT",
                VehiclePropertyIds.toString(VehiclePropertyIds.AP_POWER_STATE_REPORT));
        assertEquals("AP_POWER_BOOTUP_REASON",
                VehiclePropertyIds.toString(VehiclePropertyIds.AP_POWER_BOOTUP_REASON));
        assertEquals("DISPLAY_BRIGHTNESS",
                VehiclePropertyIds.toString(VehiclePropertyIds.DISPLAY_BRIGHTNESS));
        assertEquals("HW_KEY_INPUT", VehiclePropertyIds.toString(VehiclePropertyIds.HW_KEY_INPUT));
        assertEquals("DOOR_POS", VehiclePropertyIds.toString(VehiclePropertyIds.DOOR_POS));
        assertEquals("DOOR_MOVE", VehiclePropertyIds.toString(VehiclePropertyIds.DOOR_MOVE));
        assertEquals("DOOR_LOCK", VehiclePropertyIds.toString(VehiclePropertyIds.DOOR_LOCK));
        assertEquals("MIRROR_Z_POS", VehiclePropertyIds.toString(VehiclePropertyIds.MIRROR_Z_POS));
        assertEquals("MIRROR_Z_MOVE",
                VehiclePropertyIds.toString(VehiclePropertyIds.MIRROR_Z_MOVE));
        assertEquals("MIRROR_Y_POS", VehiclePropertyIds.toString(VehiclePropertyIds.MIRROR_Y_POS));
        assertEquals("MIRROR_Y_MOVE",
                VehiclePropertyIds.toString(VehiclePropertyIds.MIRROR_Y_MOVE));
        assertEquals("MIRROR_LOCK", VehiclePropertyIds.toString(VehiclePropertyIds.MIRROR_LOCK));
        assertEquals("MIRROR_FOLD", VehiclePropertyIds.toString(VehiclePropertyIds.MIRROR_FOLD));
        assertEquals("SEAT_MEMORY_SELECT",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_MEMORY_SELECT));
        assertEquals("SEAT_MEMORY_SET",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_MEMORY_SET));
        assertEquals("SEAT_BELT_BUCKLED",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_BELT_BUCKLED));
        assertEquals("SEAT_BELT_HEIGHT_POS",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_BELT_HEIGHT_POS));
        assertEquals("SEAT_BELT_HEIGHT_MOVE",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_BELT_HEIGHT_MOVE));
        assertEquals("SEAT_FORE_AFT_POS",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_FORE_AFT_POS));
        assertEquals("SEAT_FORE_AFT_MOVE",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_FORE_AFT_MOVE));
        assertEquals("SEAT_BACKREST_ANGLE_1_POS",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_BACKREST_ANGLE_1_POS));
        assertEquals("SEAT_BACKREST_ANGLE_1_MOVE",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_BACKREST_ANGLE_1_MOVE));
        assertEquals("SEAT_BACKREST_ANGLE_2_POS",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_BACKREST_ANGLE_2_POS));
        assertEquals("SEAT_BACKREST_ANGLE_2_MOVE",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_BACKREST_ANGLE_2_MOVE));
        assertEquals("SEAT_HEIGHT_POS",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_HEIGHT_POS));
        assertEquals("SEAT_HEIGHT_MOVE",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_HEIGHT_MOVE));
        assertEquals("SEAT_DEPTH_POS",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_DEPTH_POS));
        assertEquals("SEAT_DEPTH_MOVE",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_DEPTH_MOVE));
        assertEquals("SEAT_TILT_POS",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_TILT_POS));
        assertEquals("SEAT_TILT_MOVE",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_TILT_MOVE));
        assertEquals("SEAT_LUMBAR_FORE_AFT_POS",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_LUMBAR_FORE_AFT_POS));
        assertEquals("SEAT_LUMBAR_FORE_AFT_MOVE",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_LUMBAR_FORE_AFT_MOVE));
        assertEquals("SEAT_LUMBAR_SIDE_SUPPORT_POS",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_LUMBAR_SIDE_SUPPORT_POS));
        assertEquals("SEAT_LUMBAR_SIDE_SUPPORT_MOVE",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_LUMBAR_SIDE_SUPPORT_MOVE));
        assertEquals("SEAT_HEADREST_HEIGHT_POS",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_HEADREST_HEIGHT_POS));
        assertEquals("SEAT_HEADREST_HEIGHT_MOVE",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_HEADREST_HEIGHT_MOVE));
        assertEquals("SEAT_HEADREST_ANGLE_POS",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_HEADREST_ANGLE_POS));
        assertEquals("SEAT_HEADREST_ANGLE_MOVE",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_HEADREST_ANGLE_MOVE));
        assertEquals("SEAT_HEADREST_FORE_AFT_POS",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_HEADREST_FORE_AFT_POS));
        assertEquals("SEAT_HEADREST_FORE_AFT_MOVE",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_HEADREST_FORE_AFT_MOVE));
        assertEquals("SEAT_OCCUPANCY",
                VehiclePropertyIds.toString(VehiclePropertyIds.SEAT_OCCUPANCY));
        assertEquals("WINDOW_POS", VehiclePropertyIds.toString(VehiclePropertyIds.WINDOW_POS));
        assertEquals("WINDOW_MOVE", VehiclePropertyIds.toString(VehiclePropertyIds.WINDOW_MOVE));
        assertEquals("WINDOW_LOCK", VehiclePropertyIds.toString(VehiclePropertyIds.WINDOW_LOCK));
        assertEquals("VEHICLE_MAP_SERVICE",
                VehiclePropertyIds.toString(VehiclePropertyIds.VEHICLE_MAP_SERVICE));
        assertEquals("OBD2_LIVE_FRAME",
                VehiclePropertyIds.toString(VehiclePropertyIds.OBD2_LIVE_FRAME));
        assertEquals("OBD2_FREEZE_FRAME",
                VehiclePropertyIds.toString(VehiclePropertyIds.OBD2_FREEZE_FRAME));
        assertEquals("OBD2_FREEZE_FRAME_INFO",
                VehiclePropertyIds.toString(VehiclePropertyIds.OBD2_FREEZE_FRAME_INFO));
        assertEquals("OBD2_FREEZE_FRAME_CLEAR",
                VehiclePropertyIds.toString(VehiclePropertyIds.OBD2_FREEZE_FRAME_CLEAR));
        assertEquals("HEADLIGHTS_STATE",
                VehiclePropertyIds.toString(VehiclePropertyIds.HEADLIGHTS_STATE));
        assertEquals("HIGH_BEAM_LIGHTS_STATE",
                VehiclePropertyIds.toString(VehiclePropertyIds.HIGH_BEAM_LIGHTS_STATE));
        assertEquals("FOG_LIGHTS_STATE",
                VehiclePropertyIds.toString(VehiclePropertyIds.FOG_LIGHTS_STATE));
        assertEquals("HAZARD_LIGHTS_STATE",
                VehiclePropertyIds.toString(VehiclePropertyIds.HAZARD_LIGHTS_STATE));
        assertEquals("HEADLIGHTS_SWITCH",
                VehiclePropertyIds.toString(VehiclePropertyIds.HEADLIGHTS_SWITCH));
        assertEquals("HIGH_BEAM_LIGHTS_SWITCH",
                VehiclePropertyIds.toString(VehiclePropertyIds.HIGH_BEAM_LIGHTS_SWITCH));
        assertEquals("FOG_LIGHTS_SWITCH",
                VehiclePropertyIds.toString(VehiclePropertyIds.FOG_LIGHTS_SWITCH));
        assertEquals("HAZARD_LIGHTS_SWITCH",
                VehiclePropertyIds.toString(VehiclePropertyIds.HAZARD_LIGHTS_SWITCH));
        assertEquals("CABIN_LIGHTS_STATE",
                VehiclePropertyIds.toString(VehiclePropertyIds.CABIN_LIGHTS_STATE));
        assertEquals("CABIN_LIGHTS_SWITCH",
                VehiclePropertyIds.toString(VehiclePropertyIds.CABIN_LIGHTS_SWITCH));
        assertEquals("READING_LIGHTS_STATE",
                VehiclePropertyIds.toString(VehiclePropertyIds.READING_LIGHTS_STATE));
        assertEquals("READING_LIGHTS_SWITCH",
                VehiclePropertyIds.toString(VehiclePropertyIds.READING_LIGHTS_SWITCH));
        assertEquals("VEHICLE_SPEED_DISPLAY_UNITS",
                VehiclePropertyIds.toString(VehiclePropertyIds.VEHICLE_SPEED_DISPLAY_UNITS));
        assertEquals("INITIAL_USER_INFO",
                VehiclePropertyIds.toString(VehiclePropertyIds.INITIAL_USER_INFO));
        assertEquals("SWITCH_USER", VehiclePropertyIds.toString(VehiclePropertyIds.SWITCH_USER));
        assertEquals("USER_IDENTIFICATION_ASSOCIATION",
                VehiclePropertyIds.toString(VehiclePropertyIds.USER_IDENTIFICATION_ASSOCIATION));
        assertEquals("0x3", VehiclePropertyIds.toString(3));
    }

    private static List<Integer> getListOfConstantValues(Class clazz) {
        List<Integer> list = new ArrayList<Integer>();
        for (Field field : clazz.getDeclaredFields()) {
            int modifiers = field.getModifiers();
            if (Modifier.isStatic(modifiers) && Modifier.isFinal(modifiers)) {
                try {
                    list.add(field.getInt(null));
                } catch (IllegalAccessException e) {
                }
            }
        }
        return list;
    }

    private static List<String> getListOfConstantNames(Class clazz) {
        List<String> list = new ArrayList<String>();
        for (Field field : clazz.getDeclaredFields()) {
            int modifiers = field.getModifiers();
            if (Modifier.isStatic(modifiers) && Modifier.isFinal(modifiers)) {
                list.add(field.getName());
            }
        }
        return list;
    }
}
