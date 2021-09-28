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

import static org.testng.Assert.assertThrows;

import android.car.Car;
import android.car.content.pm.CarAppBlockingPolicy;
import android.car.content.pm.CarPackageManager;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * This class contains security permission tests for {@link CarPackageManager}.
 */
@RunWith(AndroidJUnit4.class)
public class CarPackageManagerPermissionTest {
    private Car mCar;
    private CarPackageManager mPm;

    @Before
    public void setUp() throws Exception {
        mCar = Car.createCar(InstrumentationRegistry.getInstrumentation().getTargetContext());
        mPm = (CarPackageManager) mCar.getCarManager(Car.PACKAGE_SERVICE);
    }

    @After
    public void tearDown() {
        mCar.disconnect();
    }

    @Test
    public void testRestartTask() {
        assertThrows(SecurityException.class, () -> mPm.restartTask(0));
    }

    @Test
    public void testSetAppBlockingPolicy() {
        String packageName = "com.android";
        CarAppBlockingPolicy policy = new CarAppBlockingPolicy(null, null);
        assertThrows(SecurityException.class, () -> mPm.setAppBlockingPolicy(packageName, policy,
                0));
    }

}
