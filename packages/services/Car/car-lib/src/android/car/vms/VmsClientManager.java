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

package android.car.vms;

import android.annotation.CallbackExecutor;
import android.annotation.NonNull;
import android.annotation.RequiresPermission;
import android.annotation.SystemApi;
import android.car.Car;
import android.car.CarManagerBase;
import android.car.annotation.RequiredFeature;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.ArrayMap;
import android.util.Log;

import com.android.internal.annotations.GuardedBy;

import java.util.Map;
import java.util.Objects;
import java.util.concurrent.Executor;

/**
 * Car Service manager for connecting clients to the Vehicle Map Service.
 *
 * @hide
 */
@RequiredFeature(Car.VEHICLE_MAP_SERVICE)
@SystemApi
public final class VmsClientManager extends CarManagerBase {
    private static final boolean DBG = false;
    private static final String TAG = VmsClientManager.class.getSimpleName();

    /**
     * Callback interface for Vehicle Map Service clients.
     */
    public interface VmsClientCallback {
        /**
         * Invoked when a Vehicle Map Service connection has been established for the callback.
         *
         * @param client API client
         */
        void onClientConnected(@NonNull VmsClient client);

        /**
         * Invoked when the availability of data layers has changed.
         *
         * @param availableLayers Current layer availability
         */
        void onLayerAvailabilityChanged(@NonNull VmsAvailableLayers availableLayers);

        /**
         * Invoked when any subscriptions to data layers have changed.
         *
         * @param subscriptionState Current subscription state
         */
        void onSubscriptionStateChanged(@NonNull VmsSubscriptionState subscriptionState);

        /**
         * Invoked whenever a packet is received for this client's subscriptions.
         *
         * @param providerId  Packet provider
         * @param layer       Packet layer
         * @param packet      Packet data
         */
        void onPacketReceived(int providerId, @NonNull VmsLayer layer, @NonNull byte[] packet);
    }

    private final IVmsBrokerService mBrokerService;

    private final Object mLock = new Object();
    @GuardedBy("mLock")
    private final Map<VmsClientCallback, VmsClient> mClients = new ArrayMap<>();

    /**
     * @hide
     */
    public VmsClientManager(Car car, IBinder service) {
        super(car);
        mBrokerService = IVmsBrokerService.Stub.asInterface(service);
    }

    /**
     * Registers new Vehicle Map Service client for the given callback.
     *
     * If the callback is already registered, no action is taken.
     *
     * @param executor Executor to run callback operations
     * @param callback Callback to register for new client
     */
    @RequiresPermission(anyOf = {Car.PERMISSION_VMS_PUBLISHER, Car.PERMISSION_VMS_SUBSCRIBER})
    public void registerVmsClientCallback(
            @NonNull @CallbackExecutor Executor executor,
            @NonNull VmsClientCallback callback) {
        registerVmsClientCallback(executor, callback, false);
    }

    /**
     * @hide
     */
    @RequiresPermission(anyOf = {Car.PERMISSION_VMS_PUBLISHER, Car.PERMISSION_VMS_SUBSCRIBER})
    void registerVmsClientCallback(
            @NonNull @CallbackExecutor Executor executor,
            @NonNull VmsClientCallback callback,
            boolean legacyClient) {
        Objects.requireNonNull(executor, "executor cannot be null");
        Objects.requireNonNull(callback, "callback cannot be null");
        VmsClient client;
        synchronized (mLock) {
            if (mClients.containsKey(callback)) {
                Log.w(TAG, "VmsClient already registered");
                return;
            }

            client = new VmsClient(mBrokerService, executor, callback, legacyClient,
                    /* autoCloseMemory */ true,
                    this::handleRemoteExceptionFromCarService);
            mClients.put(callback, client);
            if (DBG) Log.d(TAG, "Client count: " + mClients.size());
        }

        try {
            if (DBG) Log.d(TAG, "Registering VmsClient");
            client.register();
        } catch (RemoteException e) {
            Log.e(TAG, "Error while registering", e);
            synchronized (mLock) {
                mClients.remove(callback);
            }
            handleRemoteExceptionFromCarService(e);
            return;
        }

        if (DBG) Log.d(TAG, "Triggering callbacks for new VmsClient");
        executor.execute(() -> {
            callback.onClientConnected(client);
            if (!legacyClient) {
                callback.onLayerAvailabilityChanged(client.getAvailableLayers());
                callback.onSubscriptionStateChanged(client.getSubscriptionState());
            }
        });
    }

    /**
     * Unregisters the Vehicle Map Service client associated with the given callback.
     *
     * If the callback is not registered, no action is taken.
     *
     * @param callback
     */
    @RequiresPermission(anyOf = {Car.PERMISSION_VMS_PUBLISHER, Car.PERMISSION_VMS_SUBSCRIBER})
    public void unregisterVmsClientCallback(@NonNull VmsClientCallback callback) {
        VmsClient client;
        synchronized (mLock) {
            client = mClients.remove(callback);
        }
        if (client == null) {
            Log.w(TAG, "Unregister called for unknown callback");
            return;
        }

        if (DBG) Log.d(TAG, "Unregistering VmsClient");
        try {
            client.unregister();
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * @hide
     */
    @Override
    protected void onCarDisconnected() {
        synchronized (mLock) {
            Log.w(TAG, "Car disconnected with " + mClients.size() + " active clients");
            mClients.clear();
        }
    }
}
