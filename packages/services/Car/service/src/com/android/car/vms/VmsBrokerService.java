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

package com.android.car.vms;

import static com.android.car.ICarImpl.assertAnyVmsPermission;
import static com.android.car.ICarImpl.assertVmsPublisherPermission;
import static com.android.car.ICarImpl.assertVmsSubscriberPermission;

import android.car.vms.IVmsBrokerService;
import android.car.vms.IVmsClientCallback;
import android.car.vms.VmsAssociatedLayer;
import android.car.vms.VmsAvailableLayers;
import android.car.vms.VmsLayer;
import android.car.vms.VmsLayerDependency;
import android.car.vms.VmsLayersOffering;
import android.car.vms.VmsProviderInfo;
import android.car.vms.VmsRegistrationInfo;
import android.car.vms.VmsSubscriptionState;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Binder;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.SharedMemory;
import android.util.ArrayMap;
import android.util.ArraySet;
import android.util.Log;

import com.android.car.CarServiceBase;
import com.android.car.stats.CarStatsService;
import com.android.car.stats.VmsClientLogger;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.FunctionalUtils.ThrowingConsumer;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.IntSupplier;
import java.util.stream.Collectors;

/**
 * Message broker service for routing Vehicle Map Service messages between clients.
 *
 * This service is also responsible for tracking VMS client connections and broadcasting
 * notifications to clients about layer offering or subscription state changes.
 */
public class VmsBrokerService extends IVmsBrokerService.Stub implements CarServiceBase {
    private static final boolean DBG = false;
    private static final String TAG = VmsBrokerService.class.getSimpleName();

    private final Context mContext;
    private final PackageManager mPackageManager;
    private final CarStatsService mStatsService;
    private final IntSupplier mGetCallingUid;

    private final VmsProviderInfoStore mProviderInfoStore = new VmsProviderInfoStore();
    private final VmsLayerAvailability mAvailableLayers = new VmsLayerAvailability();

    private final Object mLock = new Object();
    @GuardedBy("mLock")
    private final Map<IBinder /* clientToken */, VmsClientInfo> mClientMap = new ArrayMap<>();
    @GuardedBy("mLock")
    private Set<VmsLayersOffering> mAllOfferings = Collections.emptySet();
    @GuardedBy("mLock")
    private VmsSubscriptionState mSubscriptionState = new VmsSubscriptionState(0,
            Collections.emptySet(), Collections.emptySet());

    public VmsBrokerService(Context context, CarStatsService statsService) {
        this(context, statsService, Binder::getCallingUid);
    }

    @VisibleForTesting
    VmsBrokerService(
            Context context,
            CarStatsService statsService,
            IntSupplier getCallingUid) {
        mContext = context;
        mPackageManager = context.getPackageManager();
        mStatsService = statsService;
        mGetCallingUid = getCallingUid;
    }

    @Override
    public void init() {
    }

    @Override
    public void release() {
    }

    @Override
    public void dump(PrintWriter writer) {
        writer.println("*" + TAG + "*");
        synchronized (mLock) {
            writer.println("mAvailableLayers: " + mAvailableLayers.getAvailableLayers());
            writer.println();
            writer.println("mSubscriptionState: " + mSubscriptionState);
            writer.println();
            writer.println("mClientMap:");
            mClientMap.values().stream()
                    .sorted(Comparator.comparingInt(VmsClientInfo::getUid))
                    .forEach(client -> client.dump(writer, "  "));
        }
    }

    @Override
    public VmsRegistrationInfo registerClient(IBinder clientToken, IVmsClientCallback callback,
            boolean legacyClient) {
        assertAnyVmsPermission(mContext);
        int clientUid = mGetCallingUid.getAsInt();
        String clientPackage = mPackageManager.getNameForUid(clientUid);
        if (DBG) Log.d(TAG, "registerClient uid: " + clientUid + " package: " + clientPackage);

        mStatsService.getVmsClientLogger(clientUid)
                .logConnectionState(VmsClientLogger.ConnectionState.CONNECTED);

        IBinder.DeathRecipient deathRecipient;
        try {
            deathRecipient = () -> unregisterClient(clientToken,
                    VmsClientLogger.ConnectionState.DISCONNECTED);
            callback.asBinder().linkToDeath(deathRecipient, 0);
        } catch (RemoteException e) {
            mStatsService.getVmsClientLogger(clientUid)
                    .logConnectionState(VmsClientLogger.ConnectionState.DISCONNECTED);
            throw new IllegalStateException("Client callback is already dead");
        }

        synchronized (mLock) {
            mClientMap.put(clientToken, new VmsClientInfo(clientUid, clientPackage, callback,
                    legacyClient, deathRecipient));
            return new VmsRegistrationInfo(
                    mAvailableLayers.getAvailableLayers(),
                    mSubscriptionState);
        }
    }

