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

package com.android.networkstack.metrics;

import android.net.util.NetworkStackUtils;
import android.net.util.Stopwatch;
import android.stats.connectivity.DhcpErrorCode;
import android.stats.connectivity.DhcpFeature;
import android.stats.connectivity.DisconnectCode;
import android.stats.connectivity.HostnameTransResult;

import java.util.HashSet;
import java.util.Set;

/**
 * Class to record the network IpProvisioning into statsd.
 * 1. Fill in NetworkIpProvisioningReported proto.
 * 2. Write the NetworkIpProvisioningReported proto into statsd.
 * 3. This class is not thread-safe, and should always be accessed from the same thread.
 * @hide
 */

public class IpProvisioningMetrics {
    private static final String TAG = IpProvisioningMetrics.class.getSimpleName();
    private final NetworkIpProvisioningReported.Builder mStatsBuilder =
            NetworkIpProvisioningReported.newBuilder();
    private final DhcpSession.Builder mDhcpSessionBuilder = DhcpSession.newBuilder();
    private final Stopwatch mIpv4Watch = new Stopwatch().start();
    private final Stopwatch mIpv6Watch = new Stopwatch().start();
    private final Stopwatch mWatch = new Stopwatch().start();
    private final Set<DhcpFeature> mDhcpFeatures = new HashSet<DhcpFeature>();

    // Define a maximum number of the DhcpErrorCode.
    public static final int MAX_DHCP_ERROR_COUNT = 20;

    /**
     *  reset this all metrics members
     */
    public void reset() {
        mStatsBuilder.clear();
        mDhcpSessionBuilder.clear();
        mDhcpFeatures.clear();
        mIpv4Watch.restart();
        mIpv6Watch.restart();
        mWatch.restart();
    }

    /**
     * Write the TransportType into mStatsBuilder.
     * TODO: implement this
     */
    public void setTransportType() {}

    /**
     * Write the IPv4Provisioned latency into mStatsBuilder.
     */
    public void setIPv4ProvisionedLatencyOnFirstTime(final boolean isIpv4Provisioned) {
        if (isIpv4Provisioned && !mStatsBuilder.hasIpv4LatencyMicros()) {
            mStatsBuilder.setIpv4LatencyMicros(NetworkStackUtils.saturatedCast(mIpv4Watch.stop()));
        }
    }

    /**
     * Write the IPv6Provisioned latency into mStatsBuilder.
     */
    public void setIPv6ProvisionedLatencyOnFirstTime(final boolean isIpv6Provisioned) {
        if (isIpv6Provisioned && !mStatsBuilder.hasIpv6LatencyMicros()) {
            mStatsBuilder.setIpv6LatencyMicros(NetworkStackUtils.saturatedCast(mIpv6Watch.stop()));
        }
    }

    /**
     * Write the DhcpFeature proto into mStatsBuilder.
     */
    public void setDhcpEnabledFeature(final DhcpFeature feature) {
        if (feature == DhcpFeature.DF_UNKNOWN) return;
        mDhcpFeatures.add(feature);
    }

    /**
     * Write the DHCPDISCOVER transmission count into DhcpSession.
     */
    public void incrementCountForDiscover() {
        mDhcpSessionBuilder.setDiscoverCount(mDhcpSessionBuilder.getDiscoverCount() + 1);
    }

    /**
     * Write the DHCPREQUEST transmission count into DhcpSession.
     */
    public void incrementCountForRequest() {
        mDhcpSessionBuilder.setRequestCount(mDhcpSessionBuilder.getRequestCount() + 1);
    }

    /**
     * Write the IPv4 address conflict count into DhcpSession.
     */
    public void incrementCountForIpConflict() {
        mDhcpSessionBuilder.setConflictCount(mDhcpSessionBuilder.getConflictCount() + 1);
    }

    /**
     * Write the hostname transliteration result into DhcpSession.
     */
    public void setHostnameTransinfo(final boolean isOptionEnabled, final boolean transSuccess) {
        mDhcpSessionBuilder.setHtResult(!isOptionEnabled ? HostnameTransResult.HTR_DISABLE :
                transSuccess ? HostnameTransResult.HTR_SUCCESS : HostnameTransResult.HTR_FAILURE);
    }

    private static DhcpErrorCode dhcpErrorFromNumberSafe(int number) {
        // See DhcpErrorCode.errorCodeWithOption
        // TODO: add a DhcpErrorCode method to extract the code;
        //       or replace legacy error codes with the new metrics.
        final DhcpErrorCode error = DhcpErrorCode.forNumber(number & 0xFFFF0000);
        if (error == null) return DhcpErrorCode.ET_UNKNOWN;
        return error;
    }

    /**
     * write the DHCP error code into DhcpSession.
     */
    public void addDhcpErrorCode(final int errorCode) {
        if (mDhcpSessionBuilder.getErrorCodeCount() >= MAX_DHCP_ERROR_COUNT) return;
        mDhcpSessionBuilder.addErrorCode(dhcpErrorFromNumberSafe(errorCode));
    }

    /**
     * Write the IP provision disconnect code into DhcpSession.
     */
    public void setDisconnectCode(final DisconnectCode disconnectCode) {
        if (mStatsBuilder.hasDisconnectCode()) return;
        mStatsBuilder.setDisconnectCode(disconnectCode);
    }

    /**
     * Write the NetworkIpProvisioningReported proto into statsd.
     */
    public NetworkIpProvisioningReported statsWrite() {
        if (!mWatch.isStarted()) return null;
        for (DhcpFeature feature : mDhcpFeatures) {
            mDhcpSessionBuilder.addUsedFeatures(feature);
        }
        mStatsBuilder.setDhcpSession(mDhcpSessionBuilder);
        mStatsBuilder.setProvisioningDurationMicros(mWatch.stop());
        mStatsBuilder.setRandomNumber((int) (Math.random() * 1000));
        final NetworkIpProvisioningReported Stats = mStatsBuilder.build();
        final byte[] DhcpSession = Stats.getDhcpSession().toByteArray();
        NetworkStackStatsLog.write(NetworkStackStatsLog.NETWORK_IP_PROVISIONING_REPORTED,
                Stats.getTransportType().getNumber(),
                Stats.getIpv4LatencyMicros(),
                Stats.getIpv6LatencyMicros(),
                Stats.getProvisioningDurationMicros(),
                Stats.getDisconnectCode().getNumber(),
                DhcpSession,
                Stats.getRandomNumber());
        mWatch.reset();
        return Stats;
    }
}
