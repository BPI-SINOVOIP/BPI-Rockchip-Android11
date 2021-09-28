/**
 * Copyright (c) 2020, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing perNmissions and
 * limitations under the License.
 */

package android.net.dhcp;

import android.net.IpPrefix;
import android.net.dhcp.DhcpLeaseParcelable;

oneway interface IDhcpEventCallbacks {
    /**
     * Called when a lease is committed or released on the DHCP server.
     *
     * <p>This only reports lease changes after assigning a lease, or after releasing a lease
     * following a DHCPRELEASE: this callback will not be fired when a lease just expires.
     * @param newLeases The new list of leases tracked by the server.
     */
    void onLeasesChanged(in List<DhcpLeaseParcelable> newLeases);

    /**
     * Called when DHCP server receives DHCPDECLINE message and only if a new IPv4 address prefix
     * (e.g. a different subnet prefix) is requested.
     *
     * <p>When this callback is called, IpServer must call IDhcpServer#updateParams with a new
     * prefix, as processing of DHCP packets should be paused until the new prefix and route
     * configuration on IpServer is completed.
     * @param currentPrefix The current prefix parameter serving on DHCP server.
     */
    void onNewPrefixRequest(in IpPrefix currentPrefix);
}
