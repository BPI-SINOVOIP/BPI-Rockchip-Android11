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

import android.annotation.CallbackExecutor;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.SystemApi;
import android.car.Car;
import android.car.CarManagerBase;
import android.car.annotation.RequiredFeature;
import android.car.vms.VmsClientManager.VmsClientCallback;

import com.android.internal.annotations.GuardedBy;

import java.util.Objects;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

/**
 * API implementation for use by Vehicle Map Service subscribers.
 *
 * Supports a single client callback that can subscribe and unsubscribe to different data layers.
 * {@link #setVmsSubscriberClientCallback} must be called before any subscription operations.
 *
 * @deprecated Use {@link VmsClientManager} instead
 * @hide
 */
@RequiredFeature(Car.VMS_SUBSCRIBER_SERVICE)
@Deprecated
@SystemApi
public final class VmsSubscriberManager extends CarManagerBase {
    private static final long CLIENT_READY_TIMEOUT_MS = 500;
    private static final byte[] DEFAULT_PUBLISHER_INFO = new byte[0];

    /**
     * Callback interface for Vehicle Map Service subscribers.
     */
    public interface VmsSubscriberClientCallback {
        /**
         * Called when a data packet is received.
         *
         * @param layer   subscribed layer that packet was received for
         * @param payload data packet that was received
         */
        void onVmsMessageReceived(@NonNull VmsLayer layer, byte[] payload);

        /**
         * Called when set of available data layers changes.
         *
         * @param availableLayers set of available data layers
         */
        void onLayersAvailabilityChanged(@NonNull VmsAvailableLayers availableLayers);
    }

    private final VmsClientManager mClientManager;

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private @Nullable VmsClient mClient;

    @GuardedBy("mLock")
    private @Nullable VmsClientCallback mClientCallback;

    private final VmsSubscriptionHelper mSubscriptionHelper =
            new VmsSubscriptionHelper(this::setSubscriptions);

    /**
     * @hide
     */
    public static VmsSubscriberManager wrap(Car car, @Nullable VmsClientManager clientManager) {
        if (clientManager == null) {
            return null;
        }
        return new VmsSubscriberManager(car, clientManager);
    }

    private VmsSubscriberManager(Car car, VmsClientManager clientManager) {
        super(car);
        mClientManager = clientManager;
    }

