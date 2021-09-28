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

import android.car.occupantawareness.IOccupantAwarenessEventCallback;
import android.car.occupantawareness.OccupantAwarenessDetection;
import android.car.occupantawareness.SystemStatusEvent;
import android.content.Context;
import android.hardware.automotive.occupant_awareness.IOccupantAwareness;
import android.hardware.automotive.occupant_awareness.IOccupantAwarenessClientCallback;
import android.hardware.automotive.occupant_awareness.OccupantAwarenessStatus;
import android.hardware.automotive.occupant_awareness.OccupantDetection;
import android.hardware.automotive.occupant_awareness.OccupantDetections;
import android.hardware.automotive.occupant_awareness.Role;
import android.os.RemoteException;
import android.test.suitebuilder.annotation.MediumTest;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.runner.AndroidJUnit4;

import junit.framework.TestCase;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
@MediumTest
public class OccupantAwarenessSystemServiceTest extends TestCase {
    private static final int TIMESTAMP = 1234; // In milliseconds.

    /**
     * Mock implementation of {@link
     * android.hardware.automotive.occupant_awareness.IOccupantAwareness} for testing the service
     * and manager.
     */
    private class MockOasHal
            extends android.hardware.automotive.occupant_awareness.IOccupantAwareness.Stub {
        private IOccupantAwarenessClientCallback mCallback;
        private boolean mGraphIsRunning;

        MockOasHal() {}

        /** Returns whether the mock graph is running. */
        public boolean isGraphRunning() {
            return mGraphIsRunning;
        }

        @Override
        public void getLatestDetection(OccupantDetections detections) {}

        @Override
        public void setCallback(IOccupantAwarenessClientCallback callback) {
            mCallback = callback;
        }

        @Override
        public @OccupantAwarenessStatus byte getState(int occupantRole, int detectionCapability) {
            return OccupantAwarenessStatus.READY;
        }

        @Override
        public @OccupantAwarenessStatus byte startDetection() {
            mGraphIsRunning = true;
            return OccupantAwarenessStatus.READY;
        }

        @Override
        public @OccupantAwarenessStatus byte stopDetection() {
            mGraphIsRunning = false;
            return OccupantAwarenessStatus.READY;
        }

        @Override
        public int getCapabilityForRole(@Role int occupantRole) {
            if (occupantRole == OccupantAwarenessDetection.VEHICLE_OCCUPANT_DRIVER) {
                return SystemStatusEvent.DETECTION_TYPE_PRESENCE
                        | SystemStatusEvent.DETECTION_TYPE_GAZE
                        | SystemStatusEvent.DETECTION_TYPE_DRIVER_MONITORING;
            } else if (occupantRole
                    == OccupantAwarenessDetection.VEHICLE_OCCUPANT_FRONT_PASSENGER) {
                return SystemStatusEvent.DETECTION_TYPE_PRESENCE;
            } else {
                return SystemStatusEvent.DETECTION_TYPE_NONE;
            }
        }

        /** Causes a status event to be generated with the specified flags. */
        public void fireStatusEvent(int detectionFlags, @OccupantAwarenessStatus byte status)
                throws RemoteException {
            if (mCallback != null) {
                mCallback.onSystemStatusChanged(detectionFlags, status);
            }
        }

        /** Causes a status event to be generated with the specified detection event data. */
        public void fireDetectionEvent(OccupantAwarenessDetection detectionEvent)
                throws RemoteException {
            if (mCallback != null) {
                OccupantDetection detection = new OccupantDetection();

                OccupantDetections detections = new OccupantDetections();
                detections.timeStampMillis = TIMESTAMP;
                detections.detections = new OccupantDetection[] {detection};
                mCallback.onDetectionEvent(detections);
            }
        }

        @Override
        public int getInterfaceVersion() {
            return this.VERSION;
        }

        @Override
        public String getInterfaceHash() {
            return this.HASH;
        }
    }

    private MockOasHal mMockHal;
    private com.android.car.OccupantAwarenessService mOasService;

    private CompletableFuture<SystemStatusEvent> mFutureStatus;
    private CompletableFuture<OccupantAwarenessDetection> mFutureDetection;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mMockHal = new MockOasHal();
        mOasService = new com.android.car.OccupantAwarenessService(context, mMockHal);
        mOasService.init();

