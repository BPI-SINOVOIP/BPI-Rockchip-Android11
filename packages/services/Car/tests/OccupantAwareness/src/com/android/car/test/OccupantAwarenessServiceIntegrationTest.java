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

import static com.google.common.truth.Truth.assertThat;

import android.car.Car;
import android.car.occupantawareness.OccupantAwarenessDetection;
import android.car.occupantawareness.OccupantAwarenessManager;
import android.car.occupantawareness.SystemStatusEvent;
import android.content.Context;
import android.test.suitebuilder.annotation.MediumTest;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;

/*
 * IMPORTANT NOTE:
 * The test assumes that mock hal for occupant awareness interface is running at the time of test.
 * Depending on test target, mock hal for occupant awareness interface may need to be added to build
 * targets. Instructions to add mock hal to build -
 * PRODUCT_PACKAGES += android.hardware.automotive.occupant_awareness@1.0-service_mock
 *
 * Mock hal may need to be launched before running the test -
 * /system/bin/android.hardware.automotive.occupant_awareness@1.0-service_mock
 *
 * The test will likely fail if mock hal is not running on the target.
 */
@RunWith(AndroidJUnit4.class)
@MediumTest
public final class OccupantAwarenessServiceIntegrationTest {
    private CompletableFuture<SystemStatusEvent> mFutureStatus;
    private CompletableFuture<OccupantAwarenessDetection> mFutureDriverDetection;
    private CompletableFuture<OccupantAwarenessDetection> mFuturePassengerDetection;

    private final Context mContext = ApplicationProvider.getApplicationContext();
    private Car mCar = Car.createCar(mContext);
    private final OccupantAwarenessManager mOasManager =
            (OccupantAwarenessManager) mCar.getCarManager(Car.OCCUPANT_AWARENESS_SERVICE);

    @Before
    public void setUp() {
        mOasManager.registerChangeCallback(new CallbackHandler());
    }

    @Test
    public void testGetCapabilityForRole() throws Exception {
        // Mock hal only supports driver monitoring and presence detection for only driver and
        // front seat passengers.
        // This test ensures that correct values are passed to clients of occupant awareness
        // service.
        assertThat(mOasManager.getCapabilityForRole(
                OccupantAwarenessDetection.VEHICLE_OCCUPANT_DRIVER))
                .isEqualTo(SystemStatusEvent.DETECTION_TYPE_PRESENCE
                    | SystemStatusEvent.DETECTION_TYPE_DRIVER_MONITORING);

        assertThat(mOasManager.getCapabilityForRole(
                OccupantAwarenessDetection.VEHICLE_OCCUPANT_FRONT_PASSENGER))
                .isEqualTo(SystemStatusEvent.DETECTION_TYPE_PRESENCE);

        assertThat(mOasManager.getCapabilityForRole(
                OccupantAwarenessDetection.VEHICLE_OCCUPANT_ROW_2_PASSENGER_RIGHT))
                .isEqualTo(SystemStatusEvent.DETECTION_TYPE_NONE);

        assertThat(mOasManager.getCapabilityForRole(
                OccupantAwarenessDetection.VEHICLE_OCCUPANT_NONE))
                .isEqualTo(SystemStatusEvent.DETECTION_TYPE_NONE);
    }

    @Test
    public void testDetectionEvents() throws Exception {
        // Since the test assumes mock hal is the source of detections, the pattern of driver and
        // passenger detection is pre-determined. The test verifies that detections from occupant
        // awareness manager matches expected detections.
        OccupantAwarenessDetection driverDetection =
                mFutureDriverDetection.get(1, TimeUnit.SECONDS);
        OccupantAwarenessDetection passengerDetection =
                mFuturePassengerDetection.get(1, TimeUnit.SECONDS);

        assertThat(driverDetection.driverMonitoringDetection.isLookingOnRoad).isFalse();
        assertThat(passengerDetection.isPresent).isTrue();
    }

    @Test
    public void testSystemTransitionsToReady() throws Exception {
        // Since the test assumes mock hal is the source of detections, it is expected that mock hal
        // is ready to serve requests from occupant awareness service. The test verifies that
        // occupant awareness manager notifies that the system transitions to ready state.
        SystemStatusEvent status;
        status = mFutureStatus.get(1, TimeUnit.SECONDS);
        assertThat(status.systemStatus).isEqualTo(SystemStatusEvent.SYSTEM_STATUS_READY);
    }

    private final class CallbackHandler extends OccupantAwarenessManager.ChangeCallback {
        @Override
        public void onDetectionEvent(OccupantAwarenessDetection event) {
            switch(event.role) {
                case OccupantAwarenessDetection.VEHICLE_OCCUPANT_DRIVER:
                    mFutureDriverDetection.complete(event);
                    break;
                case OccupantAwarenessDetection.VEHICLE_OCCUPANT_FRONT_PASSENGER:
                    mFuturePassengerDetection.complete(event);
                    break;
            }
        }

        @Override
        public void onSystemStateChanged(SystemStatusEvent systemStatus) {
            mFutureStatus.complete(systemStatus);
        }
    }
}
