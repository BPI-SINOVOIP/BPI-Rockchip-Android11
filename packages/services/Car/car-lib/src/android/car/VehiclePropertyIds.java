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

package android.car;

import android.annotation.RequiresPermission;

/**
 * Copy from android.hardware.automotive.vehicle-V2.0-java_gen_java/gen/android/hardware/automotive
 * /vehicle/V2_0. Need to update this file when vehicle propertyId is changed in VHAL.
 * Use it as PropertyId in getProperty() and setProperty() in
 * {@link android.car.hardware.property.CarPropertyManager}
 */
public final class VehiclePropertyIds {
    /**
     * Undefined property.  */
    public static final int INVALID = 0;
    /**
     * VIN of vehicle
     * Requires permission: {@link Car#PERMISSION_IDENTIFICATION}.
     */
    @RequiresPermission(Car.PERMISSION_IDENTIFICATION)
    public static final int INFO_VIN = 286261504;
    /**
     * Manufacturer of vehicle
     * Requires permission: {@link Car#PERMISSION_CAR_INFO}.
     */
    @RequiresPermission(Car.PERMISSION_CAR_INFO)
    public static final int INFO_MAKE = 286261505;
    /**
     * Model of vehicle
     * Requires permission: {@link Car#PERMISSION_CAR_INFO}.
     */
    @RequiresPermission(Car.PERMISSION_CAR_INFO)
    public static final int INFO_MODEL = 286261506;
    /**
     * Model year of vehicle.
     * Requires permission: {@link Car#PERMISSION_CAR_INFO}.
     */
    @RequiresPermission(Car.PERMISSION_CAR_INFO)
    public static final int INFO_MODEL_YEAR = 289407235;
    /**
     * Fuel capacity of the vehicle in milliliters
     * Requires permission: {@link Car#PERMISSION_CAR_INFO}.
     */
    @RequiresPermission(Car.PERMISSION_CAR_INFO)
    public static final int INFO_FUEL_CAPACITY = 291504388;
    /**
     * List of fuels the vehicle may use
     * Requires permission: {@link Car#PERMISSION_CAR_INFO}.
     */
    @RequiresPermission(Car.PERMISSION_CAR_INFO)
    public static final int INFO_FUEL_TYPE = 289472773;
    /**
     * Battery capacity of the vehicle, if EV or hybrid.  This is the nominal
     * battery capacity when the vehicle is new.
     * Requires permission: {@link Car#PERMISSION_CAR_INFO}.
     */
    @RequiresPermission(Car.PERMISSION_CAR_INFO)
    public static final int INFO_EV_BATTERY_CAPACITY = 291504390;
    /**
     * List of connectors this vehicle may use
     *
     * <p>Applications can query the property value by
     * {@link android.car.hardware.property.CarPropertyManager#getIntArrayProperty(int, int)}. The
     * return value is an integer array containing enums in {@link EvConnectorType}
     *
     * Requires permission: {@link Car#PERMISSION_CAR_INFO}.
     */
    @RequiresPermission(Car.PERMISSION_CAR_INFO)
    public static final int INFO_EV_CONNECTOR_TYPE = 289472775;
    /**
     * Fuel door location
     *
     * <p> Applications can query the property value by
     * {@link android.car.hardware.property.CarPropertyManager#getIntProperty(int, int)}. The return
     * value is one of enums in {@link PortLocationType}.
     *
     * Requires permission: {@link Car#PERMISSION_CAR_INFO}.
     */
    @RequiresPermission(Car.PERMISSION_CAR_INFO)
    public static final int INFO_FUEL_DOOR_LOCATION = 289407240;
    /**
     * EV port location
     *
     * <p> Applications can query the property value by
     * {@link android.car.hardware.property.CarPropertyManager#getIntProperty(int, int)}. The return
     * value is one of enums in {@link PortLocationType}.
     *
     * Requires permission: {@link Car#PERMISSION_CAR_INFO}.
     */
    @RequiresPermission(Car.PERMISSION_CAR_INFO)
    public static final int INFO_EV_PORT_LOCATION = 289407241;
    /**
     * Multiple EV port locations
     *
     * <p> Applications can query the property value by
     * {@link android.car.hardware.property.CarPropertyManager#getIntArrayProperty(int, int)}. The
     * return value is an integer array containing enums in {@link PortLocationType}.
     *
     * Requires permission: {@link Car#PERMISSION_CAR_INFO}.
     */
    @RequiresPermission(Car.PERMISSION_CAR_INFO)
    public static final int INFO_MULTI_EV_PORT_LOCATIONS = 289472780;
    /**
     * Driver's seat location
     * Requires permission: {@link Car#PERMISSION_CAR_INFO}.
     */
    @RequiresPermission(Car.PERMISSION_CAR_INFO)
    public static final int INFO_DRIVER_SEAT = 356516106;
    /**
     * Vehicle's exterior dimensions.
     * Requires permission: {@link Car#PERMISSION_CAR_INFO}.
     */
    @RequiresPermission(Car.PERMISSION_CAR_INFO)
    public static final int INFO_EXTERIOR_DIMENSIONS = 289472779;
    /**
     * Current odometer value of the vehicle
     * The property is protected by the signature permission: android.car.permission.CAR_MILEAGE.
     */
    @RequiresPermission(Car.PERMISSION_MILEAGE)
    public static final int PERF_ODOMETER = 291504644;
    /**
     * Speed of the vehicle
     * Requires permission: {@link Car#PERMISSION_SPEED}.
     */
    @RequiresPermission(Car.PERMISSION_SPEED)
    public static final int PERF_VEHICLE_SPEED = 291504647;
    /**
     * Speed of the vehicle for displays
     *
     * Some cars display a slightly slower speed than the actual speed. This is
     * usually displayed on the speedometer.
     * Requires permission: {@link Car#PERMISSION_SPEED}.
     */
    @RequiresPermission(Car.PERMISSION_SPEED)
    public static final int PERF_VEHICLE_SPEED_DISPLAY = 291504648;
    /**
     * Front bicycle model steering angle for vehicle
     *
     * Angle is in degrees. Left is negative.
     * Requires permission: {@link Car#PERMISSION_READ_STEERING_STATE}.
     */
    @RequiresPermission(Car.PERMISSION_READ_STEERING_STATE)
    public static final int PERF_STEERING_ANGLE = 291504649;
    /**
     * Rear bicycle model steering angle for vehicle
     *
     * Angle is in degrees. Left is negative.
     * Requires permission: {@link Car#PERMISSION_READ_STEERING_STATE}.
     */
    @RequiresPermission(Car.PERMISSION_READ_STEERING_STATE)
    public static final int PERF_REAR_STEERING_ANGLE = 291504656;
    /**
     * Temperature of engine coolant
     * The property is protected by the signature permission:
     * android.car.permission.CAR_ENGINE_DETAILED.
     */
    @RequiresPermission(Car.PERMISSION_CAR_ENGINE_DETAILED)
    public static final int ENGINE_COOLANT_TEMP = 291504897;
    /**
     * Engine oil level
     * The property is protected by the signature permission:
     * android.car.permission.CAR_ENGINE_DETAILED.
     */
    @RequiresPermission(Car.PERMISSION_CAR_ENGINE_DETAILED)
    public static final int ENGINE_OIL_LEVEL = 289407747;
    /**
     * Temperature of engine oil
     * The property is protected by the signature permission:
     * android.car.permission.CAR_ENGINE_DETAILED.
     */
    @RequiresPermission(Car.PERMISSION_CAR_ENGINE_DETAILED)
    public static final int ENGINE_OIL_TEMP = 291504900;
    /**
     * Engine rpm
     * The property is protected by the signature permission:
     * android.car.permission.CAR_ENGINE_DETAILED.
     */
    @RequiresPermission(Car.PERMISSION_CAR_ENGINE_DETAILED)
    public static final int ENGINE_RPM = 291504901;
    /**
     * Reports wheel ticks
     * Requires permission: {@link Car#PERMISSION_SPEED}.
     */
    @RequiresPermission(Car.PERMISSION_SPEED)
    public static final int WHEEL_TICK = 290521862;
    /**
     * Fuel remaining in the the vehicle, in milliliters
     * Requires permission: {@link Car#PERMISSION_ENERGY}.
     */
    @RequiresPermission(Car.PERMISSION_ENERGY)
    public static final int FUEL_LEVEL = 291504903;
    /**
     * Fuel door open
     * Requires permission: {@link Car#PERMISSION_ENERGY_PORTS} to read the property.
     * Requires the signature permission: android.car.permission.CONTROL_CAR_ENERGY_PORTS to write
     * the property.
     */
    @RequiresPermission.Read(@RequiresPermission(Car.PERMISSION_ENERGY_PORTS))
    @RequiresPermission.Write(@RequiresPermission(Car.PERMISSION_CONTROL_ENERGY_PORTS))
    public static final int FUEL_DOOR_OPEN = 287310600;
    /**
     * EV battery level in WH, if EV or hybrid
     * Requires permission: {@link Car#PERMISSION_ENERGY}.
     */
    @RequiresPermission(Car.PERMISSION_ENERGY)
    public static final int EV_BATTERY_LEVEL = 291504905;
    /**
     * EV charge port open
     * Requires permission: {@link Car#PERMISSION_ENERGY_PORTS} to read the property.
     * Requires the signature permission: android.car.permission.CONTROL_CAR_ENERGY_PORTS to write
     * the property.
     */
    @RequiresPermission.Read(@RequiresPermission(Car.PERMISSION_ENERGY_PORTS))
    @RequiresPermission.Write(@RequiresPermission(Car.PERMISSION_CONTROL_ENERGY_PORTS))
    public static final int EV_CHARGE_PORT_OPEN = 287310602;
    /**
     * EV charge port connected
     * Requires permission: {@link Car#PERMISSION_ENERGY_PORTS}.
     */
    @RequiresPermission(Car.PERMISSION_ENERGY_PORTS)
    public static final int EV_CHARGE_PORT_CONNECTED = 287310603;
    /**
     * EV instantaneous charge rate in milliwatts
     * Requires permission: {@link Car#PERMISSION_ENERGY}.
     */
    @RequiresPermission(Car.PERMISSION_ENERGY)
    public static final int EV_BATTERY_INSTANTANEOUS_CHARGE_RATE = 291504908;
    /**
     * Range remaining
     *
     * Meters remaining of fuel and charge.  Range remaining shall account for
     * all energy sources in a vehicle.  For example, a hybrid car's range will
     * be the sum of the ranges based on fuel and battery.
     * Requires permission: {@link Car#PERMISSION_ENERGY} to read the property.
     * Requires the signature permission: android.car.permission.ADJUST_RANGE_REMAINING to write
     * the property.
     */
    @RequiresPermission.Read(@RequiresPermission(Car.PERMISSION_ENERGY))
    @RequiresPermission.Write(@RequiresPermission(Car.PERMISSION_ADJUST_RANGE_REMAINING))
    public static final int RANGE_REMAINING = 291504904;
    /**
     * Tire pressure
     *
     * min/max value indicates tire pressure sensor range.  Each tire will have a separate min/max
     * value denoted by its areaConfig.areaId.
     * The property is protected by the signature permission: android.car.permission.CAR_TIRES.
     */
    @RequiresPermission(Car.PERMISSION_TIRES)
    public static final int TIRE_PRESSURE = 392168201;
    /**
     * Currently selected gear
     *
     * This is the gear selected by the user.
     * Requires permission: {@link Car#PERMISSION_POWERTRAIN}.
     */
    @RequiresPermission(Car.PERMISSION_POWERTRAIN)
    public static final int GEAR_SELECTION = 289408000;
    /**
     * Current gear. In non-manual case, selected gear may not
     * match the current gear. For example, if the selected gear is GEAR_DRIVE,
     * the current gear will be one of GEAR_1, GEAR_2 etc, which reflects
     * the actual gear the transmission is currently running in.
     * Requires permission: {@link Car#PERMISSION_POWERTRAIN}.
     */
    @RequiresPermission(Car.PERMISSION_POWERTRAIN)
    public static final int CURRENT_GEAR = 289408001;
    /**
     * Parking brake state.
     * Requires permission: {@link Car#PERMISSION_POWERTRAIN}.
     */
    @RequiresPermission(Car.PERMISSION_POWERTRAIN)
    public static final int PARKING_BRAKE_ON = 287310850;
    /**
     * Auto-apply parking brake.
     * Requires permission: {@link Car#PERMISSION_POWERTRAIN}.
     */
    @RequiresPermission(Car.PERMISSION_POWERTRAIN)
    public static final int PARKING_BRAKE_AUTO_APPLY = 287310851;
    /**
     * Warning for fuel low level.
     * Requires permission: {@link Car#PERMISSION_ENERGY}.
     */
    @RequiresPermission(Car.PERMISSION_ENERGY)
    public static final int FUEL_LEVEL_LOW = 287310853;
    /**
     * Night mode
     * Requires permission: {@link Car#PERMISSION_EXTERIOR_ENVIRONMENT}.
     */
    @RequiresPermission(Car.PERMISSION_EXTERIOR_ENVIRONMENT)
    public static final int NIGHT_MODE = 287310855;
    /**
     * State of the vehicles turn signals
     * The property is protected by the signature permission:
     * android.car.permission.CAR_EXTERIOR_LIGHTS .
     */
    @RequiresPermission(Car.PERMISSION_EXTERIOR_LIGHTS)
    public static final int TURN_SIGNAL_STATE = 289408008;
    /**
     * Represents ignition state
     * Requires permission: {@link Car#PERMISSION_POWERTRAIN}.
     */
    @RequiresPermission(Car.PERMISSION_POWERTRAIN)
    public static final int IGNITION_STATE = 289408009;
    /**
     * ABS is active
     * The property is protected by the signature permission:
     * android.car.permission.CAR_DYNAMICS_STATE.
     */
    @RequiresPermission(Car.PERMISSION_CAR_DYNAMICS_STATE)
    public static final int ABS_ACTIVE = 287310858;
    /**
     * Traction Control is active
     * The property is protected by the signature permission:
     * android.car.permission.CAR_DYNAMICS_STATE.
     */
    @RequiresPermission(Car.PERMISSION_CAR_DYNAMICS_STATE)
    public static final int TRACTION_CONTROL_ACTIVE = 287310859;
    /**
     * Fan speed setting
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_FAN_SPEED = 356517120;
    /**
     * Fan direction setting
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_FAN_DIRECTION = 356517121;
    /**
     * HVAC current temperature.
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_TEMPERATURE_CURRENT = 358614274;
    /**
     * HVAC, target temperature set.
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_TEMPERATURE_SET = 358614275;
    /**
     * On/off defrost for designated window
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_DEFROSTER = 320865540;
    /**
     * On/off AC for designated areaId
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_AC_ON = 354419973;
    /**
     * On/off max AC
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_MAX_AC_ON = 354419974;
    /**
     * On/off max defrost
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_MAX_DEFROST_ON = 354419975;
    /**
     * Recirculation on/off
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_RECIRC_ON = 354419976;
    /**
     * Enable temperature coupling between areas.
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_DUAL_ON = 354419977;
    /**
     * On/off automatic mode
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_AUTO_ON = 354419978;
    /**
     * Seat heating/cooling
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_SEAT_TEMPERATURE = 356517131;
    /**
     * Side Mirror Heat
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_SIDE_MIRROR_HEAT = 339739916;
    /**
     * Steering Wheel Heating/Cooling
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_STEERING_WHEEL_HEAT = 289408269;
    /**
     * Temperature units for display
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_TEMPERATURE_DISPLAY_UNITS = 289408270;
    /**
     * Actual fan speed
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_ACTUAL_FAN_SPEED_RPM = 356517135;
    /**
     * Represents global power state for HVAC. Setting this property to false
     * MAY mark some properties that control individual HVAC features/subsystems
     * to UNAVAILABLE state. Setting this property to true MAY mark some
     * properties that control individual HVAC features/subsystems to AVAILABLE
     * state (unless any/all of them are UNAVAILABLE on their own individual
     * merits).
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_POWER_ON = 354419984;
    /**
     * Fan Positions Available
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_FAN_DIRECTION_AVAILABLE = 356582673;
    /**
     * Automatic recirculation on/off
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_AUTO_RECIRC_ON = 354419986;
    /**
     * Seat ventilation
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_SEAT_VENTILATION = 356517139;
    /**
     * ELECTRIC DEFROSTER
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_CLIMATE.
     * @hide
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_CLIMATE)
    public static final int HVAC_ELECTRIC_DEFROSTER_ON = 320865556;
    /**
     * Distance units for display
     * Requires permission {@link Car#PERMISSION_READ_DISPLAY_UNITS} to read the property.
     * Requires two signature permissions: android.car.permission.CONTROL_CAR_DISPLAY_UNITS and
     * android.car.permission.CAR_VENDOR_EXTENSION to write the property.
     */
    @RequiresPermission.Read(@RequiresPermission(Car.PERMISSION_READ_DISPLAY_UNITS))
    @RequiresPermission.Write(@RequiresPermission(allOf = {Car.PERMISSION_CONTROL_DISPLAY_UNITS,
            Car.PERMISSION_VENDOR_EXTENSION}))
    public static final int DISTANCE_DISPLAY_UNITS = 289408512;
    /**
     * Fuel volume units for display
     * Requires permission {@link Car#PERMISSION_READ_DISPLAY_UNITS} to read the property.
     * Requires two signature permissions: android.car.permission.CONTROL_CAR_DISPLAY_UNITS and
     * android.car.permission.CAR_VENDOR_EXTENSION to write the property.
     */
    @RequiresPermission.Read(@RequiresPermission(Car.PERMISSION_READ_DISPLAY_UNITS))
    @RequiresPermission.Write(@RequiresPermission(allOf = {Car.PERMISSION_CONTROL_DISPLAY_UNITS,
            Car.PERMISSION_VENDOR_EXTENSION}))
    public static final int FUEL_VOLUME_DISPLAY_UNITS = 289408513;
    /**
     * Tire pressure units for display
     * Requires permission {@link Car#PERMISSION_READ_DISPLAY_UNITS} to read the property.
     * Requires two signature permissions: android.car.permission.CONTROL_CAR_DISPLAY_UNITS and
     * android.car.permission.CAR_VENDOR_EXTENSION to write the property.
     */
    @RequiresPermission.Read(@RequiresPermission(Car.PERMISSION_READ_DISPLAY_UNITS))
    @RequiresPermission.Write(@RequiresPermission(allOf = {Car.PERMISSION_CONTROL_DISPLAY_UNITS,
            Car.PERMISSION_VENDOR_EXTENSION}))
    public static final int TIRE_PRESSURE_DISPLAY_UNITS = 289408514;
    /**
     * EV battery units for display
     * Requires permission {@link Car#PERMISSION_READ_DISPLAY_UNITS} to read the property.
     * Requires two signature permissions: android.car.permission.CONTROL_CAR_DISPLAY_UNITS and
     * android.car.permission.CAR_VENDOR_EXTENSION to write the property.
     */
    @RequiresPermission.Read(@RequiresPermission(Car.PERMISSION_READ_DISPLAY_UNITS))
    @RequiresPermission.Write(@RequiresPermission(allOf = {Car.PERMISSION_CONTROL_DISPLAY_UNITS,
            Car.PERMISSION_VENDOR_EXTENSION}))
    public static final int EV_BATTERY_DISPLAY_UNITS = 289408515;
    /**
     * Speed Units for display
     * Requires permission {@link Car#PERMISSION_READ_DISPLAY_UNITS} to read the property.
     * Requires two signature permissions: android.car.permission.CONTROL_CAR_DISPLAY_UNITS and
     * android.car.permission.CAR_VENDOR_EXTENSION to write the property.
     * @hide
     */
    @RequiresPermission.Read(@RequiresPermission(Car.PERMISSION_READ_DISPLAY_UNITS))
    @RequiresPermission.Write(@RequiresPermission(allOf = {Car.PERMISSION_CONTROL_DISPLAY_UNITS,
            Car.PERMISSION_VENDOR_EXTENSION}))
    public static final int VEHICLE_SPEED_DISPLAY_UNITS = 289408516;
    /**
     * Fuel consumption units for display
     * Requires permission {@link Car#PERMISSION_READ_DISPLAY_UNITS} to read the property.
     * Requires two signature permissions: android.car.permission.CONTROL_CAR_DISPLAY_UNITS and
     * android.car.permission.CAR_VENDOR_EXTENSION to write the property.
     */
    @RequiresPermission.Read(@RequiresPermission(Car.PERMISSION_READ_DISPLAY_UNITS))
    @RequiresPermission.Write(@RequiresPermission(allOf = {Car.PERMISSION_CONTROL_DISPLAY_UNITS,
            Car.PERMISSION_VENDOR_EXTENSION}))
    public static final int FUEL_CONSUMPTION_UNITS_DISTANCE_OVER_VOLUME = 287311364;
    /**
     * Outside temperature
     * Requires permission: {@link Car#PERMISSION_EXTERIOR_ENVIRONMENT}.
     */
    @RequiresPermission(Car.PERMISSION_EXTERIOR_ENVIRONMENT)
    public static final int ENV_OUTSIDE_TEMPERATURE = 291505923;
    /**
     * Property to control power state of application processor
     *
     * It is assumed that AP's power state is controller by separate power
     * controller.
     * The property is protected by the signature permission: android.car.permission.CAR_POWER.
     */
    @RequiresPermission(Car.PERMISSION_CAR_POWER)
    public static final int AP_POWER_STATE_REQ = 289475072;
    /**
     * Property to report power state of application processor
     *
     * It is assumed that AP's power state is controller by separate power
     * controller.
     * The property is protected by the signature permission: android.car.permission.CAR_POWER.
     */
    @RequiresPermission(Car.PERMISSION_CAR_POWER)
    public static final int AP_POWER_STATE_REPORT = 289475073;
    /**
     * Property to report bootup reason for the current power on. This is a
     * static property that will not change for the whole duration until power
     * off. For example, even if user presses power on button after automatic
     * power on with door unlock, bootup reason must stay with
     * VehicleApPowerBootupReason#USER_UNLOCK.
     * The property is protected by the signature permission: android.car.permission.CAR_POWER.
     */
    @RequiresPermission(Car.PERMISSION_CAR_POWER)
    public static final int AP_POWER_BOOTUP_REASON = 289409538;
    /**
     * Property to represent brightness of the display. Some cars have single
     * control for the brightness of all displays and this property is to share
     * change in that control.
     * The property is protected by the signature permission: android.car.permission.CAR_POWER.
     */
    @RequiresPermission(Car.PERMISSION_CAR_POWER)
    public static final int DISPLAY_BRIGHTNESS = 289409539;
    /**
     * Property to feed H/W input events to android
     */
    public static final int HW_KEY_INPUT = 289475088;
    /**
     * Door position
     *
     * This is an integer in case a door may be set to a particular position.
     * Max value indicates fully open, min value (0) indicates fully closed.
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_DOORS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_DOORS)
    public static final int DOOR_POS = 373295872;
    /**
     * Door move
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_DOORS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_DOORS)
    public static final int DOOR_MOVE = 373295873;
    /**
     * Door lock
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_DOORS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_DOORS)
    public static final int DOOR_LOCK = 371198722;
    /**
     * Mirror Z Position
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_MIRRORS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_MIRRORS)
    public static final int MIRROR_Z_POS = 339741504;
    /**
     * Mirror Z Move
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_MIRRORS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_MIRRORS)
    public static final int MIRROR_Z_MOVE = 339741505;
    /**
     * Mirror Y Position
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_MIRRORS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_MIRRORS)
    public static final int MIRROR_Y_POS = 339741506;
    /**
     * Mirror Y Move
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_MIRRORS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_MIRRORS)
    public static final int MIRROR_Y_MOVE = 339741507;
    /**
     * Mirror Lock
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_MIRRORS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_MIRRORS)
    public static final int MIRROR_LOCK = 287312708;
    /**
     * Mirror Fold
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_MIRRORS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_MIRRORS)
    public static final int MIRROR_FOLD = 287312709;
    /**
     * Seat memory select
     *
     * This parameter selects the memory preset to use to select the seat
     * position. The minValue is always 0, and the maxValue determines the
     * number of seat positions available.
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_MEMORY_SELECT = 356518784;
    /**
     * Seat memory set
     *
     * This setting allows the user to save the current seat position settings
     * into the selected preset slot.  The maxValue for each seat position
     * must match the maxValue for SEAT_MEMORY_SELECT.
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_MEMORY_SET = 356518785;
    /**
     * Seatbelt buckled
     *
     * True indicates belt is buckled.
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_BELT_BUCKLED = 354421634;
    /**
     * Seatbelt height position
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_BELT_HEIGHT_POS = 356518787;
    /**
     * Seatbelt height move
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_BELT_HEIGHT_MOVE = 356518788;
    /**
     * Seat fore/aft position
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_FORE_AFT_POS = 356518789;
    /**
     * Seat fore/aft move
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_FORE_AFT_MOVE = 356518790;
    /**
     * Seat backrest angle 1 position
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_BACKREST_ANGLE_1_POS = 356518791;
    /**
     * Seat backrest angle 1 move
     *
     * Moves the backrest forward or recline.
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_BACKREST_ANGLE_1_MOVE = 356518792;
    /**
     * Seat backrest angle 2 position
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_BACKREST_ANGLE_2_POS = 356518793;
    /**
     * Seat backrest angle 2 move
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_BACKREST_ANGLE_2_MOVE = 356518794;
    /**
     * Seat height position
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_HEIGHT_POS = 356518795;
    /**
     * Seat height move
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_HEIGHT_MOVE = 356518796;
    /**
     * Seat depth position
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_DEPTH_POS = 356518797;
    /**
     * Seat depth move
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_DEPTH_MOVE = 356518798;
    /**
     * Seat tilt position
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_TILT_POS = 356518799;
    /**
     * Seat tilt move
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_TILT_MOVE = 356518800;
    /**
     * Lumber fore/aft position
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_LUMBAR_FORE_AFT_POS = 356518801;
    /**
     * Lumbar fore/aft move
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_LUMBAR_FORE_AFT_MOVE = 356518802;
    /**
     * Lumbar side support position
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_LUMBAR_SIDE_SUPPORT_POS = 356518803;
    /**
     * Lumbar side support move
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_LUMBAR_SIDE_SUPPORT_MOVE = 356518804;
    /**
     * Headrest height position
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_HEADREST_HEIGHT_POS = 289409941;
    /**
     * Headrest height move
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_HEADREST_HEIGHT_MOVE = 356518806;
    /**
     * Headrest angle position
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_HEADREST_ANGLE_POS = 356518807;
    /**
     * Headrest angle move
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_HEADREST_ANGLE_MOVE = 356518808;
    /**
     * Headrest fore/aft position
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_HEADREST_FORE_AFT_POS = 356518809;
    /**
     * Headrest fore/aft move
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_HEADREST_FORE_AFT_MOVE = 356518810;
    /**
     * Seat Occupancy
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_SEATS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_SEATS)
    public static final int SEAT_OCCUPANCY = 356518832;
    /**
     * Window Position
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_WINDOWS
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_WINDOWS)
    public static final int WINDOW_POS = 322964416;
    /**
     * Window Move
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_WINDOWS
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_WINDOWS)
    public static final int WINDOW_MOVE = 322964417;
    /**
     * Window Lock
     * The property is protected by the signature permission:
     * android.car.permission.CONTROL_CAR_WINDOWS
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_CAR_WINDOWS)
    public static final int WINDOW_LOCK = 320867268;
    /**
     * Vehicle Maps Service (VMS) message
     * The property is protected by the signature permissions:
     * android.car.permission.VMS_PUBLISHER and android.car.permission.VMS_SUBSCRIBER.
     */
    @RequiresPermission(anyOf = {Car.PERMISSION_VMS_PUBLISHER, Car.PERMISSION_VMS_SUBSCRIBER})
    public static final int VEHICLE_MAP_SERVICE = 299895808;
    /**
     * OBD2 Live Sensor Data
     *
     * Reports a snapshot of the current (live) values of the OBD2 sensors available.
     * The property is protected by the signature permission:
     * android.car.permission.CAR_DIAGNOSTICS.
     */
    @RequiresPermission(Car.PERMISSION_CAR_DIAGNOSTIC_READ_ALL)
    public static final int OBD2_LIVE_FRAME = 299896064;
    /**
     * OBD2 Freeze Frame Sensor Data
     *
     * Reports a snapshot of the value of the OBD2 sensors available at the time that a fault
     * occurred and was detected.
     * The property is protected by the signature permission:
     * android.car.permission.CAR_DIAGNOSTICS.
     */
    @RequiresPermission(Car.PERMISSION_CAR_DIAGNOSTIC_READ_ALL)
    public static final int OBD2_FREEZE_FRAME = 299896065;
    /**
     * OBD2 Freeze Frame Information
     * The property is protected by the signature permission:
     * android.car.permission.CAR_DIAGNOSTICS.
     */
    @RequiresPermission(Car.PERMISSION_CAR_DIAGNOSTIC_READ_ALL)
    public static final int OBD2_FREEZE_FRAME_INFO = 299896066;
    /**
     * OBD2 Freeze Frame Clear
     *
     * This property allows deletion of any of the freeze frames stored in
     * vehicle memory, as described by OBD2_FREEZE_FRAME_INFO.
     * The property is protected by the signature permission:
     * android.car.permission.CLEAR_CAR_DIAGNOSTICS.
     */
    @RequiresPermission(Car.PERMISSION_CAR_DIAGNOSTIC_CLEAR)
    public static final int OBD2_FREEZE_FRAME_CLEAR = 299896067;
    /**
     * Headlights State
     * The property is protected by the signature permission:
     * android.car.permission.CAR_EXTERIOR_LIGHTS.
     */
    @RequiresPermission(Car.PERMISSION_EXTERIOR_LIGHTS)
    public static final int HEADLIGHTS_STATE = 289410560;
    /**
     * High beam lights state
     * The property is protected by the signature permission:
     * android.car.permission.CAR_EXTERIOR_LIGHTS.
     */
    @RequiresPermission(Car.PERMISSION_EXTERIOR_LIGHTS)
    public static final int HIGH_BEAM_LIGHTS_STATE = 289410561;
    /**
     * Fog light state
     * The property is protected by the signature permission:
     * android.car.permission.CAR_EXTERIOR_LIGHTS.
     */
    @RequiresPermission(Car.PERMISSION_EXTERIOR_LIGHTS)
    public static final int FOG_LIGHTS_STATE = 289410562;
    /**
     * Hazard light status
     * The property is protected by the signature permission:
     * android.car.permission.CAR_EXTERIOR_LIGHTS.
     */
    @RequiresPermission(Car.PERMISSION_EXTERIOR_LIGHTS)
    public static final int HAZARD_LIGHTS_STATE = 289410563;
    /**
     * Headlight switch
     * The property is protected by the signature permission:
     * android.car.permission.CAR_EXTERIOR_LIGHTS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_EXTERIOR_LIGHTS)
    public static final int HEADLIGHTS_SWITCH = 289410576;
    /**
     * High beam light switch
     * The property is protected by the signature permission:
     * android.car.permission.CAR_EXTERIOR_LIGHTS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_EXTERIOR_LIGHTS)
    public static final int HIGH_BEAM_LIGHTS_SWITCH = 289410577;
    /**
     * Fog light switch
     * The property is protected by the signature permission:
     * android.car.permission.CAR_EXTERIOR_LIGHTS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_EXTERIOR_LIGHTS)
    public static final int FOG_LIGHTS_SWITCH = 289410578;
    /**
     * Hazard light switch
     * The property is protected by the signature permission:
     * android.car.permission.CAR_EXTERIOR_LIGHTS.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_EXTERIOR_LIGHTS)
    public static final int HAZARD_LIGHTS_SWITCH = 289410579;
    /**
     * Cabin lights
     * Requires permission: {@link Car#PERMISSION_READ_INTERIOR_LIGHTS}.
     */
    @RequiresPermission(Car.PERMISSION_READ_INTERIOR_LIGHTS)
    public static final int CABIN_LIGHTS_STATE = 289410817;
    /**
     * Cabin lights switch
     * Requires permission: {@link Car#PERMISSION_CONTROL_INTERIOR_LIGHTS}.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_INTERIOR_LIGHTS)
    public static final int CABIN_LIGHTS_SWITCH = 289410818;
    /**
     * Reading lights
     * Requires permission: {@link Car#PERMISSION_READ_INTERIOR_LIGHTS}.
     */
    @RequiresPermission(Car.PERMISSION_READ_INTERIOR_LIGHTS)
    public static final int READING_LIGHTS_STATE = 356519683;
    /**
     * Reading lights switch
     * Requires permission: {@link Car#PERMISSION_CONTROL_INTERIOR_LIGHTS}.
     */
    @RequiresPermission(Car.PERMISSION_CONTROL_INTERIOR_LIGHTS)
    public static final int READING_LIGHTS_SWITCH = 356519684;

