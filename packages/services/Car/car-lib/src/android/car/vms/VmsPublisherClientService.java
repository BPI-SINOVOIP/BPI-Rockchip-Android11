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

package android.car.vms;


import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.SystemApi;
import android.app.Service;
import android.car.Car;
import android.car.annotation.RequiredFeature;
import android.car.vms.VmsClientManager.VmsClientCallback;
import android.content.Intent;
import android.os.Binder;
import android.os.Handler;
import android.os.HandlerExecutor;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;

import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

/**
 * API implementation of a Vehicle Map Service publisher client.
 *
 * All publisher clients must inherit from this class and export it as a service, and the service
 * be added to either the {@code vmsPublisherSystemClients} or {@code vmsPublisherUserClients}
 * arrays in the Car service configuration, depending on which user the client will run as.
 *
 * The {@link com.android.car.VmsPublisherService} will then bind to this service, with the
 * {@link #onVmsPublisherServiceReady()} callback notifying the client implementation when the
 * connection is established and publisher operations can be called.
 *
 * Publishers must also register a publisher ID by calling {@link #getPublisherId(byte[])}.
 *
 * @deprecated Use {@link VmsClientManager} instead
 * @hide
 */
@RequiredFeature(Car.VEHICLE_MAP_SERVICE)
@Deprecated
@SystemApi
public abstract class VmsPublisherClientService extends Service {
    private static final boolean DBG = false;
    private static final String TAG = "VmsPublisherClientService";


    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final VmsClientCallback mClientCallback = new PublisherClientCallback();

    private final Object mLock = new Object();
    @GuardedBy("mLock")
    private @Nullable Car mCar;
    @GuardedBy("mLock")
    private @Nullable VmsClient mClient;

    @Override
    public void onCreate() {
        if (DBG) Log.d(TAG, "Connecting to Car service");
        synchronized (mLock) {
            mCar = Car.createCar(this, mHandler, Car.CAR_WAIT_TIMEOUT_DO_NOT_WAIT,
                    this::onCarLifecycleChanged);
        }
    }

    @Override
    public void onDestroy() {
        if (DBG) Log.d(TAG, "Disconnecting from Car service");
        synchronized (mLock) {
            if (mCar != null) {
                mCar.disconnect();
                mCar = null;
            }
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        if (DBG) Log.d(TAG, "onBind, intent: " + intent);
        return new Binder();
    }

    /**
     * @hide
     */
    @VisibleForTesting
    protected void onCarLifecycleChanged(Car car, boolean ready) {
        if (DBG) Log.d(TAG, "Car service ready: " + ready);
        if (ready) {
            VmsClientManager clientManager =
                    (VmsClientManager) car.getCarManager(Car.VEHICLE_MAP_SERVICE);
            if (DBG) Log.d(TAG, "VmsClientManager: " + clientManager);
            if (clientManager == null) {
                Log.e(TAG, "VmsClientManager is not available");
                return;
            }
            clientManager.registerVmsClientCallback(new HandlerExecutor(mHandler), mClientCallback,
                    /* legacyClient= */ true);
        }
    }

    /**
     * Notifies the client that publisher services are ready.
     */
    protected abstract void onVmsPublisherServiceReady();

    /**
     * Notifies the client of changes in layer subscriptions.
     *
     * @param subscriptionState state of layer subscriptions
     */
    public abstract void onVmsSubscriptionChange(@NonNull VmsSubscriptionState subscriptionState);

    /**
     * Publishes a data packet to subscribers.
     *
     * Publishers must only publish packets for the layers that they have made offerings for.
     *
     * @param layer       layer to publish to
     * @param publisherId ID of the publisher publishing the message
     * @param payload     data packet to be sent
     * @throws IllegalStateException if publisher services are not available
     */
    public final void publish(@NonNull VmsLayer layer, int publisherId, byte[] payload) {
        getVmsClient().publishPacket(publisherId, layer, payload);
    }

    /**
     * Sets the layers offered by a specific publisher.
     *
     * @param offering layers being offered for subscription by the publisher
     * @throws IllegalStateException if publisher services are not available
     */
    public final void setLayersOffering(@NonNull VmsLayersOffering offering) {
        getVmsClient().setProviderOfferings(offering.getPublisherId(), offering.getDependencies());
    }

    /**
     * Acquires a publisher ID for a serialized publisher description.
     *
     * Multiple calls to this method with the same information will return the same publisher ID.
     *
     * @param publisherInfo serialized publisher description information, in a vendor-specific
     *                      format
     * @return a publisher ID for the given publisher description
     * @throws IllegalStateException if publisher services are not available
     */
    public final int getPublisherId(byte[] publisherInfo) {
        return getVmsClient().registerProvider(publisherInfo);
    }

    /**
     * Gets the state of layer subscriptions.
     *
     * @return state of layer subscriptions
     * @throws IllegalStateException if publisher services are not available
     */
    public final VmsSubscriptionState getSubscriptions() {
        return getVmsClient().getSubscriptionState();
    }

    private VmsClient getVmsClient() {
        synchronized (mLock) {
            if (mClient == null) {
                throw new IllegalStateException("VMS client connection is not ready");
            }
            return mClient;
        }
    }

    private class PublisherClientCallback implements VmsClientCallback {
        @Override
        public void onClientConnected(VmsClient client) {
            synchronized (mLock) {
                mClient = client;
            }
            onVmsPublisherServiceReady();
        }

        @Override
        public void onSubscriptionStateChanged(VmsSubscriptionState subscriptionState) {
            onVmsSubscriptionChange(subscriptionState);
        }

        @Override
        public void onLayerAvailabilityChanged(VmsAvailableLayers availableLayers) {
            // Ignored
        }

        @Override
        public void onPacketReceived(int providerId, VmsLayer layer, byte[] packet) {
            // Does not subscribe to packets
        }
    }
}