    @Override
    public void unregisterClient(IBinder clientToken) {
        assertAnyVmsPermission(mContext);
        unregisterClient(clientToken, VmsClientLogger.ConnectionState.TERMINATED);
    }

    @Override
    public VmsProviderInfo getProviderInfo(IBinder clientToken, int providerId) {
        assertAnyVmsPermission(mContext);
        getClient(clientToken); // Assert that the client is registered
        return new VmsProviderInfo(mProviderInfoStore.getProviderInfo(providerId));
    }

    @Override
    public void setSubscriptions(IBinder clientToken, List<VmsAssociatedLayer> layers) {
        assertVmsSubscriberPermission(mContext);
        getClient(clientToken).setSubscriptions(layers);
        updateSubscriptionState();
    }

    @Override
    public void setMonitoringEnabled(IBinder clientToken, boolean enabled) {
        assertVmsSubscriberPermission(mContext);
        getClient(clientToken).setMonitoringEnabled(enabled);
    }

    @Override
    public int registerProvider(IBinder clientToken, VmsProviderInfo providerInfo) {
        assertVmsPublisherPermission(mContext);
        VmsClientInfo client = getClient(clientToken);
        int providerId;
        synchronized (mLock) {
            providerId = mProviderInfoStore.getProviderId(providerInfo.getDescription());
        }
        client.addProviderId(providerId);
        return providerId;
    }

    @Override
    public void setProviderOfferings(IBinder clientToken, int providerId,
            List<VmsLayerDependency> offerings) {
        assertVmsPublisherPermission(mContext);
        VmsClientInfo client = getClient(clientToken);
        if (!client.hasProviderId(providerId) && !client.isLegacyClient()) {
            throw new IllegalArgumentException("Client not registered to offer layers as "
                    + providerId);
        }
        if (client.setProviderOfferings(providerId, offerings)) {
            updateAvailableLayers();
        }
    }

    @Override
    public void publishPacket(IBinder clientToken, int providerId, VmsLayer layer, byte[] packet) {
        assertVmsPublisherPermission(mContext);
        deliverToSubscribers(clientToken, providerId, layer, packet.length,
                callback -> callback.onPacketReceived(providerId, layer, packet));
    }

    @Override
    public void publishLargePacket(IBinder clientToken, int providerId, VmsLayer layer,
            SharedMemory packet) {
        try (SharedMemory largePacket = packet) {
            assertVmsPublisherPermission(mContext);
            deliverToSubscribers(clientToken, providerId, layer, packet.getSize(),
                    callback -> callback.onLargePacketReceived(providerId, layer, largePacket));
        }
    }

    private void deliverToSubscribers(IBinder clientToken, int providerId, VmsLayer layer,
            int packetLength, ThrowingConsumer<IVmsClientCallback> callbackConsumer) {
        VmsClientInfo client = getClient(clientToken);
        if (!client.hasOffering(providerId, layer) && !client.isLegacyClient()) {
            throw new IllegalArgumentException("Client does not offer " + layer + " as "
                    + providerId);
        }

        mStatsService.getVmsClientLogger(client.getUid())
                .logPacketSent(layer, packetLength);

        Collection<VmsClientInfo> subscribers;
        synchronized (mLock) {
            subscribers = mClientMap.values().stream()
                    .filter(subscriber -> subscriber.isSubscribed(providerId, layer))
                    .collect(Collectors.toList());
        }

        if (DBG) Log.d(TAG, String.format("Number of subscribers: %d", subscribers.size()));

        if (subscribers.isEmpty()) {
            // A negative UID signals that the packet had zero subscribers
            mStatsService.getVmsClientLogger(-1).logPacketDropped(layer, packetLength);
            return;
        }

        for (VmsClientInfo subscriber : subscribers) {
            try {
                callbackConsumer.accept(subscriber.getCallback());
                mStatsService.getVmsClientLogger(subscriber.getUid())
                        .logPacketReceived(layer, packetLength);
            } catch (RuntimeException e) {
                mStatsService.getVmsClientLogger(subscriber.getUid())
                        .logPacketDropped(layer, packetLength);
                Log.e(TAG, String.format("Unable to publish to listener: %s",
                        subscriber.getPackageName()), e);
            }
        }
    }

