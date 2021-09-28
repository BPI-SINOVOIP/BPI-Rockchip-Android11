/*
 * Copyright (C) 2018 The Android Open Source Project
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

import static java.lang.Integer.toHexString;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.car.Car;
import android.car.VehicleHvacFanDirection;
import android.car.hardware.property.VehicleVendorPermission;
import android.hardware.automotive.vehicle.V2_0.EvConnectorType;
import android.hardware.automotive.vehicle.V2_0.FuelType;
import android.hardware.automotive.vehicle.V2_0.PortLocationType;
import android.hardware.automotive.vehicle.V2_0.VehicleAreaSeat;
import android.hardware.automotive.vehicle.V2_0.VehicleGear;
import android.hardware.automotive.vehicle.V2_0.VehicleIgnitionState;
import android.hardware.automotive.vehicle.V2_0.VehicleLightState;
import android.hardware.automotive.vehicle.V2_0.VehicleLightSwitch;
import android.hardware.automotive.vehicle.V2_0.VehicleOilLevel;
import android.hardware.automotive.vehicle.V2_0.VehiclePropValue;
import android.hardware.automotive.vehicle.V2_0.VehicleProperty;
import android.hardware.automotive.vehicle.V2_0.VehiclePropertyGroup;
import android.hardware.automotive.vehicle.V2_0.VehiclePropertyType;
import android.hardware.automotive.vehicle.V2_0.VehicleSeatOccupancyState;
import android.hardware.automotive.vehicle.V2_0.VehicleTurnSignal;
import android.hardware.automotive.vehicle.V2_0.VehicleUnit;
import android.util.Log;
import android.util.Pair;
import android.util.SparseArray;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Helper class to define which property IDs are used by PropertyHalService.  This class binds the
 * read and write permissions to the property ID.
 */
public class PropertyHalServiceIds {

    /**
     * Index key is propertyId, and the value is readPermission, writePermission.
     * If the property can not be written (or read), set value as NULL.
     * Throw an IllegalArgumentException when try to write READ_ONLY properties or read WRITE_ONLY
     * properties.
     */
    private final SparseArray<Pair<String, String>> mProps;
    private final HashSet<Integer> mPropForUnits;
    // Key: propId, Value: possible value for the property
    private final HashMap<Integer, Set<Integer>> mPropToValidValue;
    private final HashMap<Integer, Integer> mPropToValidBitFlag;
    private static final String TAG = "PropertyHalServiceIds";
    // Enums are used as return value in Vehicle HAL.
    private static final Set<Integer> FUEL_TYPE =
            new HashSet<>(getIntegersFromDataEnums(FuelType.class));
    private static final Set<Integer> EV_CONNECTOR_TYPE =
            new HashSet<>(getIntegersFromDataEnums(EvConnectorType.class));
    private static final Set<Integer> PORT_LOCATION =
            new HashSet<>(getIntegersFromDataEnums(PortLocationType.class));
    private static final Set<Integer> VEHICLE_SEAT =
            new HashSet<>(getIntegersFromDataEnums(VehicleAreaSeat.class));
    private static final Set<Integer> OIL_LEVEL =
            new HashSet<>(getIntegersFromDataEnums(VehicleOilLevel.class));
    private static final Set<Integer> VEHICLE_GEAR =
            new HashSet<>(getIntegersFromDataEnums(VehicleGear.class));
    private static final Set<Integer> TURN_SIGNAL =
            new HashSet<>(getIntegersFromDataEnums(VehicleTurnSignal.class));
    private static final Set<Integer> IGNITION_STATE =
            new HashSet<>(getIntegersFromDataEnums(VehicleIgnitionState.class));
    private static final Set<Integer> VEHICLE_UNITS =
            new HashSet<>(getIntegersFromDataEnums(VehicleUnit.class));
    private static final Set<Integer> SEAT_OCCUPANCY_STATE =
            new HashSet<>(getIntegersFromDataEnums(VehicleSeatOccupancyState.class));
    private static final Set<Integer> VEHICLE_LIGHT_STATE =
            new HashSet<>(getIntegersFromDataEnums(VehicleLightState.class));
    private static final Set<Integer> VEHICLE_LIGHT_SWITCH =
            new HashSet<>(getIntegersFromDataEnums(VehicleLightSwitch.class));
    private static final int HVAC_FAN_DIRECTION_COMBINATIONS =
            generateAllCombination(VehicleHvacFanDirection.class);

    // default vendor permission
    private static final int PERMISSION_CAR_VENDOR_DEFAULT = 0x00000000;

