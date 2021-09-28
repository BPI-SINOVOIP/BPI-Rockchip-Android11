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
package android.car.cts;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.testng.Assert.assertThrows;

import android.car.Car;
import android.car.VehicleAreaSeat;
import android.car.VehicleAreaType;
import android.car.VehiclePropertyIds;
import android.car.hardware.CarPropertyConfig;
import android.car.hardware.CarPropertyValue;
import android.car.hardware.property.CarPropertyManager;
import android.car.hardware.property.CarPropertyManager.CarPropertyEventCallback;
import android.platform.test.annotations.RequiresDevice;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.ArraySet;
import android.util.SparseArray;

import androidx.annotation.GuardedBy;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.CddTest;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@SmallTest
@RequiresDevice
@RunWith(AndroidJUnit4.class)
public class CarPropertyManagerTest extends CarApiTestBase {

    private static final String TAG = CarPropertyManagerTest.class.getSimpleName();
    private static final long WAIT_CALLBACK = 1500L;
    private static final int NO_EVENTS = 0;
    private static final int ONCHANGE_RATE_EVENT_COUNTER = 1;
    private static final int UI_RATE_EVENT_COUNTER = 5;
    private static final int FAST_OR_FASTEST_EVENT_COUNTER = 10;
    private CarPropertyManager mCarPropertyManager;
    /** contains property Ids for the properties required by CDD*/
    private ArraySet<Integer> mPropertyIds = new ArraySet<>();


    private static class CarPropertyEventCounter implements CarPropertyEventCallback {
        private final Object mLock = new Object();
        private int mCounter = FAST_OR_FASTEST_EVENT_COUNTER;
        private CountDownLatch mCountDownLatch = new CountDownLatch(mCounter);

        @GuardedBy("mLock")
        private SparseArray<Integer> mEventCounter = new SparseArray<>();

        @GuardedBy("mLock")
        private SparseArray<Integer> mErrorCounter = new SparseArray<>();

        public int receivedEvent(int propId) {
            int val;
            synchronized (mLock) {
                val = mEventCounter.get(propId, 0);
            }
            return val;
        }

        public int receivedError(int propId) {
            int val;
            synchronized (mLock) {
                val = mErrorCounter.get(propId, 0);
            }
            return val;
        }

        @Override
        public void onChangeEvent(CarPropertyValue value) {
            synchronized (mLock) {
                int val = mEventCounter.get(value.getPropertyId(), 0) + 1;
                mEventCounter.put(value.getPropertyId(), val);
            }
            mCountDownLatch.countDown();
        }

        @Override
        public void onErrorEvent(int propId, int zone) {
            synchronized (mLock) {
                int val = mErrorCounter.get(propId, 0) + 1;
                mErrorCounter.put(propId, val);
            }
        }

        public void resetCountDownLatch(int counter) {
            mCountDownLatch = new CountDownLatch(counter);
            mCounter = counter;
        }

        public void assertOnChangeEventCalled() throws InterruptedException {
            if (!mCountDownLatch.await(WAIT_CALLBACK, TimeUnit.MILLISECONDS)) {
                throw new IllegalStateException("Callback is not called:" + mCounter + "times in "
                        + WAIT_CALLBACK + " ms.");
            }
        }

        public void assertOnChangeEventNotCalled() throws InterruptedException {
            // Once get an event, fail the test.
            mCountDownLatch = new CountDownLatch(1);
            if (mCountDownLatch.await(WAIT_CALLBACK, TimeUnit.MILLISECONDS)) {
                throw new IllegalStateException("Callback is called in "
                        + WAIT_CALLBACK + " ms.");
            }
        }

    }