    private void unregisterClient(IBinder clientToken, int connectionState) {
        VmsClientInfo client;
        synchronized (mLock) {
            client = mClientMap.remove(clientToken);
        }
        if (client != null) {
            client.getCallback().asBinder().unlinkToDeath(client.getDeathRecipient(), 0);
            mStatsService.getVmsClientLogger(client.getUid())
                    .logConnectionState(connectionState);
            updateAvailableLayers();
            updateSubscriptionState();
        }
    }

    private VmsClientInfo getClient(IBinder clientToken) {
        synchronized (mLock) {
            VmsClientInfo client = mClientMap.get(clientToken);
            if (client == null) {
                throw new IllegalStateException("Unknown client token");
            }
            return client;
        }
    }

    private Collection<VmsClientInfo> getActiveClients() {
        synchronized (mLock) {
            return new ArrayList<>(mClientMap.values());
        }
    }

    private void updateAvailableLayers() {
        synchronized (mLock) {
            // Fuse layer offerings
            Set<VmsLayersOffering> allOfferings = mClientMap.values().stream()
                    .map(VmsClientInfo::getAllOfferings)
                    .flatMap(Collection::stream)
                    .collect(Collectors.toCollection(ArraySet::new));

            // Ignore update if offerings are unchanged
            if (mAllOfferings.equals(allOfferings)) {
                return;
            }

            // Update offerings and compute available layers
            mAllOfferings = allOfferings;
            mAvailableLayers.setPublishersOffering(allOfferings);
        }
        notifyOfAvailabilityChange(mAvailableLayers.getAvailableLayers());
    }

    private void notifyOfAvailabilityChange(VmsAvailableLayers availableLayers) {
        Log.i(TAG, "Notifying clients of layer availability change: " + availableLayers);
        for (VmsClientInfo client : getActiveClients()) {
            try {
                client.getCallback().onLayerAvailabilityChanged(availableLayers);
            } catch (RemoteException e) {
                Log.w(TAG, "onLayersAvailabilityChanged failed: " + client.getPackageName(),
                        e);
            }
        }
    }

    private void updateSubscriptionState() {
        VmsSubscriptionState subscriptionState;
        synchronized (mLock) {
            Set<VmsLayer> layerSubscriptions = new ArraySet<>();
            Map<VmsLayer, Set<Integer>> layerAndProviderSubscriptions = new ArrayMap<>();
            // Fuse subscriptions
            for (VmsClientInfo client : mClientMap.values()) {
                layerSubscriptions.addAll(client.getLayerSubscriptions());
                client.getLayerAndProviderSubscriptions().forEach((layer, providerIds) -> {
                    Set<Integer> providerSubscriptions =
                            layerAndProviderSubscriptions.computeIfAbsent(
                                    layer,
                                    ignored -> new ArraySet<>());
                    providerSubscriptions.addAll(providerIds);
                });
            }

            // Remove global layer subscriptions from provider-specific subscription state
            layerSubscriptions.forEach(layerAndProviderSubscriptions::remove);

            // Transform provider-specific subscriptions into VmsAssociatedLayers
            Set<VmsAssociatedLayer> associatedLayers =
                    layerAndProviderSubscriptions.entrySet().stream()
                            .map(entry -> new VmsAssociatedLayer(entry.getKey(), entry.getValue()))
                            .collect(Collectors.toCollection(ArraySet::new));

            // Ignore update if subscriptions are unchanged
            if (mSubscriptionState.getLayers().equals(layerSubscriptions)
                    && mSubscriptionState.getAssociatedLayers().equals(associatedLayers)) {
                return;
            }

            // Update subscription state
            subscriptionState = new VmsSubscriptionState(
                    mSubscriptionState.getSequenceNumber() + 1,
                    layerSubscriptions,
                    associatedLayers);
            mSubscriptionState = subscriptionState;
        }
        // Notify clients of update
        notifyOfSubscriptionChange(subscriptionState);
    }

    private void notifyOfSubscriptionChange(VmsSubscriptionState subscriptionState) {
        Log.i(TAG, "Notifying clients of subscription state change: " + subscriptionState);
        for (VmsClientInfo client : getActiveClients()) {
            try {
                client.getCallback().onSubscriptionStateChanged(subscriptionState);
            } catch (RemoteException e) {
                Log.w(TAG, "onSubscriptionStateChanged failed: " + client.getPackageName(),
                        e);
            }
        }
    }
}
