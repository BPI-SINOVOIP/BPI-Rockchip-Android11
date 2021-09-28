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

package android.car.cts;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.car.Car;
import android.car.hardware.CarSensorManager;
import android.platform.test.annotations.RequiresDevice;
import android.test.suitebuilder.annotation.SmallTest;

import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.CddTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.stream.IntStream;

@SmallTest
@RequiresDevice
@RunWith(AndroidJUnit4.class)
public class CarSensorManagerTest extends CarApiTestBase {

    private int[] mSupportedSensors;

    @Before
    public void setUp() throws Exception {
        super.setUp();
        CarSensorManager carSensorManager =
                (CarSensorManager) getCar().getCarManager(Car.SENSOR_SERVICE);
        mSupportedSensors = carSensorManager.getSupportedSensors();
        assertNotNull(mSupportedSensors);
    }

    @CddTest(requirement="2.5.1")
    @Test
    public void testRequiredSensorsForDrivingState() throws Exception {
        boolean foundSpeed =
            isSupportSensor(CarSensorManager.SENSOR_TYPE_CAR_SPEED);
        boolean foundGear = isSupportSensor(CarSensorManager.SENSOR_TYPE_GEAR);
        assertTrue("Must support SENSOR_TYPE_CAR_SPEED", foundSpeed);
        assertTrue("Must support SENSOR_TYPE_GEAR", foundGear);
    }

    @CddTest(requirement="2.5.1")
    @Test
    public void testMustSupportNightSensor() {
        boolean foundNightSensor =
            isSupportSensor(CarSensorManager.SENSOR_TYPE_NIGHT);
        assertTrue("Must support SENSOR_TYPE_NIGHT", foundNightSensor);
    }

    @CddTest(requirement = "2.5.1")
    @Test
    public void testMustSupportParkingBrake() throws Exception {
        boolean foundParkingBrake =
            isSupportSensor(CarSensorManager.SENSOR_TYPE_PARKING_BRAKE);
        assertTrue("Must support SENSOR_TYPE_PARKING_BRAKE", foundParkingBrake);
    }

    private boolean isSupportSensor(int sensorType) {
        return IntStream.of(mSupportedSensors)
            .anyMatch(x -> x == sensorType);
    }
}
