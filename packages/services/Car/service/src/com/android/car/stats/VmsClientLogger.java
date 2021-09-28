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

package com.android.car.stats;

import android.annotation.Nullable;
import android.car.vms.VmsLayer;
import android.util.ArrayMap;

import com.android.car.CarStatsLog;
import com.android.internal.annotations.GuardedBy;

import java.util.Collection;
import java.util.Map;
import java.util.concurrent.atomic.AtomicLong;
import java.util.stream.Collectors;

/**
 * Logger for per-client VMS statistics.
 */
public class VmsClientLogger {
    /**
     * Constants used for identifying client connection states.
     */
    public static class ConnectionState {
        // Attempting to connect to the client
        public static final int CONNECTING =
                CarStatsLog.VMS_CLIENT_CONNECTION_STATE_CHANGED__STATE__CONNECTING;
        // Client connection established
        public static final int CONNECTED =
                CarStatsLog.VMS_CLIENT_CONNECTION_STATE_CHANGED__STATE__CONNECTED;
        // Client connection closed unexpectedly
        public static final int DISCONNECTED =
                CarStatsLog.VMS_CLIENT_CONNECTION_STATE_CHANGED__STATE__DISCONNECTED;
        // Client connection closed by VMS
        public static final int TERMINATED =
                CarStatsLog.VMS_CLIENT_CONNECTION_STATE_CHANGED__STATE__TERMINATED;
        // Error establishing the client connection
        public static final int CONNECTION_ERROR =
                CarStatsLog.VMS_CLIENT_CONNECTION_STATE_CHANGED__STATE__CONNECTION_ERROR;
    }

    private final Object mLock = new Object();

    private final int mUid;
    private final String mPackageName;

    @GuardedBy("mLock")
    private Map<Integer, AtomicLong> mConnectionStateCounters = new ArrayMap<>();

    @GuardedBy("mLock")
    private final Map<VmsLayer, VmsClientStats> mLayerStats = new ArrayMap<>();

    VmsClientLogger(int clientUid, @Nullable String clientPackage) {
        mUid = clientUid;
        mPackageName = clientPackage != null ? clientPackage : "";
    }

    public int getUid() {
        return mUid;
    }

    public String getPackageName() {
        return mPackageName;
    }

    /**
     * Logs a connection state change for the client.
     *
     * @param connectionState New connection state
     */
    public void logConnectionState(int connectionState) {
        CarStatsLog.write(CarStatsLog.VMS_CLIENT_CONNECTION_STATE_CHANGED,
                mUid, connectionState);

        AtomicLong counter;
        synchronized (mLock) {
            counter = mConnectionStateCounters.computeIfAbsent(connectionState,
                    ignored -> new AtomicLong());
        }
        counter.incrementAndGet();
    }

    long getConnectionStateCount(int connectionState) {
        AtomicLong counter;
        synchronized (mLock) {
            counter = mConnectionStateCounters.get(connectionState);
        }
        return counter == null ? 0L : counter.get();
    }

    /**
     * Logs that a packet was published by the client.
     *
     * @param layer Layer of packet
     * @param size Size of packet
     */
    public void logPacketSent(VmsLayer layer, long size) {
        getLayerEntry(layer).packetSent(size);
    }

    /**
     * Logs that a packet was received successfully by the client.
     *
     * @param layer Layer of packet
     * @param size Size of packet
     */
    public void logPacketReceived(VmsLayer layer, long size) {
        getLayerEntry(layer).packetReceived(size);
    }

    /**
     * Logs that a packet was dropped due to an error delivering to the client.
     *
     * @param layer Layer of packet
     * @param size Size of packet
     */
    public void logPacketDropped(VmsLayer layer, long size) {
        getLayerEntry(layer).packetDropped(size);
    }

    Collection<VmsClientStats> getLayerEntries() {
        synchronized (mLock) {
            return mLayerStats.values().stream()
                    .map(VmsClientStats::new) // Make a deep copy of the entries
                    .collect(Collectors.toList());
        }
    }

    private VmsClientStats getLayerEntry(VmsLayer layer) {
        synchronized (mLock) {
            return mLayerStats.computeIfAbsent(
                    layer,
                    (k) -> new VmsClientStats(mUid, layer));
        }
    }
}
