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

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;
import static org.testng.Assert.assertThrows;

import android.car.Car;
import android.car.VehicleAreaType;
import android.car.VehiclePropertyIds;
import android.car.hardware.CarPropertyConfig;
import android.car.hardware.CarPropertyValue;
import android.car.hardware.property.CarInternalErrorException;
import android.car.hardware.property.CarPropertyManager;
import android.car.hardware.property.PropertyAccessDeniedSecurityException;
import android.car.hardware.property.PropertyNotAvailableAndRetryException;
import android.car.hardware.property.PropertyNotAvailableException;
import android.car.hardware.property.VehicleHalStatusCode;
import android.car.test.util.Visitor;
import android.hardware.automotive.vehicle.V2_0.VehicleArea;
import android.hardware.automotive.vehicle.V2_0.VehicleAreaSeat;
import android.hardware.automotive.vehicle.V2_0.VehiclePropValue;
import android.hardware.automotive.vehicle.V2_0.VehiclePropertyGroup;
import android.hardware.automotive.vehicle.V2_0.VehiclePropertyType;
import android.os.Build;
import android.os.ServiceSpecificException;
import android.os.SystemClock;
import android.util.ArraySet;
import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.MediumTest;

import com.android.car.vehiclehal.test.MockedVehicleHal.VehicleHalPropertyHandler;

import com.google.common.truth.Truth;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Test for {@link android.car.hardware.property.CarPropertyManager}
 */
@RunWith(AndroidJUnit4.class)
@MediumTest
public class CarPropertyManagerTest extends MockedCarTestBase {

    private static final String TAG = CarPropertyManagerTest.class.getSimpleName();

    /**
     * configArray[0], 1 indicates the property has a String value
     * configArray[1], 1 indicates the property has a Boolean value .
     * configArray[2], 1 indicates the property has a Integer value
     * configArray[3], the number indicates the size of Integer[]  in the property.
     * configArray[4], 1 indicates the property has a Long value .
     * configArray[5], the number indicates the size of Long[]  in the property.
     * configArray[6], 1 indicates the property has a Float value .
     * configArray[7], the number indicates the size of Float[] in the property.
     * configArray[8], the number indicates the size of byte[] in the property.
     */
    private static final java.util.Collection<Integer> CONFIG_ARRAY_1 =
            Arrays.asList(1, 0, 1, 0, 1, 0, 0, 0, 0);
    private static final java.util.Collection<Integer> CONFIG_ARRAY_2 =
            Arrays.asList(1, 1, 1, 0, 0, 0, 0, 2, 0);
    private static final Object[] EXPECTED_VALUE_1 = {"android", 1, 1L};
    private static final Object[] EXPECTED_VALUE_2 = {"android", true, 3, 1.1f, 2f};

    private static final int CUSTOM_SEAT_INT_PROP_1 =
            0x1201 | VehiclePropertyGroup.VENDOR | VehiclePropertyType.INT32 | VehicleArea.SEAT;
    private static final int CUSTOM_SEAT_INT_PROP_2 =
            0x1202 | VehiclePropertyGroup.VENDOR | VehiclePropertyType.INT32 | VehicleArea.SEAT;

    private static final int CUSTOM_SEAT_MIXED_PROP_ID_1 =
            0x1101 | VehiclePropertyGroup.VENDOR | VehiclePropertyType.MIXED | VehicleArea.SEAT;
    private static final int CUSTOM_GLOBAL_MIXED_PROP_ID_2 =
            0x1102 | VehiclePropertyGroup.VENDOR | VehiclePropertyType.MIXED | VehicleArea.GLOBAL;

    private static final int CUSTOM_GLOBAL_INT_ARRAY_PROP =
            0x1103 | VehiclePropertyGroup.VENDOR | VehiclePropertyType.INT32_VEC
                    | VehicleArea.GLOBAL;
    private static final Integer[] FAKE_INT_ARRAY_VALUE = {1, 2};

    // Vendor properties for testing exceptions.
    private static final int PROP_CAUSE_STATUS_CODE_TRY_AGAIN =
            0x1201 | VehiclePropertyGroup.VENDOR | VehiclePropertyType.INT32 | VehicleArea.GLOBAL;
    private static final int PROP_CAUSE_STATUS_CODE_INVALID_ARG =
            0x1202 | VehiclePropertyGroup.VENDOR | VehiclePropertyType.INT32 | VehicleArea.GLOBAL;
    private static final int PROP_CAUSE_STATUS_CODE_NOT_AVAILABLE =
            0x1203 | VehiclePropertyGroup.VENDOR | VehiclePropertyType.INT32 | VehicleArea.GLOBAL;
    private static final int PROP_CAUSE_STATUS_CODE_INTERNAL_ERROR =
            0x1204 | VehiclePropertyGroup.VENDOR | VehiclePropertyType.INT32 | VehicleArea.GLOBAL;
    private static final int PROP_CAUSE_STATUS_CODE_ACCESS_DENIED =
            0x1205 | VehiclePropertyGroup.VENDOR | VehiclePropertyType.INT32 | VehicleArea.GLOBAL;