    @Before
    public void setUp() throws Exception {
        super.setUp();
        mCarPropertyManager = (CarPropertyManager) getCar().getCarManager(Car.PROPERTY_SERVICE);
        mPropertyIds.add(VehiclePropertyIds.PERF_VEHICLE_SPEED);
        mPropertyIds.add(VehiclePropertyIds.GEAR_SELECTION);
        mPropertyIds.add(VehiclePropertyIds.NIGHT_MODE);
        mPropertyIds.add(VehiclePropertyIds.PARKING_BRAKE_ON);
    }

    /**
     * Test for {@link CarPropertyManager#getPropertyList()}
     */
    @Test
    public void testGetPropertyList() {
        List<CarPropertyConfig> allConfigs = mCarPropertyManager.getPropertyList();
        assertThat(allConfigs).isNotNull();
    }

    /**
     * Test for {@link CarPropertyManager#getPropertyList(ArraySet)}
     */
    @Test
    public void testGetPropertyListWithArraySet() {
        List<CarPropertyConfig> requiredConfigs = mCarPropertyManager.getPropertyList(mPropertyIds);
        // Vehicles need to implement all of those properties
        assertThat(requiredConfigs.size()).isEqualTo(mPropertyIds.size());
    }

    /**
     * Test for {@link CarPropertyManager#getCarPropertyConfig(int)}
     */
    @Test
    public void testGetPropertyConfig() {
        List<CarPropertyConfig> allConfigs = mCarPropertyManager.getPropertyList();
        for (CarPropertyConfig cfg : allConfigs) {
            assertThat(mCarPropertyManager.getCarPropertyConfig(cfg.getPropertyId())).isNotNull();
        }
    }

    /**
     * Test for {@link CarPropertyManager#getAreaId(int, int)}
     */
    @Test
    public void testGetAreaId() {
        // For global properties, getAreaId should always return 0.
        List<CarPropertyConfig> allConfigs = mCarPropertyManager.getPropertyList();
        for (CarPropertyConfig cfg : allConfigs) {
            if (cfg.isGlobalProperty()) {
                assertThat(mCarPropertyManager.getAreaId(cfg.getPropertyId(),
                        VehicleAreaSeat.SEAT_ROW_1_LEFT))
                        .isEqualTo(VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL);
            } else {
                int[] areaIds = cfg.getAreaIds();
                // Because areaId in propConfig must not be overlapped with each other.
                // The result should be itself.
                for (int areaIdInConfig : areaIds) {
                    int areaIdByCarPropertyManager =
                            mCarPropertyManager.getAreaId(cfg.getPropertyId(), areaIdInConfig);
                    assertThat(areaIdByCarPropertyManager).isEqualTo(areaIdInConfig);
                }
            }
        }
    }

    @CddTest(requirement="2.5.1")
    @Test
    public void testMustSupportGearSelection() throws Exception {
        assertWithMessage("Must support GEAR_SELECTION")
                .that(mCarPropertyManager.getCarPropertyConfig(VehiclePropertyIds.GEAR_SELECTION))
                .isNotNull();
    }

    @CddTest(requirement="2.5.1")
    @Test
    public void testMustSupportNightMode() {
        assertWithMessage("Must support NIGHT_MODE")
                .that(mCarPropertyManager.getCarPropertyConfig(VehiclePropertyIds.NIGHT_MODE))
                .isNotNull();
    }

    @CddTest(requirement="2.5.1")
    @Test
    public void testMustSupportPerfVehicleSpeed() throws Exception {
        assertWithMessage("Must support PERF_VEHICLE_SPEED")
                .that(mCarPropertyManager.getCarPropertyConfig(
                        VehiclePropertyIds.PERF_VEHICLE_SPEED)).isNotNull();
    }

    @CddTest(requirement = "2.5.1")
    @Test
    public void testMustSupportParkingBrakeOn() throws Exception {
        assertWithMessage("Must support PARKING_BRAKE_ON")
                .that(mCarPropertyManager.getCarPropertyConfig(VehiclePropertyIds.PARKING_BRAKE_ON))
                .isNotNull();

    }

