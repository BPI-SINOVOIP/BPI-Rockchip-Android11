/*
 * Copyright (C) 2016 The Android Open Source Project
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

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.fail;

import android.car.Car;
import android.car.hardware.CarPropertyConfig;
import android.car.hardware.hvac.CarHvacManager;
import android.hardware.automotive.vehicle.V2_0.VehicleHvacFanDirection;
import android.test.suitebuilder.annotation.MediumTest;
import android.util.Log;

import org.junit.Before;
import org.junit.Test;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

@MediumTest
public class CarHvacManagerTest extends CarApiTestBase {
    private static final String TAG = CarHvacManagerTest.class.getSimpleName();

    private CarHvacManager mHvacManager;

    @Before
    public void setUp() throws Exception {
        mHvacManager = (CarHvacManager) getCar().getCarManager(Car.HVAC_SERVICE);
        assertThat(mHvacManager).isNotNull();
    }

    @Test
    public void testAllHvacProperties() throws Exception {
        List<CarPropertyConfig> properties = mHvacManager.getPropertyList();
        Set<Class> supportedTypes = new HashSet<>(Arrays.asList(
                new Class[] { Integer.class, Float.class, Boolean.class, Integer[].class }));

        for (CarPropertyConfig property : properties) {
            if (supportedTypes.contains(property.getPropertyType())) {
                assertTypeAndZone(property);
            } else {
                fail("Type is not supported for " + property);
            }
        }
    }

    @Test
    public void testHvacPosition() {
        assertThat(VehicleHvacFanDirection.FACE).isEqualTo(CarHvacManager.FAN_DIRECTION_FACE);
        assertThat(VehicleHvacFanDirection.FLOOR).isEqualTo(CarHvacManager.FAN_DIRECTION_FLOOR);
        assertThat(VehicleHvacFanDirection.DEFROST).isEqualTo(CarHvacManager.FAN_DIRECTION_DEFROST);
    }

    private void assertTypeAndZone(CarPropertyConfig property) {
        switch (property.getPropertyId()) {
            case CarHvacManager.ID_MIRROR_DEFROSTER_ON: // non-zoned bool
                checkTypeAndGlobal(Boolean.class, true, property);
                break;
            case CarHvacManager.ID_STEERING_WHEEL_HEAT: // non-zoned int
            case CarHvacManager.ID_TEMPERATURE_DISPLAY_UNITS:
                checkTypeAndGlobal(Integer.class, true, property);
                checkIntMinMax(property);
                break;
            case CarHvacManager.ID_OUTSIDE_AIR_TEMP:
                checkTypeAndGlobal(Float.class, true, property);
                break;
            case CarHvacManager.ID_ZONED_TEMP_SETPOINT: // zoned float
            case CarHvacManager.ID_ZONED_TEMP_ACTUAL:
                checkTypeAndGlobal(Float.class, false, property);
                checkFloatMinMax(property);
                break;
            case CarHvacManager.ID_ZONED_FAN_SPEED_SETPOINT: // zoned int
            case CarHvacManager.ID_ZONED_FAN_SPEED_RPM:
            case CarHvacManager.ID_ZONED_SEAT_TEMP:
                checkTypeAndGlobal(Integer.class, false, property);
                checkIntMinMax(property);
                break;
            case CarHvacManager.ID_ZONED_FAN_DIRECTION:
                checkTypeAndGlobal(Integer.class, false, property);
                break;
            case CarHvacManager.ID_ZONED_FAN_DIRECTION_AVAILABLE:
                checkTypeAndGlobal(Integer[].class, false, property);
                break;
            case CarHvacManager.ID_ZONED_AC_ON: // zoned boolean
            case CarHvacManager.ID_ZONED_AUTOMATIC_MODE_ON:
            case CarHvacManager.ID_ZONED_AIR_RECIRCULATION_ON:
            case CarHvacManager.ID_ZONED_MAX_AC_ON:
            case CarHvacManager.ID_ZONED_DUAL_ZONE_ON:
            case CarHvacManager.ID_ZONED_MAX_DEFROST_ON:
            case CarHvacManager.ID_ZONED_HVAC_POWER_ON:
            case CarHvacManager.ID_WINDOW_DEFROSTER_ON:
                checkTypeAndGlobal(Boolean.class, false, property);
                break;
        }
    }

    private void checkTypeAndGlobal(Class<?> clazz, boolean global,
            CarPropertyConfig<Integer> property) {
        assertWithMessage("Wrong type, expecting %s type for id %s", clazz,
                property.getPropertyId()).that(property.getPropertyType()).isEqualTo(clazz);
        assertWithMessage(
                "Wrong zone, should %s be global for id:%s, area type: %s" + property.getAreaType(),
                property.getPropertyId(), property.getAreaType(), (global ? "" : "not "))
                        .that(property.isGlobalProperty()).isEqualTo(global);
    }

    private void checkIntMinMax(CarPropertyConfig<Integer> property) {
        Log.i(TAG, "checkIntMinMax property:" + property);
        if (!property.isGlobalProperty()) {
            int[] areaIds = property.getAreaIds();
            assertThat(areaIds.length).isGreaterThan(0);
            assertThat(property.getAreaCount()).isEqualTo(areaIds.length);

            for (int areaId : areaIds) {
                assertThat(property.hasArea(areaId)).isTrue();
                int min = property.getMinValue(areaId) == null ? 0 : property.getMinValue(areaId);
                int max = property.getMaxValue(areaId) == null ? 0 : property.getMaxValue(areaId);
                assertThat(min).isAtMost(max);
            }
        } else {
            int min = property.getMinValue() == null ? 0 : property.getMinValue();
            int max = property.getMaxValue() == null ? 0 : property.getMinValue();
            assertThat(min).isAtMost(max);
            for (int i = 0; i < 32; i++) {
                assertThat(property.hasArea(0x1 << i)).isFalse();
                assertThat(property.getMinValue(0x1 << i)).isNull();
                assertThat(property.getMaxValue(0x1 << i)).isNull();
            }
        }
    }

    private void checkFloatMinMax(CarPropertyConfig<Float> property) {
        Log.i(TAG, "checkFloatMinMax property:" + property);
        if (!property.isGlobalProperty()) {
            int[] areaIds = property.getAreaIds();
            assertThat(areaIds.length).isGreaterThan(0);
            assertThat(property.getAreaCount()).isEqualTo(areaIds.length);

            for (int areaId : areaIds) {
                assertThat(property.hasArea(areaId)).isTrue();
                float min =
                        property.getMinValue(areaId) == null ? 0f : property.getMinValue(areaId);
                float max =
                        property.getMaxValue(areaId) == null ? 0f : property.getMinValue(areaId);
                assertThat(min).isAtMost(max);
            }
        } else {
            float min = property.getMinValue() == null ? 0f : property.getMinValue();
            float max = property.getMaxValue() == null ? 0f : property.getMinValue();
            assertThat(min).isAtMost(max);
            for (int i = 0; i < 32; i++) {
                assertThat(property.hasArea(0x1 << i)).isFalse();
                assertThat(property.getMinValue(0x1 << i)).isNull();
                assertThat(property.getMaxValue(0x1 << i)).isNull();
            }
        }
    }
}
