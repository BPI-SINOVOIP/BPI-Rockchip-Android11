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

import android.car.Car;
import android.car.CarInfoManager;
import android.test.suitebuilder.annotation.SmallTest;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Before;
import org.junit.Test;

@SmallTest
public class CarInfoManagerTest extends CarApiTestBase {

    private CarInfoManager mInfoManager;

    @Before
    public void setUp() throws Exception {
        mInfoManager = (CarInfoManager) getCar().getCarManager(Car.INFO_SERVICE);
        assertThat(mInfoManager).isNotNull();
    }

    @Test
    public void testVehicleId() throws Exception {
        assertThat(mInfoManager.getVehicleId()).isNotNull();
    }

    @Test
    public void testNotNullItems() throws Exception {
        // call and check if it throws exception.
        assertThat(mInfoManager.getManufacturer()).isNotNull();
        assertThat(mInfoManager.getModel()).isNotNull();
        assertThat(mInfoManager.getModelYear()).isNotNull();
    }
}