    /**
     * Property to get the initial settings for multi-user management (such as initial user).
     *
     * <p>Doesn't require permission because it's not exposed through
     * {@link android.car.hardware.property.CarPropertyManager}.
     *
     * @hide
     */
    public static final int INITIAL_USER_INFO = 299896583;

    /**
     * Property to switch user for multi-user management.
     *
     * <p>Doesn't require permission because it's not exposed through
     * {@link android.car.hardware.property.CarPropertyManager}.
     *
     * @hide
     */
    public static final int SWITCH_USER = 299896584;

    /**
     * Property to create a new user for multi-user management.
     *
     * <p>Doesn't require permission because it's not exposed through
     * {@link android.car.hardware.property.CarPropertyManager}.
     *
     * @hide
     */
    public static final int CREATE_USER = 299896585;

    /**
     * Property to remove a new user for multi-user management.
     *
     * <p>Doesn't require permission because it's not exposed through
     * {@link android.car.hardware.property.CarPropertyManager}.
     *
     * @hide
     */
    public static final int REMOVE_USER = 299896586;

    /**
     * Property to get / set the user authentication types associated with an Android user.
     *
     * <p>Doesn't require permission because it's not exposed through
     * {@link android.car.hardware.property.CarPropertyManager}.
     *
     * @hide
     */
    public static final int USER_IDENTIFICATION_ASSOCIATION = 299896587;

