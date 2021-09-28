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
package com.android.car;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.car.Car;
import android.car.VehicleAreaType;
import android.car.vms.VmsAvailableLayers;
import android.car.vms.VmsLayer;
import android.car.vms.VmsPublisherClientService;
import android.car.vms.VmsSubscriberManager;
import android.car.vms.VmsSubscriptionState;
import android.hardware.automotive.vehicle.V2_0.VehiclePropValue;
import android.hardware.automotive.vehicle.V2_0.VehicleProperty;
import android.hardware.automotive.vehicle.V2_0.VehiclePropertyAccess;
import android.hardware.automotive.vehicle.V2_0.VehiclePropertyChangeMode;
import android.hardware.automotive.vehicle.V2_0.VmsAvailabilityStateIntegerValuesIndex;
import android.hardware.automotive.vehicle.V2_0.VmsBaseMessageIntegerValuesIndex;
import android.hardware.automotive.vehicle.V2_0.VmsMessageType;
import android.hardware.automotive.vehicle.V2_0.VmsStartSessionMessageIntegerValuesIndex;
import android.util.Log;
import android.util.Pair;

import com.android.car.vehiclehal.VehiclePropValueBuilder;
import com.android.car.vehiclehal.test.MockedVehicleHal;

import org.junit.Before;

import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

public class MockedVmsTestBase extends MockedCarTestBase {
    public static final long PUBLISHER_CLIENT_TIMEOUT = 500L;
    public static final long MESSAGE_RECEIVE_TIMEOUT = 500L;
    private static final String TAG = "MockedVmsTestBase";

    private MockPublisherClient mPublisherClient;
    private CountDownLatch mPublisherIsReady = new CountDownLatch(1);
    private VmsSubscriberManager mVmsSubscriberManager;
    private MockSubscriberClient mSubscriberClient;
    private MockHalClient mHalClient;

    @Override
    protected synchronized void configureMockedHal() {
        mHalClient = new MockHalClient();
        addProperty(VehicleProperty.VEHICLE_MAP_SERVICE, mHalClient)
                .setChangeMode(VehiclePropertyChangeMode.ON_CHANGE)
                .setAccess(VehiclePropertyAccess.READ_WRITE)
                .addAreaConfig(VehicleAreaType.VEHICLE_AREA_TYPE_GLOBAL, 0, 0);
    }

    @Before
    public void setUpVms() throws Exception {
        mPublisherClient = new MockPublisherClient();
        mPublisherClient.setMockCar(getCar());
        mVmsSubscriberManager = (VmsSubscriberManager) getCar().getCarManager(
                Car.VMS_SUBSCRIBER_SERVICE);
        mSubscriberClient = new MockSubscriberClient();
        mVmsSubscriberManager.setVmsSubscriberClientCallback(Executors.newSingleThreadExecutor(),
                mSubscriberClient);

        assertTrue(
                "Timeout while waiting for publisher client to be ready",
                mPublisherIsReady.await(PUBLISHER_CLIENT_TIMEOUT, TimeUnit.MILLISECONDS));

        // Validate session handshake
        List<Integer> v = mHalClient.receiveMessage().value.int32Values;
        assertEquals(VmsMessageType.START_SESSION,
                (int) v.get(VmsBaseMessageIntegerValuesIndex.MESSAGE_TYPE));
        int coreId = v.get(VmsStartSessionMessageIntegerValuesIndex.SERVICE_ID);
        assertTrue(coreId > 0);
        assertEquals(-1, (int) v.get(VmsStartSessionMessageIntegerValuesIndex.CLIENT_ID));

        // Send handshake acknowledgement
        mHalClient.sendMessage(
                VmsMessageType.START_SESSION,
                coreId,
                12345 // Client ID
        );

        // Validate layer availability sent to HAL
        v = mHalClient.receiveMessage().value.int32Values;
        assertEquals(VmsMessageType.AVAILABILITY_CHANGE,
                (int) v.get(VmsAvailabilityStateIntegerValuesIndex.MESSAGE_TYPE));
        assertEquals(0,
                (int) v.get(VmsAvailabilityStateIntegerValuesIndex.SEQUENCE_NUMBER));
        assertEquals(0,
                (int) v.get(VmsAvailabilityStateIntegerValuesIndex.NUMBER_OF_ASSOCIATED_LAYERS));
    }

