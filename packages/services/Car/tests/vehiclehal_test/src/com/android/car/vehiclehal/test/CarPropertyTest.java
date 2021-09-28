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
package com.android.car.vehiclehal.test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static java.lang.Integer.toHexString;

import android.car.Car;
import android.car.hardware.CarPropertyConfig;
import android.car.hardware.CarPropertyValue;
import android.car.hardware.property.CarPropertyManager;
import android.car.hardware.property.CarPropertyManager.CarPropertyEventCallback;
import android.car.hardware.property.VehicleVendorPermission;
import android.hardware.automotive.vehicle.V2_0.VehiclePropertyGroup;
import android.os.SystemClock;
import android.util.ArraySet;
import android.util.Log;

import androidx.test.filters.LargeTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.time.Duration;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * The test suite will execute end-to-end Car Property API test by generating VHAL property data
 * from default VHAL and verify those data on the fly. The test data is coming from assets/ folder
 * in the test APK and will be shared with VHAL to execute the test.
 */
@RunWith(AndroidJUnit4.class)
@LargeTest
public class CarPropertyTest extends E2eCarTestBase {

    private static final String TAG = Utils.concatTag(CarPropertyTest.class);

    // Test should be completed within 10 minutes as it only covers a finite set of properties
    private static final Duration TEST_TIME_OUT = Duration.ofMinutes(3);

    private static final String CAR_HVAC_TEST_JSON = "car_hvac_test.json";
    private static final String CAR_HVAC_TEST_SET_JSON = "car_hvac_test.json";
    private static final String CAR_INFO_TEST_JSON = "car_info_test.json";
    // kMixedTypePropertyForTest property ID
    private static final int MIXED_TYPE_PROPERTY = 0x21e01111;
    // kSetPropertyFromVehicleForTest
    private static final int SET_INT_FROM_VEHICLE = 0x21e01112;
    private static final int SET_FLOAT_FROM_VEHICLE = 0x21e01113;
    private static final int SET_BOOLEAN_FROM_VEHICLE = 0x21e01114;
    private static final int WAIT_FOR_CALLBACK = 200;

    // kMixedTypePropertyForTest default value
    private static final Object[] DEFAULT_VALUE = {"MIXED property", true, 2, 3, 4.5f};
    private static final String CAR_PROPERTY_TEST_JSON = "car_property_test.json";
    private static final int GEAR_PROPERTY_ID = 289408000;

    private static final Set<String> VENDOR_PERMISSIONS = new HashSet<>(Arrays.asList(
            Car.PERMISSION_VENDOR_EXTENSION,
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_WINDOW,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_WINDOW,
            // permissions for the property related with door
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_DOOR,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_DOOR,
            // permissions for the property related with seat
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_SEAT,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_SEAT,
            // permissions for the property related with mirror
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_MIRROR,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_MIRROR,

            // permissions for the property related with car's information
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_INFO,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_INFO,
            // permissions for the property related with car's engine
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_ENGINE,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_ENGINE,
            // permissions for the property related with car's HVAC
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_HVAC,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_HVAC,
            // permissions for the property related with car's light
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_LIGHT,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_LIGHT,

            // permissions reserved for other vendor permission
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_1,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_1,
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_2,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_2,
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_3,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_3,
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_4,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_4,
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_5,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_5,
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_6,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_6,
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_7,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_7,
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_8,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_8,
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_9,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_9,
            VehicleVendorPermission.PERMISSION_SET_CAR_VENDOR_CATEGORY_10,
            VehicleVendorPermission.PERMISSION_GET_CAR_VENDOR_CATEGORY_10
    ));
    private class CarPropertyEventReceiver implements CarPropertyEventCallback {

        private VhalEventVerifier mVerifier;
        private boolean mStartVerify = false;

        CarPropertyEventReceiver(VhalEventVerifier verifier) {
            mVerifier = verifier;
        }

        @Override
        public void onChangeEvent(CarPropertyValue carPropertyValue) {
            if (mStartVerify) {
                mVerifier.verify(carPropertyValue);
            }
        }

        @Override
        public void onErrorEvent(final int propertyId, final int zone) {
            Assert.fail("Error: propertyId=" + toHexString(propertyId) + " zone=" + zone);
        }

        // Start verifying events
        public void startVerifying() {
            mStartVerify = true;
        }
    }