        resetFutures();
    }

    @After
    public void tearDown() {
        mOasService.release();
    }

    @Test
    public void testWithNoRegisteredListeners() throws Exception {
        // Verify operation when no listeners are registered.
        mMockHal.fireStatusEvent(IOccupantAwareness.CAP_NONE, OccupantAwarenessStatus.READY);

        // Nothing should have been received.
        assertThat(mFutureStatus.isDone()).isFalse();
        assertThat(mFutureDetection.isDone()).isFalse();
    }

    @Test
    public void testStatusEventsWithRegisteredListeners() throws Exception {
        // Verify correct operation when a listener has been registered.
        registerCallbackToService();
        SystemStatusEvent result;

        // Fire a status event and ensure it is received.
        // "Presence status is ready"
        resetFutures();
        mMockHal.fireStatusEvent(
                IOccupantAwareness.CAP_PRESENCE_DETECTION, OccupantAwarenessStatus.READY);

        result = mFutureStatus.get(1, TimeUnit.SECONDS);
        assertEquals(result.detectionType, SystemStatusEvent.DETECTION_TYPE_PRESENCE);
        assertEquals(result.systemStatus, SystemStatusEvent.SYSTEM_STATUS_READY);

        // "Gaze status is failed"
        resetFutures();
        mMockHal.fireStatusEvent(
                IOccupantAwareness.CAP_GAZE_DETECTION, OccupantAwarenessStatus.FAILURE);

        result = mFutureStatus.get(1, TimeUnit.SECONDS);
        assertEquals(result.detectionType, SystemStatusEvent.DETECTION_TYPE_GAZE);
        assertEquals(result.systemStatus, SystemStatusEvent.SYSTEM_STATUS_SYSTEM_FAILURE);

        // "Driver monitoring status is not-ready"
        resetFutures();
        mMockHal.fireStatusEvent(
                IOccupantAwareness.CAP_DRIVER_MONITORING_DETECTION,
                OccupantAwarenessStatus.NOT_INITIALIZED);

        result = mFutureStatus.get(1, TimeUnit.SECONDS);
        assertEquals(result.detectionType, SystemStatusEvent.DETECTION_TYPE_DRIVER_MONITORING);
        assertEquals(result.systemStatus, SystemStatusEvent.SYSTEM_STATUS_NOT_READY);

        // "None is non-supported"
        resetFutures();
        mMockHal.fireStatusEvent(
                IOccupantAwareness.CAP_NONE, OccupantAwarenessStatus.NOT_SUPPORTED);

        result = mFutureStatus.get(1, TimeUnit.SECONDS);
        assertEquals(result.detectionType, SystemStatusEvent.DETECTION_TYPE_NONE);
        assertEquals(result.systemStatus, SystemStatusEvent.SYSTEM_STATUS_NOT_SUPPORTED);
    }

    @Test
    public void test_unregisteredListeners() throws Exception {
        // Verify that listeners are successfully unregistered.
        IOccupantAwarenessEventCallback callback = registerCallbackToService();

        // Unregister the registered listener.
        mOasService.unregisterEventListener(callback);

        // Fire some events.
        mMockHal.fireStatusEvent(IOccupantAwareness.CAP_NONE, OccupantAwarenessStatus.READY);
        mMockHal.fireStatusEvent(
                IOccupantAwareness.CAP_GAZE_DETECTION, OccupantAwarenessStatus.READY);
        mMockHal.fireStatusEvent(
                IOccupantAwareness.CAP_DRIVER_MONITORING_DETECTION, OccupantAwarenessStatus.READY);

        // Nothing should have been received.
        assertThat(mFutureStatus.isDone()).isFalse();
        assertThat(mFutureDetection.isDone()).isFalse();

        // Unregister a second time should log an error, but otherwise not cause any action.
        mOasService.unregisterEventListener(callback);
    }

    @Test
    public void test_getCapabilityForRole() throws Exception {
        assertThat(
                        mOasService.getCapabilityForRole(
                                OccupantAwarenessDetection.VEHICLE_OCCUPANT_DRIVER))
                .isEqualTo(
                        SystemStatusEvent.DETECTION_TYPE_PRESENCE
                                | SystemStatusEvent.DETECTION_TYPE_GAZE
                                | SystemStatusEvent.DETECTION_TYPE_DRIVER_MONITORING);

        assertThat(
                        mOasService.getCapabilityForRole(
                                OccupantAwarenessDetection.VEHICLE_OCCUPANT_FRONT_PASSENGER))
                .isEqualTo(SystemStatusEvent.DETECTION_TYPE_PRESENCE);

        assertThat(
                        mOasService.getCapabilityForRole(
                                OccupantAwarenessDetection.VEHICLE_OCCUPANT_ROW_2_PASSENGER_RIGHT))
                .isEqualTo(SystemStatusEvent.DETECTION_TYPE_NONE);

        assertThat(
                        mOasService.getCapabilityForRole(
                                OccupantAwarenessDetection.VEHICLE_OCCUPANT_NONE))
                .isEqualTo(SystemStatusEvent.DETECTION_TYPE_NONE);
    }

    @Test
    public void test_serviceStartsAndStopGraphWithListeners() throws Exception {
        // Verify that the service starts the detection graph when the first client connects, and
        // stop when the last client disconnects.

        // Should be not running on start (no clients are yet connected).
        assertThat(mMockHal.isGraphRunning()).isFalse();

        // Connect a client. Graph should be running.
        IOccupantAwarenessEventCallback first_client = registerCallbackToService();
        assertThat(mMockHal.isGraphRunning()).isTrue();

        // Connect a second client. Graph should continue running.
        IOccupantAwarenessEventCallback second_client = registerCallbackToService();
        assertThat(mMockHal.isGraphRunning()).isTrue();

        // Remove the first client. Graph should continue to run since a client still remains.
        mOasService.unregisterEventListener(first_client);
        assertThat(mMockHal.isGraphRunning()).isTrue();

        // Remove the second client. Graph should now stop since all clients have now closed.
        mOasService.unregisterEventListener(second_client);
        assertThat(mMockHal.isGraphRunning()).isFalse();
    }

    /** Registers a listener to the service. */
    private IOccupantAwarenessEventCallback registerCallbackToService() {
        IOccupantAwarenessEventCallback callback =
                new IOccupantAwarenessEventCallback.Stub() {
                    @Override
                    public void onStatusChanged(SystemStatusEvent systemStatusEvent) {
                        mFutureStatus.complete(systemStatusEvent);
                    }

                    public void onDetectionEvent(OccupantAwarenessDetection detectionEvent) {
                        mFutureDetection.complete(detectionEvent);
                    }
                };

        mOasService.registerEventListener(callback);
        return callback;
    }

    /** Resets futures for testing. */
    private void resetFutures() {
        mFutureStatus = new CompletableFuture<>();
        mFutureDetection = new CompletableFuture<>();
    }
}
