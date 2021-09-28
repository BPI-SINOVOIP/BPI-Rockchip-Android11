/*
 * Copyright (C) 2017 The Android Open Source Project
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

import static org.junit.Assume.assumeTrue;

import android.car.Car;
import android.car.diagnostic.CarDiagnosticEvent;
import android.car.diagnostic.CarDiagnosticManager;
import android.test.suitebuilder.annotation.MediumTest;

import org.junit.Before;
import org.junit.Test;

@MediumTest
public class CarDiagnosticManagerTest extends CarApiTestBase {

    private CarDiagnosticManager mCarDiagnosticManager;

    @Before
    public void setUp() throws Exception {
        Car car = getCar();
        assumeTrue(car.isFeatureEnabled(Car.DIAGNOSTIC_SERVICE));
        mCarDiagnosticManager = (CarDiagnosticManager) car.getCarManager(Car.DIAGNOSTIC_SERVICE);
        assertThat(mCarDiagnosticManager).isNotNull();
    }

    /**
     * Test that we can read live frame data over the diagnostic manager
     *
     * @throws Exception
     */
    @Test
    public void testLiveFrame() throws Exception {
        CarDiagnosticEvent liveFrame = mCarDiagnosticManager.getLatestLiveFrame();
        if (null != liveFrame) {
            assertThat(liveFrame.isLiveFrame()).isTrue();
            assertThat(liveFrame.isEmptyFrame()).isFalse();
        }
    }

    /**
     * Test that we can read well-formed freeze frame data over the diagnostic manager
     *
     * @throws Exception
     */
    @Test
    public void testFreezeFrames() throws Exception {
        long[] timestamps = mCarDiagnosticManager.getFreezeFrameTimestamps();
        if (null != timestamps) {
            for (long timestamp : timestamps) {
                CarDiagnosticEvent freezeFrame = mCarDiagnosticManager.getFreezeFrame(timestamp);
                assertThat(freezeFrame).isNotNull();
                assertThat(freezeFrame.timestamp).isEqualTo(timestamp);
                assertThat(freezeFrame.isFreezeFrame()).isTrue();
                assertThat(freezeFrame.isEmptyFrame()).isFalse();
                assertThat(freezeFrame.dtc).isNotEmpty();
            }
        }
    }
}