    /**
     * This test will use {@link CarPropertyManager#setProperty(Class, int, int, Object)} to turn
     * on the HVAC_PROWER and then let Default VHAL to generate HVAC data and verify on-the-fly
     * in the test. It is simulating the HVAC actions coming from hard buttons in a car.
     * @throws Exception
     */
    @Test
    public void testHvacHardButtonOperations() throws Exception {

        Log.d(TAG, "Prepare HVAC test data");
        List<CarPropertyValue> expectedEvents = getExpectedEvents(CAR_HVAC_TEST_JSON);
        List<CarPropertyValue> expectedSetEvents = getExpectedEvents(CAR_HVAC_TEST_SET_JSON);

        CarPropertyManager propMgr = (CarPropertyManager) mCar.getCarManager(Car.PROPERTY_SERVICE);
        assertNotNull("CarPropertyManager is null", propMgr);

        // test set method from android side
        for (CarPropertyValue expectedEvent : expectedSetEvents) {
            Class valueClass = expectedEvent.getValue().getClass();
            propMgr.setProperty(valueClass,
                    expectedEvent.getPropertyId(),
                    expectedEvent.getAreaId(),
                    expectedEvent.getValue());
            Thread.sleep(WAIT_FOR_CALLBACK);
            CarPropertyValue receivedEvent = propMgr.getProperty(valueClass,
                    expectedEvent.getPropertyId(), expectedEvent.getAreaId());
            assertTrue("Mismatched events, expected: " + expectedEvent + ", received: "
                    + receivedEvent, Utils.areCarPropertyValuesEqual(expectedEvent, receivedEvent));
        }

        // test that set from vehicle side will trigger callback to android
        VhalEventVerifier verifier = new VhalEventVerifier(expectedEvents);
        ArraySet<Integer> props = new ArraySet<>();
        for (CarPropertyValue event : expectedEvents) {
                props.add(event.getPropertyId());
        }
        CarPropertyEventReceiver receiver =
                new CarPropertyEventReceiver(verifier);
        for (Integer prop : props) {
            propMgr.registerCallback(receiver, prop, 0);
        }
        Thread.sleep(WAIT_FOR_CALLBACK);
        receiver.startVerifying();
        injectEventFromVehicleSide(expectedEvents, propMgr);
        verifier.waitForEnd(TEST_TIME_OUT.toMillis());
        propMgr.unregisterCallback(receiver);

        assertTrue("Detected mismatched events: " + verifier.getResultString(),
                    verifier.getMismatchedEvents().isEmpty());
    }

    /**
     * Static properties' value should never be changed.
     * @throws Exception
     */
    @Test
    public void testStaticInfoOperations() throws Exception {
        Log.d(TAG, "Prepare static car information");

        List<CarPropertyValue> expectedEvents = getExpectedEvents(CAR_INFO_TEST_JSON);
        CarPropertyManager propMgr = (CarPropertyManager) mCar.getCarManager(Car.PROPERTY_SERVICE);
        assertNotNull("CarPropertyManager is null", propMgr);
        for (CarPropertyValue expectedEvent : expectedEvents) {
            CarPropertyValue actualEvent = propMgr.getProperty(
                    expectedEvent.getPropertyId(), expectedEvent.getAreaId());
            assertTrue(String.format(
                    "Mismatched car information data, actual: %s, expected: %s",
                    actualEvent, expectedEvent),
                    Utils.areCarPropertyValuesEqual(actualEvent, expectedEvent));
        }
    }

    /**
     * This test will test set/get on MIX type properties. It needs a vendor property in Google
     * Vehicle HAL. See kMixedTypePropertyForTest in google defaultConfig.h for details.
     * @throws Exception
     */
    @Test
    public void testMixedTypeProperty() throws Exception {
        CarPropertyManager propertyManager =
                (CarPropertyManager) mCar.getCarManager(Car.PROPERTY_SERVICE);
        ArraySet<Integer> propConfigSet = new ArraySet<>();
        propConfigSet.add(MIXED_TYPE_PROPERTY);

        List<CarPropertyConfig> configs = propertyManager.getPropertyList(propConfigSet);

        // use google HAL in the test
        assertNotEquals("Can not find MIXED type properties in HAL",
                0, configs.size());

        // test CarPropertyConfig
        CarPropertyConfig<?> cfg = configs.get(0);
        List<Integer> configArrayExpected = Arrays.asList(1, 1, 0, 2, 0, 0, 1, 0, 0);
        assertArrayEquals(configArrayExpected.toArray(), cfg.getConfigArray().toArray());

        // test SET/GET methods
        CarPropertyValue<Object[]> propertyValue = propertyManager.getProperty(Object[].class,
                MIXED_TYPE_PROPERTY, 0);
        assertArrayEquals(DEFAULT_VALUE, propertyValue.getValue());

        Object[] expectedValue = {"MIXED property", false, 5, 4, 3.2f};
        propertyManager.setProperty(Object[].class, MIXED_TYPE_PROPERTY, 0, expectedValue);
        // Wait for VHAL
        Thread.sleep(WAIT_FOR_CALLBACK);
        CarPropertyValue<Object[]> result = propertyManager.getProperty(Object[].class,
                MIXED_TYPE_PROPERTY, 0);
        assertArrayEquals(expectedValue, result.getValue());
    }