    // permissions for the property related with window
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_WINDOW = 0X00000001;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_WINDOW = 0x00000002;
    // permissions for the property related with door
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_DOOR = 0x00000003;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_DOOR = 0x00000004;
    // permissions for the property related with seat
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_SEAT = 0x00000005;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_SEAT = 0x00000006;
    // permissions for the property related with mirror
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_MIRROR = 0x00000007;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_MIRROR = 0x00000008;

    // permissions for the property related with car's information
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_INFO = 0x00000009;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_INFO = 0x0000000A;
    // permissions for the property related with car's engine
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_ENGINE = 0x0000000B;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_ENGINE = 0x0000000C;
    // permissions for the property related with car's HVAC
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_HVAC = 0x0000000D;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_HVAC = 0x0000000E;
    // permissions for the property related with car's light
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_LIGHT = 0x0000000F;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_LIGHT = 0x00000010;

    // permissions reserved for other vendor permission
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_1 = 0x00010000;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_1 = 0x00011000;
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_2 = 0x00020000;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_2 = 0x00021000;
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_3 = 0x00030000;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_3 = 0x00031000;
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_4 = 0x00040000;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_4 = 0x00041000;
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_5 = 0x00050000;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_5 = 0x00051000;
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_6 = 0x00060000;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_6 = 0x00061000;
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_7 = 0x00070000;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_7 = 0x00071000;
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_8 = 0x00080000;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_8 = 0x00081000;
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_9 = 0x00090000;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_9 = 0x00091000;
    private static final int PERMISSION_SET_CAR_VENDOR_CATEGORY_10 = 0x000A0000;
    private static final int PERMISSION_GET_CAR_VENDOR_CATEGORY_10 = 0x000A1000;
    // Not available for android
    private static final int PERMISSION_CAR_VENDOR_NOT_ACCESSIBLE = 0xF0000000;