    // Use FAKE_PROPERTY_ID to test api return null or throw exception.
    private static final int FAKE_PROPERTY_ID = 0x111;

    private static final int DRIVER_SIDE_AREA_ID = VehicleAreaSeat.ROW_1_LEFT
                                                    | VehicleAreaSeat.ROW_2_LEFT;
    private static final int PASSENGER_SIDE_AREA_ID = VehicleAreaSeat.ROW_1_RIGHT
                                                    | VehicleAreaSeat.ROW_2_CENTER
                                                    | VehicleAreaSeat.ROW_2_RIGHT;
    private static final float INIT_TEMP_VALUE = 16f;
    private static final float CHANGED_TEMP_VALUE = 20f;
    private static final int CALLBACK_SHORT_TIMEOUT_MS = 250; // ms
    // Wait for CarPropertyManager register/unregister listener
    private static final long WAIT_FOR_NO_EVENTS = 50;

    private static final List<Integer> USER_HAL_PROPERTIES = Arrays.asList(
            VehiclePropertyIds.INITIAL_USER_INFO,
            VehiclePropertyIds.SWITCH_USER,
            VehiclePropertyIds.CREATE_USER,
            VehiclePropertyIds.REMOVE_USER,
            VehiclePropertyIds.USER_IDENTIFICATION_ASSOCIATION
            );

    private CarPropertyManager mManager;

    @Rule public TestName mTestName = new TestName();

    @Override
    public void setUp() throws Exception {
        super.setUp();
        setUpTargetSdk();
        mManager = (CarPropertyManager) getCar().getCarManager(Car.PROPERTY_SERVICE);
        assertThat(mManager).isNotNull();
    }

    private void setUpTargetSdk() {
        if (mTestName.getMethodName().endsWith("InQ")) {
            getContext().getApplicationInfo().targetSdkVersion = Build.VERSION_CODES.Q;
        } else if (mTestName.getMethodName().endsWith("InR")) {
            getContext().getApplicationInfo().targetSdkVersion = Build.VERSION_CODES.R;
        }
    }

    @Test
    public void testMixedPropertyConfigs() {
        List<CarPropertyConfig> configs = mManager.getPropertyList();
        for (CarPropertyConfig cfg : configs) {
            switch (cfg.getPropertyId()) {
                case CUSTOM_SEAT_MIXED_PROP_ID_1:
                    assertThat(cfg.getConfigArray()).containsExactlyElementsIn(CONFIG_ARRAY_1)
                            .inOrder();
                    break;
                case CUSTOM_GLOBAL_MIXED_PROP_ID_2:
                    assertThat(cfg.getConfigArray()).containsExactlyElementsIn(CONFIG_ARRAY_2)
                            .inOrder();
                    break;
                case VehiclePropertyIds.HVAC_TEMPERATURE_SET:
                case PROP_CAUSE_STATUS_CODE_ACCESS_DENIED:
                case PROP_CAUSE_STATUS_CODE_INTERNAL_ERROR:
                case PROP_CAUSE_STATUS_CODE_TRY_AGAIN:
                case PROP_CAUSE_STATUS_CODE_NOT_AVAILABLE:
                case PROP_CAUSE_STATUS_CODE_INVALID_ARG:
                case CUSTOM_SEAT_INT_PROP_1:
                case CUSTOM_SEAT_INT_PROP_2:
                case CUSTOM_GLOBAL_INT_ARRAY_PROP:
                case VehiclePropertyIds.INFO_VIN:
                    break;
                default:
                    Assert.fail("Unexpected CarPropertyConfig: " + cfg.toString());
            }
        }
    }

    @Test
    public void testGetMixTypeProperty() {
        mManager.setProperty(Object[].class, CUSTOM_SEAT_MIXED_PROP_ID_1,
                0, EXPECTED_VALUE_1);
        CarPropertyValue<Object[]> result = mManager.getProperty(
                CUSTOM_SEAT_MIXED_PROP_ID_1, 0);
        assertThat(result.getValue()).isEqualTo(EXPECTED_VALUE_1);
        mManager.setProperty(Object[].class, CUSTOM_GLOBAL_MIXED_PROP_ID_2,
                0, EXPECTED_VALUE_2);
        result = mManager.getProperty(
                CUSTOM_GLOBAL_MIXED_PROP_ID_2, 0);
        assertThat(result.getValue()).isEqualTo(EXPECTED_VALUE_2);
    }