    /**
     * Gets a user-friendly representation of a property.
     */
    public static String toString(int property) {
        switch (property) {
            case INVALID:
                return "INVALID";
            case INFO_VIN:
                return "INFO_VIN";
            case INFO_MAKE:
                return "INFO_MAKE";
            case INFO_MODEL:
                return "INFO_MODEL";
            case INFO_MODEL_YEAR:
                return "INFO_MODEL_YEAR";
            case INFO_FUEL_CAPACITY:
                return "INFO_FUEL_CAPACITY";
            case INFO_FUEL_TYPE:
                return "INFO_FUEL_TYPE";
            case INFO_EV_BATTERY_CAPACITY:
                return "INFO_EV_BATTERY_CAPACITY";
            case INFO_MULTI_EV_PORT_LOCATIONS:
                return "INFO_MULTI_EV_PORT_LOCATIONS";
            case INFO_EV_CONNECTOR_TYPE:
                return "INFO_EV_CONNECTOR_TYPE";
            case INFO_FUEL_DOOR_LOCATION:
                return "INFO_FUEL_DOOR_LOCATION";
            case INFO_EV_PORT_LOCATION:
                return "INFO_EV_PORT_LOCATION";
            case INFO_DRIVER_SEAT:
                return "INFO_DRIVER_SEAT";
            case INFO_EXTERIOR_DIMENSIONS:
                return "INFO_EXTERIOR_DIMENSIONS";
            case PERF_ODOMETER:
                return "PERF_ODOMETER";
            case PERF_VEHICLE_SPEED:
                return "PERF_VEHICLE_SPEED";
            case PERF_VEHICLE_SPEED_DISPLAY:
                return "PERF_VEHICLE_SPEED_DISPLAY";
            case PERF_STEERING_ANGLE:
                return "PERF_STEERING_ANGLE";
            case PERF_REAR_STEERING_ANGLE:
                return "PERF_REAR_STEERING_ANGLE";
            case ENGINE_COOLANT_TEMP:
                return "ENGINE_COOLANT_TEMP";
            case ENGINE_OIL_LEVEL:
                return "ENGINE_OIL_LEVEL";
            case ENGINE_OIL_TEMP:
                return "ENGINE_OIL_TEMP";
            case ENGINE_RPM:
                return "ENGINE_RPM";
            case WHEEL_TICK:
                return "WHEEL_TICK";
            case FUEL_LEVEL:
                return "FUEL_LEVEL";
            case FUEL_DOOR_OPEN:
                return "FUEL_DOOR_OPEN";
            case EV_BATTERY_LEVEL:
                return "EV_BATTERY_LEVEL";
            case EV_CHARGE_PORT_OPEN:
                return "EV_CHARGE_PORT_OPEN";
            case EV_CHARGE_PORT_CONNECTED:
                return "EV_CHARGE_PORT_CONNECTED";
            case EV_BATTERY_INSTANTANEOUS_CHARGE_RATE:
                return "EV_BATTERY_INSTANTANEOUS_CHARGE_RATE";
            case RANGE_REMAINING:
                return "RANGE_REMAINING";
            case TIRE_PRESSURE:
                return "TIRE_PRESSURE";
            case GEAR_SELECTION:
                return "GEAR_SELECTION";
            case CURRENT_GEAR:
                return "CURRENT_GEAR";
            case PARKING_BRAKE_ON:
                return "PARKING_BRAKE_ON";
            case PARKING_BRAKE_AUTO_APPLY:
                return "PARKING_BRAKE_AUTO_APPLY";
            case FUEL_LEVEL_LOW:
                return "FUEL_LEVEL_LOW";
            case NIGHT_MODE:
                return "NIGHT_MODE";
            case TURN_SIGNAL_STATE:
                return "TURN_SIGNAL_STATE";
            case IGNITION_STATE:
                return "IGNITION_STATE";
            case ABS_ACTIVE:
                return "ABS_ACTIVE";
            case TRACTION_CONTROL_ACTIVE:
                return "TRACTION_CONTROL_ACTIVE";
            case HVAC_FAN_SPEED:
                return "HVAC_FAN_SPEED";
            case HVAC_FAN_DIRECTION:
                return "HVAC_FAN_DIRECTION";
            case HVAC_TEMPERATURE_CURRENT:
                return "HVAC_TEMPERATURE_CURRENT";
            case HVAC_TEMPERATURE_SET:
                return "HVAC_TEMPERATURE_SET";
            case HVAC_DEFROSTER:
                return "HVAC_DEFROSTER";
            case HVAC_AC_ON:
                return "HVAC_AC_ON";
            case HVAC_MAX_AC_ON:
                return "HVAC_MAX_AC_ON";
            case HVAC_MAX_DEFROST_ON:
                return "HVAC_MAX_DEFROST_ON";
            case HVAC_RECIRC_ON:
                return "HVAC_RECIRC_ON";
            case HVAC_DUAL_ON:
                return "HVAC_DUAL_ON";
            case HVAC_AUTO_ON:
                return "HVAC_AUTO_ON";
            case HVAC_SEAT_TEMPERATURE:
                return "HVAC_SEAT_TEMPERATURE";
            case HVAC_SIDE_MIRROR_HEAT:
                return "HVAC_SIDE_MIRROR_HEAT";
            case HVAC_STEERING_WHEEL_HEAT:
                return "HVAC_STEERING_WHEEL_HEAT";
            case HVAC_TEMPERATURE_DISPLAY_UNITS:
                return "HVAC_TEMPERATURE_DISPLAY_UNITS";
            case HVAC_ACTUAL_FAN_SPEED_RPM:
                return "HVAC_ACTUAL_FAN_SPEED_RPM";
            case HVAC_POWER_ON:
                return "HVAC_POWER_ON";
            case HVAC_FAN_DIRECTION_AVAILABLE:
                return "HVAC_FAN_DIRECTION_AVAILABLE";
            case HVAC_AUTO_RECIRC_ON:
                return "HVAC_AUTO_RECIRC_ON";
            case HVAC_SEAT_VENTILATION:
                return "HVAC_SEAT_VENTILATION";
            case HVAC_ELECTRIC_DEFROSTER_ON:
                return "HVAC_ELECTRIC_DEFROSTER_ON";
            case DISTANCE_DISPLAY_UNITS:
                return "DISTANCE_DISPLAY_UNITS";
            case FUEL_VOLUME_DISPLAY_UNITS:
                return "FUEL_VOLUME_DISPLAY_UNITS";
            case TIRE_PRESSURE_DISPLAY_UNITS:
                return "TIRE_PRESSURE_DISPLAY_UNITS";
            case EV_BATTERY_DISPLAY_UNITS:
                return "EV_BATTERY_DISPLAY_UNITS";
            case FUEL_CONSUMPTION_UNITS_DISTANCE_OVER_VOLUME:
                return "FUEL_CONSUMPTION_UNITS_DISTANCE_OVER_VOLUME";
            case ENV_OUTSIDE_TEMPERATURE:
                return "ENV_OUTSIDE_TEMPERATURE";
            case AP_POWER_STATE_REQ:
                return "AP_POWER_STATE_REQ";
            case AP_POWER_STATE_REPORT:
                return "AP_POWER_STATE_REPORT";
            case AP_POWER_BOOTUP_REASON:
                return "AP_POWER_BOOTUP_REASON";
            case DISPLAY_BRIGHTNESS:
                return "DISPLAY_BRIGHTNESS";
            case HW_KEY_INPUT:
                return "HW_KEY_INPUT";
            case DOOR_POS:
                return "DOOR_POS";
            case DOOR_MOVE:
                return "DOOR_MOVE";
            case DOOR_LOCK:
                return "DOOR_LOCK";
            case MIRROR_Z_POS:
                return "MIRROR_Z_POS";
            case MIRROR_Z_MOVE:
                return "MIRROR_Z_MOVE";
            case MIRROR_Y_POS:
                return "MIRROR_Y_POS";
            case MIRROR_Y_MOVE:
                return "MIRROR_Y_MOVE";
            case MIRROR_LOCK:
                return "MIRROR_LOCK";
            case MIRROR_FOLD:
                return "MIRROR_FOLD";
            case SEAT_MEMORY_SELECT:
                return "SEAT_MEMORY_SELECT";
            case SEAT_MEMORY_SET:
                return "SEAT_MEMORY_SET";
            case SEAT_BELT_BUCKLED:
                return "SEAT_BELT_BUCKLED";
            case SEAT_BELT_HEIGHT_POS:
                return "SEAT_BELT_HEIGHT_POS";
            case SEAT_BELT_HEIGHT_MOVE:
                return "SEAT_BELT_HEIGHT_MOVE";
            case SEAT_FORE_AFT_POS:
                return "SEAT_FORE_AFT_POS";
            case SEAT_FORE_AFT_MOVE:
                return "SEAT_FORE_AFT_MOVE";
            case SEAT_BACKREST_ANGLE_1_POS:
                return "SEAT_BACKREST_ANGLE_1_POS";
            case SEAT_BACKREST_ANGLE_1_MOVE:
                return "SEAT_BACKREST_ANGLE_1_MOVE";
            case SEAT_BACKREST_ANGLE_2_POS:
                return "SEAT_BACKREST_ANGLE_2_POS";
            case SEAT_BACKREST_ANGLE_2_MOVE:
                return "SEAT_BACKREST_ANGLE_2_MOVE";
            case SEAT_HEIGHT_POS:
                return "SEAT_HEIGHT_POS";
            case SEAT_HEIGHT_MOVE:
                return "SEAT_HEIGHT_MOVE";
            case SEAT_DEPTH_POS:
                return "SEAT_DEPTH_POS";
            case SEAT_DEPTH_MOVE:
                return "SEAT_DEPTH_MOVE";
            case SEAT_TILT_POS:
                return "SEAT_TILT_POS";
            case SEAT_TILT_MOVE:
                return "SEAT_TILT_MOVE";
            case SEAT_LUMBAR_FORE_AFT_POS:
                return "SEAT_LUMBAR_FORE_AFT_POS";
            case SEAT_LUMBAR_FORE_AFT_MOVE:
                return "SEAT_LUMBAR_FORE_AFT_MOVE";
            case SEAT_LUMBAR_SIDE_SUPPORT_POS:
                return "SEAT_LUMBAR_SIDE_SUPPORT_POS";
            case SEAT_LUMBAR_SIDE_SUPPORT_MOVE:
                return "SEAT_LUMBAR_SIDE_SUPPORT_MOVE";
            case SEAT_HEADREST_HEIGHT_POS:
                return "SEAT_HEADREST_HEIGHT_POS";
            case SEAT_HEADREST_HEIGHT_MOVE:
                return "SEAT_HEADREST_HEIGHT_MOVE";
            case SEAT_HEADREST_ANGLE_POS:
                return "SEAT_HEADREST_ANGLE_POS";
            case SEAT_HEADREST_ANGLE_MOVE:
                return "SEAT_HEADREST_ANGLE_MOVE";
            case SEAT_HEADREST_FORE_AFT_POS:
                return "SEAT_HEADREST_FORE_AFT_POS";
            case SEAT_HEADREST_FORE_AFT_MOVE:
                return "SEAT_HEADREST_FORE_AFT_MOVE";
            case SEAT_OCCUPANCY:
                return "SEAT_OCCUPANCY";
            case WINDOW_POS:
                return "WINDOW_POS";
            case WINDOW_MOVE:
                return "WINDOW_MOVE";
            case WINDOW_LOCK:
                return "WINDOW_LOCK";
            case VEHICLE_MAP_SERVICE:
                return "VEHICLE_MAP_SERVICE";
            case OBD2_LIVE_FRAME:
                return "OBD2_LIVE_FRAME";
            case OBD2_FREEZE_FRAME:
                return "OBD2_FREEZE_FRAME";
            case OBD2_FREEZE_FRAME_INFO:
                return "OBD2_FREEZE_FRAME_INFO";
            case OBD2_FREEZE_FRAME_CLEAR:
                return "OBD2_FREEZE_FRAME_CLEAR";
            case HEADLIGHTS_STATE:
                return "HEADLIGHTS_STATE";
            case HIGH_BEAM_LIGHTS_STATE:
                return "HIGH_BEAM_LIGHTS_STATE";
            case FOG_LIGHTS_STATE:
                return "FOG_LIGHTS_STATE";
            case HAZARD_LIGHTS_STATE:
                return "HAZARD_LIGHTS_STATE";
            case HEADLIGHTS_SWITCH:
                return "HEADLIGHTS_SWITCH";
            case HIGH_BEAM_LIGHTS_SWITCH:
                return "HIGH_BEAM_LIGHTS_SWITCH";
            case FOG_LIGHTS_SWITCH:
                return "FOG_LIGHTS_SWITCH";
            case HAZARD_LIGHTS_SWITCH:
                return "HAZARD_LIGHTS_SWITCH";
            case CABIN_LIGHTS_STATE:
                return "CABIN_LIGHTS_STATE";
            case CABIN_LIGHTS_SWITCH:
                return "CABIN_LIGHTS_SWITCH";
            case READING_LIGHTS_STATE:
                return "READING_LIGHTS_STATE";
            case READING_LIGHTS_SWITCH:
                return "READING_LIGHTS_SWITCH";
            case VEHICLE_SPEED_DISPLAY_UNITS:
                return "VEHICLE_SPEED_DISPLAY_UNITS";
            case INITIAL_USER_INFO:
                return "INITIAL_USER_INFO";
            case SWITCH_USER:
                return "SWITCH_USER";
            case CREATE_USER:
                return "CREATE_USER";
            case REMOVE_USER:
                return "REMOVE_USER";
            case USER_IDENTIFICATION_ASSOCIATION:
                return "USER_IDENTIFICATION_ASSOCIATION";
            default:
                return "0x" + Integer.toHexString(property);
        }
    }
}
