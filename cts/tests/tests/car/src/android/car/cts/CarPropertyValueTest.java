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
 * limitations under the License
 */
package android.car.cts;

import android.car.Car;
import android.car.VehicleAreaType;
import android.car.hardware.CarPropertyConfig;
import android.car.hardware.CarPropertyValue;
import android.car.hardware.property.CarPropertyManager;

import android.util.SparseArray;
import androidx.test.runner.AndroidJUnit4;
import static com.google.common.truth.Truth.assertThat;

import android.platform.test.annotations.RequiresDevice;
import android.test.suitebuilder.annotation.SmallTest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.Test.None;
import org.junit.runner.RunWith;

@SmallTest
@RequiresDevice
@RunWith(AndroidJUnit4.class)
public class CarPropertyValueTest extends CarApiTestBase {
    private CarPropertyManager mCarPropertyManager;
    private List<CarPropertyValue> mCarPropertyValues = new ArrayList<>();
    private SparseArray<CarPropertyConfig> mPropIdToConfig = new SparseArray<>();

    @Before
    public void setUp() throws Exception {
        super.setUp();
        mCarPropertyManager = (CarPropertyManager) getCar().getCarManager(Car.PROPERTY_SERVICE);
        getCarPropertyValues();
    }

    @Test
    public void testGetPropertyId() {
        for (CarPropertyValue propertyValue : mCarPropertyValues) {
            int propId = propertyValue.getPropertyId();
            CarPropertyConfig cfg = mPropIdToConfig.get(propId);
            Assert.assertNotNull(cfg);
        }
    }

    @Test
    public void testGetPropertyAreaId() {
        for (CarPropertyValue propertyValue : mCarPropertyValues) {
            int areaId = propertyValue.getAreaId();
            CarPropertyConfig cfg = mPropIdToConfig.get(propertyValue.getPropertyId());
            if (cfg.isGlobalProperty()) {
                Assert.assertEquals(areaId, VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL);
            } else {
                List<Integer> expectedAreaIds = new ArrayList<>();
                for (int area : cfg.getAreaIds()) {
                    expectedAreaIds.add(area);
                }
                assertThat(expectedAreaIds).contains(areaId);
            }
        }
    }

    @Test(expected = None.class /* no exception expected*/)
    public void testGetPropertyTimestamp() {
        for (CarPropertyValue propertyValue : mCarPropertyValues) {
            propertyValue.getTimestamp();
        }
    }

    @Test
    public void testGetPropertyStatus() {
        List<Integer> expectedStatus = Arrays.asList(
                CarPropertyValue.STATUS_AVAILABLE,
                CarPropertyValue.STATUS_ERROR,
                CarPropertyValue.STATUS_UNAVAILABLE);
        for (CarPropertyValue propertyValue : mCarPropertyValues) {
            int status = propertyValue.getStatus();
            assertThat(expectedStatus).contains(status);
        }
    }

    @Test
    public void testGetValue() {
        for (CarPropertyValue propertyValue : mCarPropertyValues) {
            Assert.assertNotNull(propertyValue.getValue());
        }
    }

    private void getCarPropertyValues() {
        List<CarPropertyConfig> configs = mCarPropertyManager.getPropertyList();
        for (CarPropertyConfig cfg : configs) {
            mPropIdToConfig.put(cfg.getPropertyId(), cfg);
            if (cfg.getAccess() == CarPropertyConfig.VEHICLE_PROPERTY_ACCESS_READ) {
                if (cfg.isGlobalProperty()) {
                    CarPropertyValue value = mCarPropertyManager.getProperty(
                            cfg.getPropertyType(), cfg.getPropertyId(), 0);
                    if (value != null) {
                        Assert.assertEquals(value.getPropertyId(), cfg.getPropertyId());
                        mCarPropertyValues.add(value);
                    }
                } else {
                    for (int areaId : cfg.getAreaIds()) {
                        CarPropertyValue value = mCarPropertyManager.getProperty(
                            cfg.getPropertyType(), cfg.getPropertyId(), areaId);
                        if (value != null) {
                            Assert.assertEquals(value.getPropertyId(), cfg.getPropertyId());
                            mCarPropertyValues.add(value);
                        }
                    }
                }
            }
        }
    }
}