    /**
     * Test {@link android.car.hardware.property.CarPropertyManager#getIntArrayProperty(int, int)}
     */
    @Test
    public void testGetIntArrayProperty() {
        mManager.setProperty(Integer[].class, CUSTOM_GLOBAL_INT_ARRAY_PROP, VehicleArea.GLOBAL,
                FAKE_INT_ARRAY_VALUE);

        int[] result = mManager.getIntArrayProperty(CUSTOM_GLOBAL_INT_ARRAY_PROP,
                VehicleArea.GLOBAL);
        assertThat(result).asList().containsExactlyElementsIn(FAKE_INT_ARRAY_VALUE);
    }

    /**
     * Test {@link CarPropertyManager#getProperty(Class, int, int)}
     */
    @Test
    public void testGetPropertyWithClass() {
        mManager.setProperty(Integer[].class, CUSTOM_GLOBAL_INT_ARRAY_PROP, VehicleArea.GLOBAL,
                FAKE_INT_ARRAY_VALUE);

        CarPropertyValue<Integer[]> result = mManager.getProperty(Integer[].class,
                CUSTOM_GLOBAL_INT_ARRAY_PROP, VehicleArea.GLOBAL);
        assertThat(result.getValue()).asList().containsExactlyElementsIn(FAKE_INT_ARRAY_VALUE);
    }

    /**
     * Test {@link CarPropertyManager#isPropertyAvailable(int, int)}
     */
    @Test
    public void testIsPropertyAvailable() {
        assertThat(mManager.isPropertyAvailable(FAKE_PROPERTY_ID, VehicleArea.GLOBAL)).isFalse();
        assertThat(mManager.isPropertyAvailable(CUSTOM_GLOBAL_INT_ARRAY_PROP, VehicleArea.GLOBAL))
                .isTrue();
    }

    /**
     * Test {@link CarPropertyManager#getWritePermission(int)}
     * and {@link CarPropertyManager#getWritePermission(int)}
     */
    @Test
    public void testGetPermission() {
        String hvacReadPermission = mManager.getReadPermission(
                VehiclePropertyIds.HVAC_TEMPERATURE_SET);
        assertThat(hvacReadPermission).isEqualTo(Car.PERMISSION_CONTROL_CAR_CLIMATE);
        String hvacWritePermission = mManager.getWritePermission(
                VehiclePropertyIds.HVAC_TEMPERATURE_SET);
        assertThat(hvacWritePermission).isEqualTo(Car.PERMISSION_CONTROL_CAR_CLIMATE);

        // For read-only property
        String vinReadPermission = mManager.getReadPermission(VehiclePropertyIds.INFO_VIN);
        assertThat(vinReadPermission).isEqualTo(Car.PERMISSION_IDENTIFICATION);
        String vinWritePermission = mManager.getWritePermission(VehiclePropertyIds.INFO_VIN);
        assertThat(vinWritePermission).isNull();
    }

    @Test
    public void testGetPropertyConfig() {
        CarPropertyConfig config = mManager.getCarPropertyConfig(CUSTOM_SEAT_MIXED_PROP_ID_1);
        assertThat(config.getPropertyId()).isEqualTo(CUSTOM_SEAT_MIXED_PROP_ID_1);
        // return null if can not find the propertyConfig for the property.
        assertThat(mManager.getCarPropertyConfig(FAKE_PROPERTY_ID)).isNull();
    }

    @Test
    public void testGetAreaId() {
        int result = mManager.getAreaId(CUSTOM_SEAT_MIXED_PROP_ID_1, VehicleAreaSeat.ROW_1_LEFT);
        assertThat(result).isEqualTo(DRIVER_SIDE_AREA_ID);
        //test for the GLOBAL property
        int globalAreaId =
                mManager.getAreaId(CUSTOM_GLOBAL_MIXED_PROP_ID_2, VehicleAreaSeat.ROW_1_LEFT);
        assertThat(globalAreaId).isEqualTo(VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL);
        //test exception
        assertThrows(IllegalArgumentException.class, () -> mManager.getAreaId(
                CUSTOM_SEAT_MIXED_PROP_ID_1, VehicleAreaSeat.ROW_3_CENTER));
        assertThrows(IllegalArgumentException.class, () -> mManager.getAreaId(FAKE_PROPERTY_ID,
                VehicleAreaSeat.ROW_1_LEFT));
    }