    public PropertyHalServiceIds() {
        mProps = new SparseArray<>();
        mPropForUnits = new HashSet<>();
        mPropToValidValue = new HashMap<>();
        mPropToValidBitFlag = new HashMap<>();
        // Add propertyId and read/write permissions
        // Cabin Properties
        mProps.put(VehicleProperty.DOOR_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_DOORS,
                Car.PERMISSION_CONTROL_CAR_DOORS));
        mProps.put(VehicleProperty.DOOR_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_DOORS,
                Car.PERMISSION_CONTROL_CAR_DOORS));
        mProps.put(VehicleProperty.DOOR_LOCK, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_DOORS,
                Car.PERMISSION_CONTROL_CAR_DOORS));
        mProps.put(VehicleProperty.MIRROR_Z_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_MIRRORS,
                Car.PERMISSION_CONTROL_CAR_MIRRORS));
        mProps.put(VehicleProperty.MIRROR_Z_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_MIRRORS,
                Car.PERMISSION_CONTROL_CAR_MIRRORS));
        mProps.put(VehicleProperty.MIRROR_Y_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_MIRRORS,
                Car.PERMISSION_CONTROL_CAR_MIRRORS));
        mProps.put(VehicleProperty.MIRROR_Y_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_MIRRORS,
                Car.PERMISSION_CONTROL_CAR_MIRRORS));
        mProps.put(VehicleProperty.MIRROR_LOCK, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_MIRRORS,
                Car.PERMISSION_CONTROL_CAR_MIRRORS));
        mProps.put(VehicleProperty.MIRROR_FOLD, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_MIRRORS,
                Car.PERMISSION_CONTROL_CAR_MIRRORS));
        mProps.put(VehicleProperty.SEAT_MEMORY_SELECT, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_MEMORY_SET, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_BELT_BUCKLED, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_BELT_HEIGHT_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_BELT_HEIGHT_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_FORE_AFT_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_FORE_AFT_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_BACKREST_ANGLE_1_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_BACKREST_ANGLE_1_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_BACKREST_ANGLE_2_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_BACKREST_ANGLE_2_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_HEIGHT_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_HEIGHT_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_DEPTH_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_DEPTH_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_TILT_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_TILT_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_LUMBAR_FORE_AFT_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_LUMBAR_FORE_AFT_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_LUMBAR_SIDE_SUPPORT_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_LUMBAR_SIDE_SUPPORT_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_HEADREST_HEIGHT_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_HEADREST_HEIGHT_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_HEADREST_ANGLE_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_HEADREST_ANGLE_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_HEADREST_FORE_AFT_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_HEADREST_FORE_AFT_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                Car.PERMISSION_CONTROL_CAR_SEATS));
        mProps.put(VehicleProperty.SEAT_OCCUPANCY, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_SEATS,
                null));
        mProps.put(VehicleProperty.WINDOW_POS, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_WINDOWS,
                Car.PERMISSION_CONTROL_CAR_WINDOWS));
        mProps.put(VehicleProperty.WINDOW_MOVE, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_WINDOWS,
                Car.PERMISSION_CONTROL_CAR_WINDOWS));
        mProps.put(VehicleProperty.WINDOW_LOCK, new Pair<>(
                Car.PERMISSION_CONTROL_CAR_WINDOWS,
                Car.PERMISSION_CONTROL_CAR_WINDOWS));

        // HVAC properties
        mProps.put(VehicleProperty.HVAC_FAN_SPEED, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_FAN_DIRECTION, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_TEMPERATURE_CURRENT, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_TEMPERATURE_SET, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_DEFROSTER, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_ELECTRIC_DEFROSTER_ON, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_AC_ON, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_MAX_AC_ON, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_MAX_DEFROST_ON, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_RECIRC_ON, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_DUAL_ON, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_AUTO_ON, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_SEAT_TEMPERATURE, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_SIDE_MIRROR_HEAT, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_STEERING_WHEEL_HEAT, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_TEMPERATURE_DISPLAY_UNITS, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_ACTUAL_FAN_SPEED_RPM, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_POWER_ON, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_FAN_DIRECTION_AVAILABLE, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_AUTO_RECIRC_ON, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));
        mProps.put(VehicleProperty.HVAC_SEAT_VENTILATION, new Pair<>(
                    Car.PERMISSION_CONTROL_CAR_CLIMATE,
                    Car.PERMISSION_CONTROL_CAR_CLIMATE));

        // Info properties
        mProps.put(VehicleProperty.INFO_VIN, new Pair<>(
                    Car.PERMISSION_IDENTIFICATION,
                    null));
        mProps.put(VehicleProperty.INFO_MAKE, new Pair<>(
                    Car.PERMISSION_CAR_INFO,
                    null));
        mProps.put(VehicleProperty.INFO_MODEL, new Pair<>(
                    Car.PERMISSION_CAR_INFO,
                    null));
        mProps.put(VehicleProperty.INFO_MODEL_YEAR, new Pair<>(
                    Car.PERMISSION_CAR_INFO,
                    null));
        mProps.put(VehicleProperty.INFO_FUEL_CAPACITY, new Pair<>(
                    Car.PERMISSION_CAR_INFO,
                    null));
        mProps.put(VehicleProperty.INFO_FUEL_TYPE, new Pair<>(
                    Car.PERMISSION_CAR_INFO,
                    null));
        mProps.put(VehicleProperty.INFO_EV_BATTERY_CAPACITY, new Pair<>(
                    Car.PERMISSION_CAR_INFO,
                    null));
        mProps.put(VehicleProperty.INFO_EV_CONNECTOR_TYPE, new Pair<>(
                    Car.PERMISSION_CAR_INFO,
                    null));
        mProps.put(VehicleProperty.INFO_FUEL_DOOR_LOCATION, new Pair<>(
                    Car.PERMISSION_CAR_INFO,
                    null));
        mProps.put(VehicleProperty.INFO_MULTI_EV_PORT_LOCATIONS, new Pair<>(
                    Car.PERMISSION_CAR_INFO,
                    null));
        mProps.put(VehicleProperty.INFO_EV_PORT_LOCATION, new Pair<>(
                    Car.PERMISSION_CAR_INFO,
                    null));
        mProps.put(VehicleProperty.INFO_DRIVER_SEAT, new Pair<>(
                    Car.PERMISSION_CAR_INFO,
                    null));
        mProps.put(VehicleProperty.INFO_EXTERIOR_DIMENSIONS, new Pair<>(
                    Car.PERMISSION_CAR_INFO,
                    null));

        // Sensor properties
        mProps.put(VehicleProperty.PERF_ODOMETER, new Pair<>(
                Car.PERMISSION_MILEAGE,
                null));
        mProps.put(VehicleProperty.PERF_VEHICLE_SPEED, new Pair<>(
                Car.PERMISSION_SPEED,
                null));
        mProps.put(VehicleProperty.PERF_VEHICLE_SPEED_DISPLAY, new Pair<>(
                Car.PERMISSION_SPEED,
                null));
        mProps.put(VehicleProperty.ENGINE_COOLANT_TEMP, new Pair<>(
                Car.PERMISSION_CAR_ENGINE_DETAILED,
                null));
        mProps.put(VehicleProperty.ENGINE_OIL_LEVEL, new Pair<>(
                Car.PERMISSION_CAR_ENGINE_DETAILED,
                null));
        mProps.put(VehicleProperty.ENGINE_OIL_TEMP, new Pair<>(
                Car.PERMISSION_CAR_ENGINE_DETAILED,
                null));
        mProps.put(VehicleProperty.ENGINE_RPM, new Pair<>(
                Car.PERMISSION_CAR_ENGINE_DETAILED,
                null));
        mProps.put(VehicleProperty.WHEEL_TICK, new Pair<>(
                Car.PERMISSION_SPEED,
                null));
        mProps.put(VehicleProperty.FUEL_LEVEL, new Pair<>(
                Car.PERMISSION_ENERGY,
                null));
        mProps.put(VehicleProperty.FUEL_DOOR_OPEN, new Pair<>(
                Car.PERMISSION_ENERGY_PORTS,
                Car.PERMISSION_CONTROL_ENERGY_PORTS));
        mProps.put(VehicleProperty.EV_BATTERY_LEVEL, new Pair<>(
                Car.PERMISSION_ENERGY,
                null));
        mProps.put(VehicleProperty.EV_CHARGE_PORT_OPEN, new Pair<>(
                Car.PERMISSION_ENERGY_PORTS,
                Car.PERMISSION_CONTROL_ENERGY_PORTS));
        mProps.put(VehicleProperty.EV_CHARGE_PORT_CONNECTED, new Pair<>(
                Car.PERMISSION_ENERGY_PORTS,
                null));
        mProps.put(VehicleProperty.EV_BATTERY_INSTANTANEOUS_CHARGE_RATE, new Pair<>(
                Car.PERMISSION_ENERGY,
                null));
        mProps.put(VehicleProperty.RANGE_REMAINING, new Pair<>(
                Car.PERMISSION_ENERGY,
                Car.PERMISSION_ADJUST_RANGE_REMAINING));
        mProps.put(VehicleProperty.TIRE_PRESSURE, new Pair<>(
                Car.PERMISSION_TIRES,
                null));
        mProps.put(VehicleProperty.PERF_STEERING_ANGLE, new Pair<>(
                Car.PERMISSION_READ_STEERING_STATE,
                null));
        mProps.put(VehicleProperty.PERF_REAR_STEERING_ANGLE, new Pair<>(
                Car.PERMISSION_READ_STEERING_STATE,
                null));
        mProps.put(VehicleProperty.GEAR_SELECTION, new Pair<>(
                Car.PERMISSION_POWERTRAIN,
                null));
        mProps.put(VehicleProperty.CURRENT_GEAR, new Pair<>(
                Car.PERMISSION_POWERTRAIN,
                null));
        mProps.put(VehicleProperty.PARKING_BRAKE_ON, new Pair<>(
                Car.PERMISSION_POWERTRAIN,
                null));
        mProps.put(VehicleProperty.PARKING_BRAKE_AUTO_APPLY, new Pair<>(
                Car.PERMISSION_POWERTRAIN,
                null));
        mProps.put(VehicleProperty.FUEL_LEVEL_LOW, new Pair<>(
                Car.PERMISSION_ENERGY,
                null));
        mProps.put(VehicleProperty.NIGHT_MODE, new Pair<>(
                Car.PERMISSION_EXTERIOR_ENVIRONMENT,
                null));
        mProps.put(VehicleProperty.TURN_SIGNAL_STATE, new Pair<>(
                Car.PERMISSION_EXTERIOR_LIGHTS,
                null));
        mProps.put(VehicleProperty.IGNITION_STATE, new Pair<>(
                Car.PERMISSION_POWERTRAIN,
                null));
        mProps.put(VehicleProperty.ABS_ACTIVE, new Pair<>(
                Car.PERMISSION_CAR_DYNAMICS_STATE,
                null));
        mProps.put(VehicleProperty.TRACTION_CONTROL_ACTIVE, new Pair<>(
                Car.PERMISSION_CAR_DYNAMICS_STATE,
                null));
        mProps.put(VehicleProperty.ENV_OUTSIDE_TEMPERATURE, new Pair<>(
                Car.PERMISSION_EXTERIOR_ENVIRONMENT,
                null));
        mProps.put(VehicleProperty.HEADLIGHTS_STATE, new Pair<>(
                Car.PERMISSION_EXTERIOR_LIGHTS,
                null));
        mProps.put(VehicleProperty.HIGH_BEAM_LIGHTS_STATE, new Pair<>(
                Car.PERMISSION_EXTERIOR_LIGHTS,
                null));
        mProps.put(VehicleProperty.FOG_LIGHTS_STATE, new Pair<>(
                Car.PERMISSION_EXTERIOR_LIGHTS,
                null));
        mProps.put(VehicleProperty.HAZARD_LIGHTS_STATE, new Pair<>(
                Car.PERMISSION_EXTERIOR_LIGHTS,
                null));
        mProps.put(VehicleProperty.HEADLIGHTS_SWITCH, new Pair<>(
                Car.PERMISSION_CONTROL_EXTERIOR_LIGHTS,
                Car.PERMISSION_CONTROL_EXTERIOR_LIGHTS));
        mProps.put(VehicleProperty.HIGH_BEAM_LIGHTS_SWITCH, new Pair<>(
                Car.PERMISSION_CONTROL_EXTERIOR_LIGHTS,
                Car.PERMISSION_CONTROL_EXTERIOR_LIGHTS));
        mProps.put(VehicleProperty.FOG_LIGHTS_SWITCH, new Pair<>(
                Car.PERMISSION_CONTROL_EXTERIOR_LIGHTS,
                Car.PERMISSION_CONTROL_EXTERIOR_LIGHTS));
        mProps.put(VehicleProperty.HAZARD_LIGHTS_SWITCH, new Pair<>(
                Car.PERMISSION_CONTROL_EXTERIOR_LIGHTS,
                Car.PERMISSION_CONTROL_EXTERIOR_LIGHTS));
        mProps.put(VehicleProperty.READING_LIGHTS_STATE, new Pair<>(
                Car.PERMISSION_READ_INTERIOR_LIGHTS,
                null));
        mProps.put(VehicleProperty.CABIN_LIGHTS_STATE, new Pair<>(
                Car.PERMISSION_READ_INTERIOR_LIGHTS,
                null));
        mProps.put(VehicleProperty.READING_LIGHTS_SWITCH, new Pair<>(
                Car.PERMISSION_CONTROL_INTERIOR_LIGHTS,
                Car.PERMISSION_CONTROL_INTERIOR_LIGHTS));
        mProps.put(VehicleProperty.CABIN_LIGHTS_SWITCH, new Pair<>(
                Car.PERMISSION_CONTROL_INTERIOR_LIGHTS,
                Car.PERMISSION_CONTROL_INTERIOR_LIGHTS));
        // Display_Units
        mProps.put(VehicleProperty.DISTANCE_DISPLAY_UNITS, new Pair<>(
                Car.PERMISSION_READ_DISPLAY_UNITS,
                Car.PERMISSION_CONTROL_DISPLAY_UNITS));
        mPropForUnits.add(VehicleProperty.DISTANCE_DISPLAY_UNITS);
        mProps.put(VehicleProperty.FUEL_VOLUME_DISPLAY_UNITS, new Pair<>(
                Car.PERMISSION_READ_DISPLAY_UNITS,
                Car.PERMISSION_CONTROL_DISPLAY_UNITS));
        mPropForUnits.add(VehicleProperty.FUEL_VOLUME_DISPLAY_UNITS);
        mProps.put(VehicleProperty.TIRE_PRESSURE_DISPLAY_UNITS, new Pair<>(
                Car.PERMISSION_READ_DISPLAY_UNITS,
                Car.PERMISSION_CONTROL_DISPLAY_UNITS));
        mPropForUnits.add(VehicleProperty.TIRE_PRESSURE_DISPLAY_UNITS);
        mProps.put(VehicleProperty.EV_BATTERY_DISPLAY_UNITS, new Pair<>(
                Car.PERMISSION_READ_DISPLAY_UNITS,
                Car.PERMISSION_CONTROL_DISPLAY_UNITS));
        mPropForUnits.add(VehicleProperty.EV_BATTERY_DISPLAY_UNITS);
        mProps.put(VehicleProperty.FUEL_CONSUMPTION_UNITS_DISTANCE_OVER_VOLUME, new Pair<>(
                Car.PERMISSION_READ_DISPLAY_UNITS,
                Car.PERMISSION_CONTROL_DISPLAY_UNITS));
        mPropForUnits.add(VehicleProperty.FUEL_CONSUMPTION_UNITS_DISTANCE_OVER_VOLUME);
        mProps.put(VehicleProperty.VEHICLE_SPEED_DISPLAY_UNITS, new Pair<>(
                Car.PERMISSION_READ_DISPLAY_UNITS,
                Car.PERMISSION_CONTROL_DISPLAY_UNITS));
        mPropForUnits.add(VehicleProperty.VEHICLE_SPEED_DISPLAY_UNITS);

        mProps.put(VehicleProperty.SUPPORT_CUSTOMIZE_VENDOR_PERMISSION, new Pair<>(
                Car.PERMISSION_READ_CAR_VENDOR_PERMISSION_INFO,
                null));

        // mPropToValidValue should contain all properties which has @data_enum in types.hal
        mPropToValidValue.put(VehicleProperty.INFO_FUEL_TYPE, FUEL_TYPE);
        mPropToValidValue.put(VehicleProperty.INFO_EV_CONNECTOR_TYPE, EV_CONNECTOR_TYPE);
        mPropToValidValue.put(VehicleProperty.INFO_FUEL_DOOR_LOCATION, PORT_LOCATION);
        mPropToValidValue.put(VehicleProperty.INFO_DRIVER_SEAT, VEHICLE_SEAT);
        mPropToValidValue.put(VehicleProperty.INFO_MULTI_EV_PORT_LOCATIONS, PORT_LOCATION);
        mPropToValidValue.put(VehicleProperty.ENGINE_OIL_LEVEL, OIL_LEVEL);
        mPropToValidValue.put(VehicleProperty.GEAR_SELECTION, VEHICLE_GEAR);
        mPropToValidValue.put(VehicleProperty.CURRENT_GEAR, VEHICLE_GEAR);
        mPropToValidValue.put(VehicleProperty.TURN_SIGNAL_STATE, TURN_SIGNAL);
        mPropToValidValue.put(VehicleProperty.IGNITION_STATE, IGNITION_STATE);
        mPropToValidValue.put(VehicleProperty.HVAC_TEMPERATURE_DISPLAY_UNITS, VEHICLE_UNITS);
        mPropToValidValue.put(VehicleProperty.DISTANCE_DISPLAY_UNITS, VEHICLE_UNITS);
        mPropToValidValue.put(VehicleProperty.FUEL_VOLUME_DISPLAY_UNITS, VEHICLE_UNITS);
        mPropToValidValue.put(VehicleProperty.TIRE_PRESSURE_DISPLAY_UNITS, VEHICLE_UNITS);
        mPropToValidValue.put(VehicleProperty.EV_BATTERY_DISPLAY_UNITS, VEHICLE_UNITS);
        mPropToValidValue.put(VehicleProperty.SEAT_OCCUPANCY, SEAT_OCCUPANCY_STATE);
        mPropToValidValue.put(VehicleProperty.HIGH_BEAM_LIGHTS_STATE, VEHICLE_LIGHT_STATE);
        mPropToValidValue.put(VehicleProperty.HEADLIGHTS_STATE, VEHICLE_LIGHT_STATE);
        mPropToValidValue.put(VehicleProperty.FOG_LIGHTS_STATE, VEHICLE_LIGHT_STATE);
        mPropToValidValue.put(VehicleProperty.HAZARD_LIGHTS_STATE, VEHICLE_LIGHT_STATE);
        mPropToValidValue.put(VehicleProperty.CABIN_LIGHTS_STATE, VEHICLE_LIGHT_STATE);
        mPropToValidValue.put(VehicleProperty.READING_LIGHTS_STATE, VEHICLE_LIGHT_STATE);
        mPropToValidValue.put(VehicleProperty.HEADLIGHTS_SWITCH, VEHICLE_LIGHT_SWITCH);
        mPropToValidValue.put(VehicleProperty.HIGH_BEAM_LIGHTS_SWITCH, VEHICLE_LIGHT_SWITCH);
        mPropToValidValue.put(VehicleProperty.FOG_LIGHTS_SWITCH, VEHICLE_LIGHT_SWITCH);
        mPropToValidValue.put(VehicleProperty.HAZARD_LIGHTS_SWITCH, VEHICLE_LIGHT_SWITCH);
        mPropToValidValue.put(VehicleProperty.CABIN_LIGHTS_SWITCH, VEHICLE_LIGHT_SWITCH);
        mPropToValidValue.put(VehicleProperty.READING_LIGHTS_SWITCH, VEHICLE_LIGHT_SWITCH);

        // mPropToValidBitFlag contains all properties which return values are combinations of bits
        mPropToValidBitFlag.put(VehicleProperty.HVAC_FAN_DIRECTION_AVAILABLE,
                HVAC_FAN_DIRECTION_COMBINATIONS);
        mPropToValidBitFlag.put(VehicleProperty.HVAC_FAN_DIRECTION,
                HVAC_FAN_DIRECTION_COMBINATIONS);
    }

    /**
     * @param propId Property ID
     * @return Read permission string for given property ID. NULL if property ID does not exist or
     * the property is not available for reading.
     */
    @Nullable
    public String getReadPermission(int propId) {
        Pair<String, String> p = mProps.get(propId);
        if (p != null) {
            // Property ID exists.  Return read permission.
            if (p.first == null) {
                Log.e(TAG, "propId is not available for reading : 0x" + toHexString(propId));
            }
            return p.first;
        } else if (isVendorProperty(propId)) {
            // if property is vendor property and do not have specific permission.
            return Car.PERMISSION_VENDOR_EXTENSION;
        } else {
            return null;
        }
    }

    /**
     * @param propId Property ID
     * @return Write permission string for given property ID. NULL if property ID does not exist or
     * the property is not writable.
     */
    @Nullable
    public String getWritePermission(int propId) {
        Pair<String, String> p = mProps.get(propId);
        if (p != null) {
            // Property ID exists.  Return write permission.
            if (p.second == null) {
                Log.e(TAG, "propId is not writable : 0x" + toHexString(propId));
            }
            return p.second;
        } else if (isVendorProperty(propId)) {
            // if property is vendor property and do not have specific permission.
            return Car.PERMISSION_VENDOR_EXTENSION;
        } else {
            return null;
        }
    }

    private static boolean isVendorProperty(int propId) {
        return (propId & VehiclePropertyGroup.MASK) == VehiclePropertyGroup.VENDOR;
    }
    /**
     * Check if property ID is in the list of known IDs that PropertyHalService is interested it.
     */
    public boolean isSupportedProperty(int propId) {
        // Property is in the list of supported properties
        return mProps.get(propId) != null || isVendorProperty(propId);
    }

    /**
     * Check if the property is one of display units properties.
     */
    public boolean isPropertyToChangeUnits(int propertyId) {
        return mPropForUnits.contains(propertyId);
    }

    /**
     * Overrides the permission map for vendor properties
     *
     * @param configArray the configArray for
     * {@link VehicleProperty#SUPPORT_CUSTOMIZE_VENDOR_PERMISSION}
     */
    public void customizeVendorPermission(@NonNull List<Integer> configArray) {
        if (configArray == null || configArray.size() % 3 != 0) {
            throw new IllegalArgumentException(
                    "ConfigArray for SUPPORT_CUSTOMIZE_VENDOR_PERMISSION is wrong");
        }
        int index = 0;
        while (index < configArray.size()) {
            int propId = configArray.get(index++);
            if (!isVendorProperty(propId)) {
                throw new IllegalArgumentException("Property Id: " + propId
                        + " is not in vendor range");
            }
            int readPermission = configArray.get(index++);
            int writePermission = configArray.get(index++);
            mProps.put(propId, new Pair<>(
                    toPermissionString(readPermission, propId),
                    toPermissionString(writePermission, propId)));
        }

    }

    /**
     * Map VehicleVendorPermission enums in VHAL to android permissions.
     *
     * @return permission string, return null if vendor property is not available.
     */
    @Nullable
    private String toPermissionString(int permissionEnum, int propId) {
        switch (permissionEnum) {
            case PERMISSION_CAR_VENDOR_DEFAULT:
                return Car.PERMISSION_VENDOR_EXTENSION;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_WINDOW:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_WINDOW;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_WINDOW:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_WINDOW;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_DOOR:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_DOOR;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_DOOR:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_DOOR;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_SEAT:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_SEAT;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_SEAT:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_SEAT;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_MIRROR:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_MIRROR;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_MIRROR:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_MIRROR;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_INFO:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_INFO;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_INFO:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_INFO;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_ENGINE:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_ENGINE;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_ENGINE:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_ENGINE;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_HVAC:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_HVAC;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_HVAC:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_HVAC;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_LIGHT:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_LIGHT;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_LIGHT:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_LIGHT;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_1:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_1;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_1:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_1;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_2:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_2;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_2:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_2;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_3:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_3;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_3:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_3;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_4:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_4;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_4:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_4;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_5:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_5;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_5:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_5;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_6:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_6;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_6:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_6;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_7:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_7;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_7:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_7;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_8:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_8;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_8:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_8;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_9:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_9;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_9:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_9;
            case PERMISSION_SET_CAR_VENDOR_CATEGORY_10:
                return VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_10;
            case PERMISSION_GET_CAR_VENDOR_CATEGORY_10:
                return VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_10;
            case PERMISSION_CAR_VENDOR_NOT_ACCESSIBLE:
                return null;
            default:
                throw new IllegalArgumentException("permission Id: " + permissionEnum
                    + " for property:" + propId + " is invalid vendor permission Id");
        }
    }

    /**
     * Checks property value's format for all properties. Checks property value range if property
     * has @data_enum flag in types.hal.
     * @return true if property value's payload is valid.
     */
    public boolean checkPayload(VehiclePropValue propValue) {
        // Mixed property uses config array to indicate the data format. Checked it when convert it
        // to CarPropertyValue.
        if ((propValue.prop & VehiclePropertyType.MASK) == VehiclePropertyType.MIXED) {
            return true;
        }
        if (!checkFormatForAllProperties(propValue)) {
            Log.e(TAG, "Property value" + propValue + "has an invalid data format");
            return false;
        }
        if (mPropToValidValue.containsKey(propValue.prop)) {
            return checkDataEnum(propValue);
        }
        if (mPropToValidBitFlag.containsKey(propValue.prop)) {
            return checkValidBitFlag(propValue);
        }
        return true;
    }

    private boolean checkValidBitFlag(VehiclePropValue propValue) {
        int flagCombination = mPropToValidBitFlag.get(propValue.prop);
        for (int value : propValue.value.int32Values) {
            if ((value & flagCombination) != value) {
                return false;
            }
        }
        return true;
    }

    private boolean checkFormatForAllProperties(VehiclePropValue propValue) {
        int propId = propValue.prop;
        VehiclePropValue.RawValue rawValue = propValue.value;
        //Records sum size of int32values, floatValue, int64Values, bytes, String
        int sizeOfAllValue = rawValue.int32Values.size() + rawValue.floatValues.size()
                + rawValue.int64Values.size() + rawValue.bytes.size()
                + rawValue.stringValue.length();
        if (sizeOfAllValue == 0) {
            Log.e(TAG, "Property value is empty: " + propValue);
            return false;
        }
        switch (propId & VehiclePropertyType.MASK) {
            case VehiclePropertyType.BOOLEAN:
            case VehiclePropertyType.INT32:
                return sizeOfAllValue == 1 && rawValue.int32Values.size() == 1;
            case VehiclePropertyType.FLOAT:
                return sizeOfAllValue == 1 && rawValue.floatValues.size() == 1;
            case VehiclePropertyType.INT64:
                return sizeOfAllValue == 1 && rawValue.int64Values.size() == 1;
            case VehiclePropertyType.FLOAT_VEC:
                return sizeOfAllValue == rawValue.floatValues.size();
            case VehiclePropertyType.INT64_VEC:
                return sizeOfAllValue == rawValue.int64Values.size();
            case VehiclePropertyType.INT32_VEC:
                return sizeOfAllValue == rawValue.int32Values.size();
            case VehiclePropertyType.BYTES:
                return sizeOfAllValue == rawValue.bytes.size();
            case VehiclePropertyType.STRING:
                return sizeOfAllValue == rawValue.stringValue.length();
            default:
                throw new IllegalArgumentException("Unexpected property type for propId: "
                        + Integer.toHexString(propId));
        }
    }
    private boolean checkDataEnum(VehiclePropValue propValue) {
        int propId = propValue.prop;
        VehiclePropValue.RawValue rawValue = propValue.value;
        Set<Integer> validValue = mPropToValidValue.get(propId);
        for (int value : rawValue.int32Values) {
            if (!validValue.contains(value)) {
                return false;
            }
        }
        return true;
    }

    private static List<Integer> getIntegersFromDataEnums(Class clazz) {
        Field[] fields = clazz.getDeclaredFields();
        List<Integer> integerList = new ArrayList<>(5);
        for (Field f : fields) {
            if (f.getType() == int.class) {
                try {
                    integerList.add(f.getInt(clazz));
                } catch (Exception e) {
                    Log.w(TAG, "Failed to get value");
                }
            }
        }
        return integerList;
    }

    // Generate all combinations at once
    private static int generateAllCombination(Class clazz) {
        List<Integer> allBits = getIntegersFromDataEnums(clazz);
        int combination = allBits.get(0);
        for (int i = 1; i < allBits.size(); i++) {
            combination |= allBits.get(i);
        }
        return combination;
    }
}
