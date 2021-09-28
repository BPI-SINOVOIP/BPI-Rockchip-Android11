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

package android.net.dhcp;

import static android.net.shared.IpConfigurationParcelableUtil.parcelAddress;
import static android.net.shared.IpConfigurationParcelableUtil.unparcelAddress;

import android.net.DhcpResults;
import android.net.DhcpResultsParcelable;

import androidx.annotation.Nullable;

import java.net.Inet4Address;

/**
 * A utility class to convert DhcpResults to DhcpResultsParcelable.
 */
public final class DhcpResultsParcelableUtil {
    /**
     * Convert DhcpResults to a DhcpResultsParcelable.
     */
    public static DhcpResultsParcelable toStableParcelable(@Nullable DhcpResults results) {
        if (results == null) return null;
        final DhcpResultsParcelable p = new DhcpResultsParcelable();
        p.baseConfiguration = results.toStaticIpConfiguration();
        p.leaseDuration = results.leaseDuration;
        p.mtu = results.mtu;
        p.serverAddress = parcelAddress(results.serverAddress);
        p.vendorInfo = results.vendorInfo;
        p.serverHostName = results.serverHostName;
        p.captivePortalApiUrl = results.captivePortalApiUrl;
        return p;
    }

    /**
     * Convert a DhcpResultsParcelable to DhcpResults.
     */
    public static DhcpResults fromStableParcelable(@Nullable DhcpResultsParcelable p) {
        if (p == null) return null;
        final DhcpResults results = new DhcpResults(p.baseConfiguration);
        results.leaseDuration = p.leaseDuration;
        results.mtu = p.mtu;
        results.serverAddress = (Inet4Address) unparcelAddress(p.serverAddress);
        results.vendorInfo = p.vendorInfo;
        results.serverHostName = p.serverHostName;
        results.captivePortalApiUrl = p.captivePortalApiUrl;
        return results;
    }
}