    @Test
    public void testNotReceiveOnErrorEvent() throws Exception {
        TestErrorCallback callback = new TestErrorCallback();
        mManager.registerCallback(callback, VehiclePropertyIds.HVAC_TEMPERATURE_SET,
                CarPropertyManager.SENSOR_RATE_ONCHANGE);
        callback.assertRegisterCompleted();
        injectErrorEvent(VehiclePropertyIds.HVAC_TEMPERATURE_SET, PASSENGER_SIDE_AREA_ID,
                CarPropertyManager.CAR_SET_PROPERTY_ERROR_CODE_UNKNOWN);
        // app never change the value of HVAC_TEMPERATURE_SET, it won't get an error code.
        callback.assertOnErrorEventNotCalled();
    }

    @Test
    public void testReceiveOnErrorEvent() throws Exception {
        TestErrorCallback callback = new TestErrorCallback();
        mManager.registerCallback(callback, VehiclePropertyIds.HVAC_TEMPERATURE_SET,
                CarPropertyManager.SENSOR_RATE_ONCHANGE);
        callback.assertRegisterCompleted();
        mManager.setFloatProperty(
                VehiclePropertyIds.HVAC_TEMPERATURE_SET, PASSENGER_SIDE_AREA_ID,
                CHANGED_TEMP_VALUE);
        injectErrorEvent(VehiclePropertyIds.HVAC_TEMPERATURE_SET, PASSENGER_SIDE_AREA_ID,
                CarPropertyManager.CAR_SET_PROPERTY_ERROR_CODE_UNKNOWN);
        callback.assertOnErrorEventCalled();
        assertThat(callback.mReceivedErrorEventWithErrorCode).isTrue();
        assertThat(callback.mErrorCode).isEqualTo(
                CarPropertyManager.CAR_SET_PROPERTY_ERROR_CODE_UNKNOWN);
        assertThat(callback.mReceivedErrorEventWithOutErrorCode).isFalse();
    }

    @Test
    public void testNotReceiveOnErrorEventAfterUnregister() throws Exception {
        TestErrorCallback callback1 = new TestErrorCallback();
        mManager.registerCallback(callback1, VehiclePropertyIds.HVAC_TEMPERATURE_SET,
                CarPropertyManager.SENSOR_RATE_ONCHANGE);
        callback1.assertRegisterCompleted();
        TestErrorCallback callback2 = new TestErrorCallback();
        mManager.registerCallback(callback2, VehiclePropertyIds.HVAC_TEMPERATURE_SET,
                CarPropertyManager.SENSOR_RATE_ONCHANGE);
        mManager.setFloatProperty(
                VehiclePropertyIds.HVAC_TEMPERATURE_SET, PASSENGER_SIDE_AREA_ID,
                CHANGED_TEMP_VALUE);
        mManager.unregisterCallback(callback1, VehiclePropertyIds.HVAC_TEMPERATURE_SET);
        SystemClock.sleep(WAIT_FOR_NO_EVENTS);
        injectErrorEvent(VehiclePropertyIds.HVAC_TEMPERATURE_SET, PASSENGER_SIDE_AREA_ID,
                CarPropertyManager.CAR_SET_PROPERTY_ERROR_CODE_UNKNOWN);
        // callback1 is unregistered
        callback1.assertOnErrorEventNotCalled();
        callback2.assertOnErrorEventCalled();
    }
    @Test
    public void testSetterExceptionsInQ() {
        Truth.assertThat(getContext().getApplicationInfo().targetSdkVersion)
                .isEqualTo(Build.VERSION_CODES.Q);

        assertThrows(IllegalStateException.class,
                ()->mManager.setProperty(Integer.class, PROP_CAUSE_STATUS_CODE_ACCESS_DENIED,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL, 1));
        assertThrows(IllegalStateException.class,
                ()->mManager.setProperty(Integer.class, PROP_CAUSE_STATUS_CODE_INTERNAL_ERROR,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL, 1));
        assertThrows(IllegalStateException.class,
                ()->mManager.setProperty(Integer.class, PROP_CAUSE_STATUS_CODE_NOT_AVAILABLE,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL, 1));
        assertThrows(IllegalArgumentException.class,
                ()->mManager.setProperty(Integer.class, PROP_CAUSE_STATUS_CODE_INVALID_ARG,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL, 1));
        assertThrows(RuntimeException.class,
                ()->mManager.setProperty(Integer.class, PROP_CAUSE_STATUS_CODE_ACCESS_DENIED,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL, 1));
    }