    VmsSubscriberManager getSubscriberManager() {
        return mVmsSubscriberManager;
    }

    MockPublisherClient getMockPublisherClient() {
        return mPublisherClient;
    }

    MockSubscriberClient getMockSubscriberClient() {
        return mSubscriberClient;
    }

    MockHalClient getMockHalClient() {
        return mHalClient;
    }

    class MockPublisherClient extends VmsPublisherClientService {
        private BlockingQueue<VmsSubscriptionState> mSubscriptionState =
                new LinkedBlockingQueue<>();

        void setMockCar(Car car) {
            onCarLifecycleChanged(car, true);
        }

        @Override
        protected void onVmsPublisherServiceReady() {
            Log.d(TAG, "MockPublisherClient.onVmsPublisherServiceReady");
            mPublisherIsReady.countDown();
        }

        @Override
        public void onVmsSubscriptionChange(VmsSubscriptionState subscriptionState) {
            Log.d(TAG, "MockPublisherClient.onVmsSubscriptionChange: "
                    + subscriptionState.getSequenceNumber());
            mSubscriptionState.add(subscriptionState);
        }

        VmsSubscriptionState receiveSubscriptionState() {
            return receiveWithTimeout(mSubscriptionState);
        }
    }

    class MockSubscriberClient implements VmsSubscriberManager.VmsSubscriberClientCallback {
        private BlockingQueue<Pair<VmsLayer, byte[]>> mMessages = new LinkedBlockingQueue<>();
        private BlockingQueue<VmsAvailableLayers> mAvailableLayers = new LinkedBlockingQueue<>();

        @Override
        public void onVmsMessageReceived(VmsLayer layer, byte[] payload) {
            Log.d(TAG, "MockSubscriberClient.onVmsMessageReceived");
            mMessages.add(Pair.create(layer, payload));
        }

        @Override
        public void onLayersAvailabilityChanged(VmsAvailableLayers availableLayers) {
            Log.d(TAG, "MockSubscriberClient.onVmsMessageReceived");
            mAvailableLayers.add(availableLayers);
        }

        Pair<VmsLayer, byte[]> receiveMessage() {
            return receiveWithTimeout(mMessages);
        }

        VmsAvailableLayers receiveLayerAvailability() {
            return receiveWithTimeout(mAvailableLayers);
        }
    }

    class MockHalClient implements MockedVehicleHal.VehicleHalPropertyHandler {
        private BlockingQueue<VehiclePropValue> mMessages = new LinkedBlockingQueue<>();

        @Override
        public void onPropertySet(VehiclePropValue value) {
            Log.d(TAG, "MockHalClient.onPropertySet");
            if (value.prop == VehicleProperty.VEHICLE_MAP_SERVICE) {
                mMessages.add(value);
            }
        }

        void sendMessage(int... message) {
            getMockedVehicleHal().injectEvent(
                    VehiclePropValueBuilder.newBuilder(VehicleProperty.VEHICLE_MAP_SERVICE)
                            .addIntValue(message)
                            .build());
        }

        void sendMessage(int[] message, byte[] payload) {
            getMockedVehicleHal().injectEvent(
                    VehiclePropValueBuilder.newBuilder(VehicleProperty.VEHICLE_MAP_SERVICE)
                            .addIntValue(message)
                            .addByteValue(payload)
                            .build());
        }

        VehiclePropValue receiveMessage() {
            return receiveWithTimeout(mMessages);
        }
    }

    private static <T> T receiveWithTimeout(BlockingQueue<T> queue) {
        try {
            return queue.poll(MESSAGE_RECEIVE_TIMEOUT, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }
}