    @SuppressWarnings("unchecked")
    @Test
    public void testGetProperty() {
        List<CarPropertyConfig> configs = mCarPropertyManager.getPropertyList(mPropertyIds);
        for (CarPropertyConfig cfg : configs) {
            if (cfg.getAccess() == CarPropertyConfig.VEHICLE_PROPERTY_ACCESS_READ) {
                int[] areaIds = getAreaIdsHelper(cfg);
                int propId = cfg.getPropertyId();
                // no guarantee if we can get values, just call and check if it throws exception.
                if (cfg.getPropertyType() == Boolean.class) {
                    for (int areaId : areaIds) {
                        mCarPropertyManager.getBooleanProperty(propId, areaId);
                    }
                } else if (cfg.getPropertyType() == Integer.class) {
                    for (int areaId : areaIds) {
                        mCarPropertyManager.getIntProperty(propId, areaId);
                    }
                } else if (cfg.getPropertyType() == Float.class) {
                    for (int areaId : areaIds) {
                        mCarPropertyManager.getFloatProperty(propId, areaId);
                    }
                } else if (cfg.getPropertyType() == Integer[].class) {
                    for (int areId : areaIds) {
                        mCarPropertyManager.getIntArrayProperty(propId, areId);
                    }
                } else {
                    for (int areaId : areaIds) {
                        mCarPropertyManager.getProperty(
                                cfg.getPropertyType(), propId, areaId);;
                    }
                }
            }
        }
    }

    @Test
    public void testGetIntArrayProperty() {
        List<CarPropertyConfig> allConfigs = mCarPropertyManager.getPropertyList();
        for (CarPropertyConfig cfg : allConfigs) {
            if (cfg.getAccess() == CarPropertyConfig.VEHICLE_PROPERTY_ACCESS_NONE
                    || cfg.getAccess() == CarPropertyConfig.VEHICLE_PROPERTY_ACCESS_WRITE
                    || cfg.getPropertyType() != Integer[].class) {
                // skip the test if the property is not readable or not an int array type property.
                continue;
            }
            switch (cfg.getPropertyId()) {
                case VehiclePropertyIds.INFO_FUEL_TYPE:
                    int[] fuelTypes = mCarPropertyManager.getIntArrayProperty(cfg.getPropertyId(),
                            VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL);
                    verifyEnumsRange(EXPECTED_FUEL_TYPES, fuelTypes);
                    break;
                case VehiclePropertyIds.INFO_MULTI_EV_PORT_LOCATIONS:
                    int[] evPortLocations = mCarPropertyManager.getIntArrayProperty(
                            cfg.getPropertyId(),VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL);
                    verifyEnumsRange(EXPECTED_PORT_LOCATIONS, evPortLocations);
                    break;
                default:
                    int[] areaIds = getAreaIdsHelper(cfg);
                    for(int areaId : areaIds) {
                        mCarPropertyManager.getIntArrayProperty(cfg.getPropertyId(), areaId);
                    }
            }
        }
    }

    private void verifyEnumsRange(List<Integer> expectedResults, int[] results) {
        assertThat(results).isNotNull();
        // If the property is not implemented in cars, getIntArrayProperty returns an empty array.
        if (results.length == 0) {
            return;
        }
        for (int result : results) {
            assertThat(result).isIn(expectedResults);
        }
    }

    @Test
    public void testIsPropertyAvailable() {
        List<CarPropertyConfig> configs = mCarPropertyManager.getPropertyList(mPropertyIds);

        for (CarPropertyConfig cfg : configs) {
            int[] areaIds = getAreaIdsHelper(cfg);
            for (int areaId : areaIds) {
                assertThat(mCarPropertyManager.isPropertyAvailable(cfg.getPropertyId(), areaId))
                        .isTrue();
            }
        }
    }