    @Test
    public void testSetterExceptionsInR() {
        Truth.assertThat(getContext().getApplicationInfo().targetSdkVersion)
                .isEqualTo(Build.VERSION_CODES.R);

        assertThrows(PropertyAccessDeniedSecurityException.class,
                ()->mManager.setProperty(Integer.class, PROP_CAUSE_STATUS_CODE_ACCESS_DENIED,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL, 1));
        assertThrows(PropertyNotAvailableAndRetryException.class,
                ()->mManager.setProperty(Integer.class, PROP_CAUSE_STATUS_CODE_TRY_AGAIN,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL, 1));
        assertThrows(PropertyNotAvailableException.class,
                ()->mManager.setProperty(Integer.class, PROP_CAUSE_STATUS_CODE_NOT_AVAILABLE,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL, 1));
        assertThrows(CarInternalErrorException.class,
                ()->mManager.setProperty(Integer.class, PROP_CAUSE_STATUS_CODE_INTERNAL_ERROR,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL, 1));
        assertThrows(IllegalArgumentException.class,
                ()->mManager.setProperty(Integer.class, PROP_CAUSE_STATUS_CODE_INVALID_ARG,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL, 1));
    }

    @Test
    public void testGetterExceptionsInQ() {
        Truth.assertThat(getContext().getApplicationInfo().targetSdkVersion)
                .isEqualTo(Build.VERSION_CODES.Q);

        assertThrows(IllegalStateException.class,
                ()->mManager.getProperty(PROP_CAUSE_STATUS_CODE_ACCESS_DENIED,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL));
        assertThrows(IllegalArgumentException.class,
                ()->mManager.getProperty(PROP_CAUSE_STATUS_CODE_INVALID_ARG,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL));
        assertThrows(IllegalStateException.class,
                ()->mManager.getProperty(PROP_CAUSE_STATUS_CODE_NOT_AVAILABLE,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL));
        assertThrows(IllegalStateException.class,
                ()->mManager.getProperty(PROP_CAUSE_STATUS_CODE_INTERNAL_ERROR,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL));
        Truth.assertThat(mManager.getProperty(PROP_CAUSE_STATUS_CODE_TRY_AGAIN,
                VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL)).isNull();
    }

    @Test
    public void testGetterExceptionsInR() {
        Truth.assertThat(getContext().getApplicationInfo().targetSdkVersion)
                .isEqualTo(Build.VERSION_CODES.R);

        assertThrows(PropertyAccessDeniedSecurityException.class,
                () -> mManager.getProperty(PROP_CAUSE_STATUS_CODE_ACCESS_DENIED,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL));
        assertThrows(IllegalArgumentException.class,
                () -> mManager.getProperty(PROP_CAUSE_STATUS_CODE_INVALID_ARG,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL));
        assertThrows(PropertyNotAvailableAndRetryException.class,
                () -> mManager.getProperty(PROP_CAUSE_STATUS_CODE_TRY_AGAIN,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL));
        assertThrows(PropertyNotAvailableException.class,
                () -> mManager.getProperty(PROP_CAUSE_STATUS_CODE_NOT_AVAILABLE,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL));
        assertThrows(CarInternalErrorException.class,
                () -> mManager.getProperty(PROP_CAUSE_STATUS_CODE_INTERNAL_ERROR,
                        VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL));
    }

    @Test
    public void testOnChangeEventWithSameAreaId() throws Exception {
        // init
        mManager.setProperty(Integer.class,
                CUSTOM_SEAT_INT_PROP_1, DRIVER_SIDE_AREA_ID, 1);
        TestSequenceCallback callback = new TestSequenceCallback(1);
        mManager.registerCallback(callback, CUSTOM_SEAT_INT_PROP_1, 0);
        callback.assertRegisterCompleted();

        VehiclePropValue firstFakeValueDriveSide = new VehiclePropValue();
        firstFakeValueDriveSide.prop = CUSTOM_SEAT_INT_PROP_1;
        firstFakeValueDriveSide.areaId = DRIVER_SIDE_AREA_ID;
        firstFakeValueDriveSide.value.int32Values.add(2);
        firstFakeValueDriveSide.timestamp = SystemClock.elapsedRealtimeNanos();
        VehiclePropValue secFakeValueDriveSide = new VehiclePropValue();
        secFakeValueDriveSide.prop = CUSTOM_SEAT_INT_PROP_1;
        secFakeValueDriveSide.areaId = DRIVER_SIDE_AREA_ID;
        secFakeValueDriveSide.value.int32Values.add(3); // 0 in HAL indicate false;
        secFakeValueDriveSide.timestamp = SystemClock.elapsedRealtimeNanos();
        // inject the new event first
        getMockedVehicleHal().injectEvent(secFakeValueDriveSide);
        // inject the old event
        getMockedVehicleHal().injectEvent(firstFakeValueDriveSide);
        callback.assertOnChangeEventCalled();
        // Client should only get the new event
        assertThat((int) callback.getLastCarPropertyValue(CUSTOM_SEAT_INT_PROP_1).getValue())
                .isEqualTo(3);
        assertThat(callback.getEventCounter()).isEqualTo(1);

    }