    /**
     * Sets the subscriber client's callback, for receiving layer availability and data events.
     *
     * @param executor       {@link Executor} to handle the callbacks
     * @param clientCallback subscriber callback that will handle events
     * @throws IllegalStateException if the client callback was already set
     */
    public void setVmsSubscriberClientCallback(
            @NonNull @CallbackExecutor Executor executor,
            @NonNull VmsSubscriberClientCallback clientCallback) {
        Objects.requireNonNull(clientCallback, "clientCallback cannot be null");
        Objects.requireNonNull(executor, "executor cannot be null");
        CountDownLatch clientReady;
        synchronized (mLock) {
            if (mClientCallback != null) {
                throw new IllegalStateException("Client callback is already configured.");
            }
            clientReady = new CountDownLatch(1);
            mClientCallback = new SubscriberCallbackWrapper(clientCallback, clientReady);
            // Register callback with broker service
            mClientManager.registerVmsClientCallback(executor, mClientCallback,
                    /* legacyClient= */ true);
        }

        try {
            // Wait for VmsClient to be available
            if (!clientReady.await(CLIENT_READY_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                clearVmsSubscriberClientCallback();
                throw new IllegalStateException("Subscriber client is not ready");
            }
        } catch (InterruptedException e) {
            clearVmsSubscriberClientCallback();
            Thread.currentThread().interrupt();
            throw new IllegalStateException("Interrupted while waiting for subscriber client", e);
        }
    }

    /**
     * Clears the subscriber client's callback.
     */
    public void clearVmsSubscriberClientCallback() {
        synchronized (mLock) {
            mClientManager.unregisterVmsClientCallback(mClientCallback);
            mClient = null;
            mClientCallback = null;
        }
    }

    /**
     * Gets a publisher's self-reported description information.
     *
     * @param publisherId publisher ID to retrieve information for
     * @return serialized publisher information, in a vendor-specific format
     */
    @NonNull
    public byte[] getPublisherInfo(int publisherId) {
        byte[] publisherInfo = getVmsClient().getProviderDescription(publisherId);
        return publisherInfo != null ? publisherInfo : DEFAULT_PUBLISHER_INFO;
    }

    /**
     * Gets all layers available for subscription.
     *
     * @return available layers
     */
    @NonNull
    public VmsAvailableLayers getAvailableLayers() {
        return getVmsClient().getAvailableLayers();
    }

    /**
     * Subscribes to data packets for a specific layer.
     *
     * @param layer layer to subscribe to
     * @throws IllegalStateException if the client callback was not set via
     *                               {@link #setVmsSubscriberClientCallback}.
     */
    public void subscribe(@NonNull VmsLayer layer) {
        mSubscriptionHelper.subscribe(layer);
    }

    /**
     * Subscribes to data packets for a specific layer from a specific publisher.
     *
     * @param layer       layer to subscribe to
     * @param publisherId a publisher of the layer
     * @throws IllegalStateException if the client callback was not set via
     *                               {@link #setVmsSubscriberClientCallback}.
     */
    public void subscribe(@NonNull VmsLayer layer, int publisherId) {
        mSubscriptionHelper.subscribe(layer, publisherId);
    }

    /**
     * Start monitoring all messages for all layers, regardless of subscriptions.
     */
    public void startMonitoring() {
        getVmsClient().setMonitoringEnabled(true);
    }

    /**
     * Unsubscribes from data packets for a specific layer.
     *
     * @param layer layer to unsubscribe from
     * @throws IllegalStateException if the client callback was not set via
     *                               {@link #setVmsSubscriberClientCallback}.
     */
    public void unsubscribe(@NonNull VmsLayer layer) {
        mSubscriptionHelper.unsubscribe(layer);
    }

    /**
     * Unsubscribes from data packets for a specific layer from a specific publisher.
     *
     * @param layer       layer to unsubscribe from
     * @param publisherId a publisher of the layer
     * @throws IllegalStateException if the client callback was not set via
     *                               {@link #setVmsSubscriberClientCallback}.
     */
    public void unsubscribe(@NonNull VmsLayer layer, int publisherId) {
        mSubscriptionHelper.unsubscribe(layer, publisherId);
    }

    /**
     * Stop monitoring. Only receive messages for layers which have been subscribed to."
     */
    public void stopMonitoring() {
        getVmsClient().setMonitoringEnabled(false);
    }

    /**
     * @hide
     */
    @Override
    public void onCarDisconnected() {}

    private void setSubscriptions(Set<VmsAssociatedLayer> subscriptions) {
        getVmsClient().setSubscriptions(subscriptions);
    }

    private VmsClient getVmsClient() {
        synchronized (mLock) {
            if (mClient == null) {
                throw new IllegalStateException("VMS client connection is not ready");
            }
            return mClient;
        }
    }

    private final class SubscriberCallbackWrapper implements VmsClientCallback {
        private final VmsSubscriberClientCallback mCallback;
        private final CountDownLatch mClientReady;

        SubscriberCallbackWrapper(VmsSubscriberClientCallback callback,
                CountDownLatch clientReady) {
            mCallback = callback;
            mClientReady = clientReady;
        }

        @Override
        public void onClientConnected(VmsClient client) {
            synchronized (mLock) {
                mClient = client;
            }
            mClientReady.countDown();
        }

        @Override
        public void onSubscriptionStateChanged(VmsSubscriptionState subscriptionState) {
            // Ignored
        }

        @Override
        public void onLayerAvailabilityChanged(VmsAvailableLayers availableLayers) {
            mCallback.onLayersAvailabilityChanged(availableLayers);
        }

        @Override
        public void onPacketReceived(int providerId, VmsLayer layer, byte[] packet) {
            mCallback.onVmsMessageReceived(layer, packet);
        }
    }
}