    @Test
    public void testSetProperty() {
        List<CarPropertyConfig> configs = mCarPropertyManager.getPropertyList();
        for (CarPropertyConfig cfg : configs) {
            if (cfg.getAccess() == CarPropertyConfig.VEHICLE_PROPERTY_ACCESS_READ_WRITE
                    && cfg.getPropertyType() == Boolean.class) {
                // In R, there is no property which is writable for third-party apps.
                for (int areaId : getAreaIdsHelper(cfg)) {
                    assertThrows(SecurityException.class,
                            () -> mCarPropertyManager.setBooleanProperty(
                                    cfg.getPropertyId(), areaId,true));
                }
            }
        }
    }

    @Test
    public void testRegisterCallback() throws Exception {
        //Test on registering a invalid property
        int invalidPropertyId = -1;
        boolean isRegistered = mCarPropertyManager.registerCallback(
            new CarPropertyEventCounter(), invalidPropertyId, 0);
        assertThat(isRegistered).isFalse();

        // Test for continuous properties
        int vehicleSpeed = VehiclePropertyIds.PERF_VEHICLE_SPEED;
        CarPropertyEventCounter speedListenerUI = new CarPropertyEventCounter();
        CarPropertyEventCounter speedListenerFast = new CarPropertyEventCounter();

        assertThat(speedListenerUI.receivedEvent(vehicleSpeed)).isEqualTo(NO_EVENTS);
        assertThat(speedListenerUI.receivedError(vehicleSpeed)).isEqualTo(NO_EVENTS);
        assertThat(speedListenerFast.receivedEvent(vehicleSpeed)).isEqualTo(NO_EVENTS);
        assertThat(speedListenerFast.receivedError(vehicleSpeed)).isEqualTo(NO_EVENTS);

        mCarPropertyManager.registerCallback(speedListenerUI, vehicleSpeed,
                CarPropertyManager.SENSOR_RATE_UI);
        mCarPropertyManager.registerCallback(speedListenerFast, vehicleSpeed,
                CarPropertyManager.SENSOR_RATE_FASTEST);
        speedListenerUI.resetCountDownLatch(UI_RATE_EVENT_COUNTER);
        speedListenerUI.assertOnChangeEventCalled();
        assertThat(speedListenerUI.receivedEvent(vehicleSpeed)).isGreaterThan(NO_EVENTS);
        assertThat(speedListenerFast.receivedEvent(vehicleSpeed)).isGreaterThan(
                speedListenerUI.receivedEvent(vehicleSpeed));

        mCarPropertyManager.unregisterCallback(speedListenerFast);
        mCarPropertyManager.unregisterCallback(speedListenerUI);

        // Test for on_change properties
        int nightMode = VehiclePropertyIds.NIGHT_MODE;
        CarPropertyEventCounter nightModeListener = new CarPropertyEventCounter();
        nightModeListener.resetCountDownLatch(ONCHANGE_RATE_EVENT_COUNTER);
        mCarPropertyManager.registerCallback(nightModeListener, nightMode, 0);
        nightModeListener.assertOnChangeEventCalled();
        assertThat(nightModeListener.receivedEvent(nightMode)).isEqualTo(1);
        mCarPropertyManager.unregisterCallback(nightModeListener);

    }