    @Test
    public void testOnChangeEventWithDifferentAreaId() throws Exception {
        // init
        mManager.setProperty(Integer.class,
                CUSTOM_SEAT_INT_PROP_2, DRIVER_SIDE_AREA_ID, 1);
        TestSequenceCallback callback = new TestSequenceCallback(2);
        mManager.registerCallback(callback, CUSTOM_SEAT_INT_PROP_2, 0);
        callback.assertRegisterCompleted();
        VehiclePropValue fakeValueDriveSide = new VehiclePropValue();
        fakeValueDriveSide.prop = CUSTOM_SEAT_INT_PROP_2;
        fakeValueDriveSide.areaId = DRIVER_SIDE_AREA_ID;
        fakeValueDriveSide.value.int32Values.add(4);
        fakeValueDriveSide.timestamp = SystemClock.elapsedRealtimeNanos();

        VehiclePropValue fakeValuePsgSide = new VehiclePropValue();
        fakeValuePsgSide.prop = CUSTOM_SEAT_INT_PROP_2;
        fakeValuePsgSide.areaId = PASSENGER_SIDE_AREA_ID;
        fakeValuePsgSide.value.int32Values.add(5);
        fakeValuePsgSide.timestamp = SystemClock.elapsedRealtimeNanos();

        // inject passenger event before driver event
        getMockedVehicleHal().injectEvent(fakeValuePsgSide);
        getMockedVehicleHal().injectEvent(fakeValueDriveSide);
        callback.assertOnChangeEventCalled();

        // both events should be received by listener
        assertThat((int) callback.getLastCarPropertyValue(CUSTOM_SEAT_INT_PROP_2).getValue())
                .isEqualTo(4);
        assertThat(callback.getEventCounter()).isEqualTo(2);
    }

    @Test
    public void testUserHal_getProperty() {
        userHalPropertiesTest("getProperty()", (prop) ->
                mManager.getProperty(prop, /* areaId= */ 0));
    }

    @Test
    public void testUserHal_getBooleanProperty() {
        userHalPropertiesTest("getBooleanProperty()", (prop) ->
                mManager.getBooleanProperty(prop, /* areaId= */ 0));
    }

    @Test
    public void testUserHal_getIntProperty() {
        userHalPropertiesTest("getIntProperty()", (prop) ->
                mManager.getIntProperty(prop, /* areaId= */ 0));
    }

    @Test
    public void testUserHal_getIntArrayProperty() {
        userHalPropertiesTest("getIntArrayProperty()", (prop) ->
                mManager.getIntArrayProperty(prop, /* areaId= */ 0));
    }

    @Test
    public void testUserHal_getFloatProperty() {
        userHalPropertiesTest("getFloatProperty()", (prop) ->
                mManager.getFloatProperty(prop, /* areaId= */ 0));
    }

    @Test
    public void testUserHal_getPropertyList() {
        userHalPropertiesTest("getPropertyList()", (prop) -> {
            ArraySet<Integer> list = new ArraySet<>();
            list.add(prop);
            mManager.getPropertyList(list);
        });
    }

    @Test
    public void testUserHal_getCarPropertyConfig() {
        userHalPropertiesTest("getCarPropertyConfig()", (prop) ->
                mManager.getCarPropertyConfig(prop));
    }

    @Test
    public void testUserHal_getAreaId() {
        userHalPropertiesTest("getAreaId()", (prop) ->
                mManager.getAreaId(prop, /* areaId= */ 0));
    }

    @Test
    public void testUserHal_getReadPermission() {
        userHalPropertiesTest("getReadPermission()", (prop) ->
                mManager.getReadPermission(prop));
    }

    @Test
    public void testUserHal_getWritePermission() {
        userHalPropertiesTest("getWritePermission()", (prop) ->
                mManager.getWritePermission(prop));
    }

    @Test
    public void testUserHal_isPropertyAvailable() {
        userHalPropertiesTest("isPropertyAvailable()", (prop) ->
                mManager.isPropertyAvailable(prop, /* area= */ 0));
    }

    @Test
    public void testUserHal_setProperty() {
        userHalPropertiesTest("setProperty()", (prop) ->
                mManager.setProperty(Object.class, prop, /* areaId= */ 0, /* val= */ null));
    }

    @Test
    public void testUserHal_setBooleanProperty() {
        userHalPropertiesTest("setBooleanProperty()", (prop) ->
                mManager.setBooleanProperty(prop, /* areaId= */ 0, /* val= */ true));
    }

    @Test
    public void testUserHal_setFloatProperty() {
        userHalPropertiesTest("setFloatProperty()", (prop) ->
                mManager.setFloatProperty(prop, /* areaId= */ 0, /* val= */ 0.0F));
    }

    @Test
    public void testUserHal_setIntProperty() {
        userHalPropertiesTest("setIntProperty()", (prop) ->
                mManager.setIntProperty(prop, /* areaId= */ 0, /* val= */ 0));
    }