    /**
     * This test will test the case: vehicle events comes to android out of order.
     * See the events in car_property_test.json.
     * @throws Exception
     */
    @Test
    public void testPropertyEventOutOfOrder() throws Exception {
        CarPropertyManager propMgr = (CarPropertyManager) mCar.getCarManager(Car.PROPERTY_SERVICE);
        assertNotNull("CarPropertyManager is null", propMgr);
        List<CarPropertyValue> expectedEvents = getExpectedEvents(CAR_PROPERTY_TEST_JSON);

        GearEventTestCallback cb = new GearEventTestCallback();
        propMgr.registerCallback(cb, GEAR_PROPERTY_ID, CarPropertyManager.SENSOR_RATE_ONCHANGE);
        injectEventFromVehicleSide(expectedEvents, propMgr);
        // check VHAL ignored the last event in car_property_test, because it is out of order.
        int currentGear = propMgr.getIntProperty(GEAR_PROPERTY_ID, 0);
        assertEquals(16, currentGear);
    }

    /**
     * Check only vendor properties have vendor permissions.
     */
    @Test
    public void checkPropertyPermission() {
        CarPropertyManager propMgr = (CarPropertyManager) mCar.getCarManager(Car.PROPERTY_SERVICE);
        List<CarPropertyConfig> configs = propMgr.getPropertyList();
        for (CarPropertyConfig cfg : configs) {
            String readPermission = propMgr.getReadPermission(cfg.getPropertyId());
            String writePermission = propMgr.getWritePermission(cfg.getPropertyId());
            if ((cfg.getPropertyId() & VehiclePropertyGroup.MASK) == VehiclePropertyGroup.VENDOR) {
                Assert.assertTrue(readPermission == null
                        || VENDOR_PERMISSIONS.contains(readPermission));
                Assert.assertTrue(writePermission == null
                        || VENDOR_PERMISSIONS.contains(writePermission));
            } else {
                Assert.assertTrue(readPermission == null
                        || !VENDOR_PERMISSIONS.contains(readPermission));
                Assert.assertTrue(writePermission == null
                        || !VENDOR_PERMISSIONS.contains(writePermission));
            }
        }
    }

    private class GearEventTestCallback implements CarPropertyEventCallback {
        private long mTimestamp = 0L;

        @Override
        public void onChangeEvent(CarPropertyValue carPropertyValue) {
            if (carPropertyValue.getPropertyId() != GEAR_PROPERTY_ID) {
                return;
            }
            if (carPropertyValue.getStatus() == CarPropertyValue.STATUS_AVAILABLE) {
                Assert.assertTrue("Received events out of oder",
                        mTimestamp <= carPropertyValue.getTimestamp());
                mTimestamp = carPropertyValue.getTimestamp();
            }
        }

        @Override
        public void onErrorEvent(final int propertyId, final int zone) {
            Assert.fail("Error: propertyId: x0" + toHexString(propertyId) + " areaId: " + zone);
        }
    }

    /**
     * Inject events from vehicle side. It change the value of property even the property is a
     * read_only property such as GEAR_SELECTION. It only works with Google VHAL.
     */
    private void injectEventFromVehicleSide(List<CarPropertyValue> expectedEvents,
            CarPropertyManager propMgr) {
        for (CarPropertyValue propertyValue : expectedEvents) {
            Object[] values = new Object[3];
            int propId;
            // The order of values is matter
            if (propertyValue.getValue() instanceof Integer) {
                propId = SET_INT_FROM_VEHICLE;
                values[0] = propertyValue.getPropertyId();
                values[1] = propertyValue.getValue();
                values[2] = propertyValue.getTimestamp() + SystemClock.elapsedRealtimeNanos();
            } else if (propertyValue.getValue() instanceof Float) {
                values[0] = propertyValue.getPropertyId();
                values[1] = propertyValue.getTimestamp() + SystemClock.elapsedRealtimeNanos();
                values[2] = propertyValue.getValue();
                propId = SET_FLOAT_FROM_VEHICLE;
            } else if (propertyValue.getValue() instanceof Boolean) {
                propId = SET_BOOLEAN_FROM_VEHICLE;
                values[1] = propertyValue.getPropertyId();
                values[0] = propertyValue.getValue();
                values[2] = propertyValue.getTimestamp() + SystemClock.elapsedRealtimeNanos();
            } else {
                throw new IllegalArgumentException(
                        "Unexpected property type for property " + propertyValue.getPropertyId());
            }
            propMgr.setProperty(Object[].class, propId, propertyValue.getAreaId(), values);
        }
    }
}