    @Test
    public void testUnregisterCallback() throws Exception {

        int vehicleSpeed = VehiclePropertyIds.PERF_VEHICLE_SPEED;
        CarPropertyEventCounter speedListenerNormal = new CarPropertyEventCounter();
        CarPropertyEventCounter speedListenerUI = new CarPropertyEventCounter();

        mCarPropertyManager.registerCallback(speedListenerNormal, vehicleSpeed,
                CarPropertyManager.SENSOR_RATE_NORMAL);

        // test on unregistering a callback that was never registered
        try {
            mCarPropertyManager.unregisterCallback(speedListenerUI);
        } catch (Exception e) {
            Assert.fail();
        }

        mCarPropertyManager.registerCallback(speedListenerUI, vehicleSpeed,
                CarPropertyManager.SENSOR_RATE_UI);
        speedListenerUI.resetCountDownLatch(UI_RATE_EVENT_COUNTER);
        speedListenerUI.assertOnChangeEventCalled();
        mCarPropertyManager.unregisterCallback(speedListenerNormal, vehicleSpeed);

        int currentEventNormal = speedListenerNormal.receivedEvent(vehicleSpeed);
        int currentEventUI = speedListenerUI.receivedEvent(vehicleSpeed);
        speedListenerNormal.assertOnChangeEventNotCalled();

        assertThat(speedListenerNormal.receivedEvent(vehicleSpeed)).isEqualTo(currentEventNormal);
        assertThat(speedListenerUI.receivedEvent(vehicleSpeed)).isNotEqualTo(currentEventUI);

        mCarPropertyManager.unregisterCallback(speedListenerUI);
        speedListenerUI.assertOnChangeEventNotCalled();

        currentEventUI = speedListenerUI.receivedEvent(vehicleSpeed);
        assertThat(speedListenerUI.receivedEvent(vehicleSpeed)).isEqualTo(currentEventUI);
    }

    @Test
    public void testUnregisterWithPropertyId() throws Exception {
        // Ignores the test if wheel_tick property does not exist in the car.
        Assume.assumeTrue("WheelTick is not available, skip unregisterCallback test",
                mCarPropertyManager.isPropertyAvailable(
                        VehiclePropertyIds.WHEEL_TICK, VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL));

        CarPropertyEventCounter speedAndWheelTicksListener = new CarPropertyEventCounter();
        mCarPropertyManager.registerCallback(speedAndWheelTicksListener,
                VehiclePropertyIds.PERF_VEHICLE_SPEED, CarPropertyManager.SENSOR_RATE_FASTEST);
        mCarPropertyManager.registerCallback(speedAndWheelTicksListener,
                VehiclePropertyIds.WHEEL_TICK, CarPropertyManager.SENSOR_RATE_FASTEST);
        speedAndWheelTicksListener.resetCountDownLatch(FAST_OR_FASTEST_EVENT_COUNTER);
        speedAndWheelTicksListener.assertOnChangeEventCalled();

        mCarPropertyManager.unregisterCallback(speedAndWheelTicksListener,
                VehiclePropertyIds.PERF_VEHICLE_SPEED);
        speedAndWheelTicksListener.resetCountDownLatch(FAST_OR_FASTEST_EVENT_COUNTER);
        speedAndWheelTicksListener.assertOnChangeEventCalled();
        int currentSpeedEvents = speedAndWheelTicksListener.receivedEvent(
                VehiclePropertyIds.PERF_VEHICLE_SPEED);
        int currentWheelTickEvents = speedAndWheelTicksListener.receivedEvent(
                VehiclePropertyIds.WHEEL_TICK);

        speedAndWheelTicksListener.resetCountDownLatch(FAST_OR_FASTEST_EVENT_COUNTER);
        speedAndWheelTicksListener.assertOnChangeEventCalled();
        int speedEventsAfterUnregister = speedAndWheelTicksListener.receivedEvent(
                VehiclePropertyIds.PERF_VEHICLE_SPEED);
        int wheelTicksEventsAfterUnregister = speedAndWheelTicksListener.receivedEvent(
                VehiclePropertyIds.WHEEL_TICK);

        assertThat(currentSpeedEvents).isEqualTo(speedEventsAfterUnregister);
        assertThat(wheelTicksEventsAfterUnregister).isGreaterThan(currentWheelTickEvents);
    }


    // Returns {0} if the property is global property, otherwise query areaId for CarPropertyConfig
    private int[] getAreaIdsHelper(CarPropertyConfig config) {
        if (config.isGlobalProperty()) {
            int[] areaIds = {0};
            return areaIds;
        } else {
            return config.getAreaIds();
        }
    }
}