    private void userHalPropertiesTest(String method, Visitor<Integer> visitor) {
        List<String> failedProperties = new ArrayList<String>();
        for (int propertyId : USER_HAL_PROPERTIES) {
            try {
                visitor.visit(propertyId);
                failedProperties.add(propToString(propertyId));
            } catch (IllegalArgumentException e) {
                // expected
            }
        }
        if (!failedProperties.isEmpty()) {
            fail(method + " should not support these properties: " + failedProperties);
        }
    }

    @Override
    protected synchronized void configureMockedHal() {
        PropertyHandler handler = new PropertyHandler();
        addProperty(CUSTOM_SEAT_MIXED_PROP_ID_1, handler).setConfigArray(CONFIG_ARRAY_1)
                .addAreaConfig(DRIVER_SIDE_AREA_ID).addAreaConfig(PASSENGER_SIDE_AREA_ID);
        addProperty(CUSTOM_GLOBAL_MIXED_PROP_ID_2, handler).setConfigArray(CONFIG_ARRAY_2);
        addProperty(CUSTOM_GLOBAL_INT_ARRAY_PROP, handler);

        VehiclePropValue tempValue = new VehiclePropValue();
        tempValue.value.floatValues.add(INIT_TEMP_VALUE);
        tempValue.prop = VehiclePropertyIds.HVAC_TEMPERATURE_SET;
        addProperty(VehiclePropertyIds.HVAC_TEMPERATURE_SET, tempValue)
                .addAreaConfig(DRIVER_SIDE_AREA_ID).addAreaConfig(PASSENGER_SIDE_AREA_ID);
        addProperty(VehiclePropertyIds.INFO_VIN);

        addProperty(PROP_CAUSE_STATUS_CODE_ACCESS_DENIED, handler);
        addProperty(PROP_CAUSE_STATUS_CODE_TRY_AGAIN, handler);
        addProperty(PROP_CAUSE_STATUS_CODE_INTERNAL_ERROR, handler);
        addProperty(PROP_CAUSE_STATUS_CODE_INVALID_ARG, handler);
        addProperty(PROP_CAUSE_STATUS_CODE_NOT_AVAILABLE, handler);

        addProperty(CUSTOM_SEAT_INT_PROP_1, handler).addAreaConfig(DRIVER_SIDE_AREA_ID)
                                                        .addAreaConfig(PASSENGER_SIDE_AREA_ID);
        addProperty(CUSTOM_SEAT_INT_PROP_2, handler).addAreaConfig(DRIVER_SIDE_AREA_ID)
                                                        .addAreaConfig(PASSENGER_SIDE_AREA_ID);
    }

    private class PropertyHandler implements VehicleHalPropertyHandler {
        HashMap<Integer, VehiclePropValue> mMap = new HashMap<>();
        @Override
        public synchronized void onPropertySet(VehiclePropValue value) {
            // Simulate HalClient.set() behavior.
            int statusCode = mapPropertyToStatusCode(value.prop);
            if (statusCode == VehicleHalStatusCode.STATUS_INVALID_ARG) {
                throw new IllegalArgumentException();
            }

            if (statusCode != VehicleHalStatusCode.STATUS_OK) {
                throw new ServiceSpecificException(statusCode);
            }

            mMap.put(value.prop, value);
        }

        @Override
        public synchronized VehiclePropValue onPropertyGet(VehiclePropValue value) {
            // Simulate HalClient.get() behavior.
            int statusCode = mapPropertyToStatusCode(value.prop);
            if (statusCode == VehicleHalStatusCode.STATUS_INVALID_ARG) {
                throw new IllegalArgumentException();
            }

            if (statusCode != VehicleHalStatusCode.STATUS_OK) {
                throw new ServiceSpecificException(statusCode);
            }

            VehiclePropValue currentValue = mMap.get(value.prop);
            return currentValue != null ? currentValue : value;
        }

        @Override
        public synchronized void onPropertySubscribe(int property, float sampleRate) {
            Log.d(TAG, "onPropertySubscribe property "
                    + property + " sampleRate " + sampleRate);
        }

        @Override
        public synchronized void onPropertyUnsubscribe(int property) {
            Log.d(TAG, "onPropertyUnSubscribe property " + property);
        }
    }

    private static String propToString(int propertyId) {
        return VehiclePropertyIds.toString(propertyId) + " (" + propertyId + ")";
    }

