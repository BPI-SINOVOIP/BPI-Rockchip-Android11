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

import android.car.vms.VmsLayer;

import com.android.internal.annotations.GuardedBy;

/**
 * Java representation of VmsClientStats statsd atom.
 *
 * All access to this class is synchronized through VmsClientLog.
 */
class VmsClientStats {
    private final Object mLock = new Object();

    private final int mUid;

    private final int mLayerType;
    private final int mLayerChannel;
    private final int mLayerVersion;

    @GuardedBy("mLock")
    private long mTxBytes;
    @GuardedBy("mLock")
    private long mTxPackets;

    @GuardedBy("mLock")
    private long mRxBytes;
    @GuardedBy("mLock")
    private long mRxPackets;

    @GuardedBy("mLock")
    private long mDroppedBytes;
    @GuardedBy("mLock")
    private long mDroppedPackets;

    /**
     * Constructor for a VmsClientStats entry.
     *
     * @param uid UID of client package.
     * @param layer Vehicle Maps Service layer.
     */
    VmsClientStats(int uid, VmsLayer layer) {
        mUid = uid;

        mLayerType = layer.getType();
        mLayerChannel = layer.getSubtype();
        mLayerVersion = layer.getVersion();
    }

    /**
     * Copy constructor for entries exported from {@link VmsClientLogger}.
     */
    VmsClientStats(VmsClientStats other) {
        synchronized (other.mLock) {
            this.mUid = other.mUid;

            this.mLayerType = other.mLayerType;
            this.mLayerChannel = other.mLayerChannel;
            this.mLayerVersion = other.mLayerVersion;

            this.mTxBytes = other.mTxBytes;
            this.mTxPackets = other.mTxPackets;
            this.mRxBytes = other.mRxBytes;
            this.mRxPackets = other.mRxPackets;
            this.mDroppedBytes = other.mDroppedBytes;
            this.mDroppedPackets = other.mDroppedPackets;
        }
    }

    /**
     * Records that a packet was sent by a publisher client.
     *
     * @param size Size of packet.
     */
    void packetSent(long size) {
        synchronized (mLock) {
            mTxBytes += size;
            mTxPackets++;
        }
    }

    /**
     * Records that a packet was successfully received by a subscriber client.
     *
     * @param size Size of packet.
     */
    void packetReceived(long size) {
        synchronized (mLock) {
            mRxBytes += size;
            mRxPackets++;
        }
    }

    /**
     * Records that a packet was dropped while being delivered to a subscriber client.
     *
     * @param size Size of packet.
     */
    void packetDropped(long size) {
        synchronized (mLock) {
            mDroppedBytes += size;
            mDroppedPackets++;
        }
    }

    int getUid() {
        return mUid;
    }

    int getLayerType() {
        return mLayerType;
    }

    int getLayerChannel() {
        return mLayerChannel;
    }

    int getLayerVersion() {
        return mLayerVersion;
    }

    long getTxBytes() {
        synchronized (mLock) {
            return mTxBytes;
        }
    }

    long getTxPackets() {
        synchronized (mLock) {
            return mTxPackets;
        }
    }

    long getRxBytes() {
        synchronized (mLock) {
            return mRxBytes;
        }
    }

    long getRxPackets() {
        synchronized (mLock) {
            return mRxPackets;
        }
    }

    long getDroppedBytes() {
        synchronized (mLock) {
            return mDroppedBytes;
        }
    }

    long getDroppedPackets() {
        synchronized (mLock) {
            return mDroppedPackets;
        }
    }
}

