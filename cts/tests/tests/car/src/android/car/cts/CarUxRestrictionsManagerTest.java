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


import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.car.Car;
import android.car.drivingstate.CarUxRestrictions;
import android.car.drivingstate.CarUxRestrictionsManager;
import android.platform.test.annotations.RequiresDevice;
import android.test.suitebuilder.annotation.SmallTest;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RequiresDevice
@RunWith(AndroidJUnit4.class)
public class CarUxRestrictionsManagerTest extends CarApiTestBase {
    private CarUxRestrictionsManager mManager;

    @Before
    public void setUp() throws Exception {
        super.setUp();

        mManager = (CarUxRestrictionsManager)
                getCar().getCarManager(Car.CAR_UX_RESTRICTION_SERVICE);
        assertNotNull(mManager);
    }

    @Test
    public void testCarUxRestrictionsBuilder() {
        int maxContentDepth = 1;
        int maxCumulativeContentItems = 1;
        int maxStringLength = 1;
        CarUxRestrictions.Builder builder = new CarUxRestrictions.Builder(
                true, CarUxRestrictions.UX_RESTRICTIONS_FULLY_RESTRICTED, 0L);
        builder.setMaxContentDepth(maxContentDepth);
        builder.setMaxCumulativeContentItems(maxCumulativeContentItems);
        builder.setMaxStringLength(maxStringLength);

        CarUxRestrictions restrictions = builder.build();

        assertTrue(restrictions.toString(),
                restrictions.isRequiresDistractionOptimization());
        assertEquals(restrictions.toString(),
                restrictions.getActiveRestrictions(),
                CarUxRestrictions.UX_RESTRICTIONS_FULLY_RESTRICTED);
        assertEquals(restrictions.toString(),
                restrictions.getMaxContentDepth(), maxContentDepth);
        assertEquals(restrictions.toString(),
                restrictions.getMaxCumulativeContentItems(), maxCumulativeContentItems);
        assertEquals(restrictions.toString(),
                restrictions.getMaxRestrictedStringLength(), maxStringLength);
    }

    @Test
    public void testCarUxRestrictions_isSameRestrictions() {
        CarUxRestrictions.Builder oneBuilder = new CarUxRestrictions.Builder(
                true, CarUxRestrictions.UX_RESTRICTIONS_FULLY_RESTRICTED, 0L);
        CarUxRestrictions.Builder anotherBuilder = new CarUxRestrictions.Builder(
                true, CarUxRestrictions.UX_RESTRICTIONS_FULLY_RESTRICTED, 0L);

        assertTrue(oneBuilder.build().isSameRestrictions(anotherBuilder.build()));
    }

    @Test
    public void testRegisterListener_noCrash() {
        mManager.registerListener(restrictions -> {});
        mManager.unregisterListener();
    }
}