    private static int mapPropertyToStatusCode(int propId) {
        switch (propId) {
            case PROP_CAUSE_STATUS_CODE_TRY_AGAIN:
                return VehicleHalStatusCode.STATUS_TRY_AGAIN;
            case PROP_CAUSE_STATUS_CODE_NOT_AVAILABLE:
                return VehicleHalStatusCode.STATUS_NOT_AVAILABLE;
            case PROP_CAUSE_STATUS_CODE_ACCESS_DENIED:
                return VehicleHalStatusCode.STATUS_ACCESS_DENIED;
            case PROP_CAUSE_STATUS_CODE_INVALID_ARG:
                return VehicleHalStatusCode.STATUS_INVALID_ARG;
            case PROP_CAUSE_STATUS_CODE_INTERNAL_ERROR:
                return VehicleHalStatusCode.STATUS_INTERNAL_ERROR;
            default:
                return VehicleHalStatusCode.STATUS_OK;
        }
    }

    private static class TestErrorCallback implements CarPropertyManager.CarPropertyEventCallback {

        private static final String CALLBACK_TAG = "ErrorEventTest";
        private boolean mReceivedErrorEventWithErrorCode = false;
        private boolean mReceivedErrorEventWithOutErrorCode = false;
        private int mErrorCode;
        private final CountDownLatch mEventsCountDownLatch = new CountDownLatch(1);
        private final CountDownLatch mRegisterCountDownLatch = new CountDownLatch(2);
        @Override
        public void onChangeEvent(CarPropertyValue value) {
            Log.d(CALLBACK_TAG, "onChangeEvent: " + value);
            mRegisterCountDownLatch.countDown();
        }

        @Override
        public void onErrorEvent(int propId, int zone) {
            mReceivedErrorEventWithOutErrorCode = true;
            Log.d(CALLBACK_TAG, "onErrorEvent, propId: " + propId + " zone: " + zone);
            mEventsCountDownLatch.countDown();
        }

        @Override
        public void onErrorEvent(int propId, int areaId, int errorCode) {
            mReceivedErrorEventWithErrorCode = true;
            mErrorCode = errorCode;
            Log.d(CALLBACK_TAG, "onErrorEvent, propId: " + propId + " areaId: " + areaId
                    + "errorCode: " + errorCode);
            mEventsCountDownLatch.countDown();
        }

        public void assertOnErrorEventCalled() throws InterruptedException {
            if (!mEventsCountDownLatch.await(CALLBACK_SHORT_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                throw new IllegalStateException("Callback is not called in "
                        + CALLBACK_SHORT_TIMEOUT_MS + " ms.");
            }
        }

        public void assertOnErrorEventNotCalled() throws InterruptedException {
            if (mEventsCountDownLatch.await(CALLBACK_SHORT_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                throw new IllegalStateException("Callback is called in " + CALLBACK_SHORT_TIMEOUT_MS
                        + " ms.");
            }
        }

        public void assertRegisterCompleted() throws InterruptedException {
            if (!mRegisterCountDownLatch.await(CALLBACK_SHORT_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                throw new IllegalStateException("Register failed in " + CALLBACK_SHORT_TIMEOUT_MS
                        + " ms.");
            }
        }
    }

    private class TestSequenceCallback implements CarPropertyManager.CarPropertyEventCallback {

        private ConcurrentHashMap<Integer, CarPropertyValue> mRecorder = new ConcurrentHashMap<>();
        private int mCounter = 0;
        private final CountDownLatch mEventsCountDownLatch;
        private final CountDownLatch mRegisterCountDownLatch = new CountDownLatch(2);
        @Override
        public void onChangeEvent(CarPropertyValue value) {
            Log.e(TAG, "onChanged get a event " + value);
            mRecorder.put(value.getPropertyId(), value);
            mRegisterCountDownLatch.countDown();
            // Skip initial events
            if (value.getTimestamp() != 0) {
                mCounter++;
                mEventsCountDownLatch.countDown();
            }
        }

        TestSequenceCallback(int expectedTimes) {
            mEventsCountDownLatch = new CountDownLatch(expectedTimes);
        }

        @Override
        public void onErrorEvent(int properId, int zone) {
            Log.e(TAG, "TestSequenceCallback get an onErrorEvent");
        }

        public CarPropertyValue getLastCarPropertyValue(int propId) {
            return mRecorder.get(propId);
        }

        public int getEventCounter() {
            return mCounter;
        }

        public void assertOnChangeEventCalled() throws InterruptedException {
            if (!mEventsCountDownLatch.await(CALLBACK_SHORT_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                throw new IllegalStateException("Callback is not called in "
                        + CALLBACK_SHORT_TIMEOUT_MS + " ms.");
            }
        }

        public void assertRegisterCompleted() throws InterruptedException {
            if (!mRegisterCountDownLatch.await(CALLBACK_SHORT_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                throw new IllegalStateException("Register failed in " + CALLBACK_SHORT_TIMEOUT_MS
                        + " ms.");
            }
        }
    }

}
