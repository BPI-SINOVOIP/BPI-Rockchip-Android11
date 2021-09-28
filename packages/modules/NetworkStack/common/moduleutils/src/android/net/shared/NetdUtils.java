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

package android.net.shared;

import static android.net.RouteInfo.RTN_UNICAST;
import static android.system.OsConstants.EBUSY;

import android.net.INetd;
import android.net.IpPrefix;
import android.net.RouteInfo;
import android.net.TetherConfigParcel;
import android.os.RemoteException;
import android.os.ServiceSpecificException;
import android.os.SystemClock;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;

/**
 * Implements common operations on INetd
 * @hide
 */
public class NetdUtils {
    private static final String TAG = NetdUtils.class.getSimpleName();

    /** Start tethering. */
    public static void tetherStart(final INetd netd, final boolean usingLegacyDnsProxy,
            final String[] dhcpRange) throws RemoteException, ServiceSpecificException {
        final TetherConfigParcel config = new TetherConfigParcel();
        config.usingLegacyDnsProxy = usingLegacyDnsProxy;
        config.dhcpRanges = dhcpRange;
        netd.tetherStartWithConfiguration(config);
    }

    /** Setup interface for tethering. */
    public static void tetherInterface(final INetd netd, final String iface, final IpPrefix dest)
            throws RemoteException, ServiceSpecificException {
        tetherInterface(netd, iface, dest, 20 /* maxAttempts */, 50 /* pollingIntervalMs */);
    }

    /** Setup interface with configurable retries for tethering. */
    public static void tetherInterface(final INetd netd, final String iface, final IpPrefix dest,
            int maxAttempts, int pollingIntervalMs)
            throws RemoteException, ServiceSpecificException {
        netd.tetherInterfaceAdd(iface);
        networkAddInterface(netd, iface, maxAttempts, pollingIntervalMs);
        List<RouteInfo> routes = new ArrayList<>();
        routes.add(new RouteInfo(dest, null, iface, RTN_UNICAST));
        RouteUtils.addRoutesToLocalNetwork(netd, iface, routes);
    }

    /**
     * Retry Netd#networkAddInterface for EBUSY error code.
     * If the same interface (e.g., wlan0) is in client mode and then switches to tethered mode.
     * There can be a race where puts the interface into the local network but interface is still
     * in use in netd because the ConnectivityService thread hasn't processed the disconnect yet.
     * See b/158269544 for detail.
     */
    private static void networkAddInterface(final INetd netd, final String iface,
            int maxAttempts, int pollingIntervalMs)
            throws ServiceSpecificException, RemoteException {
        for (int i = 1; i <= maxAttempts; i++) {
            try {
                netd.networkAddInterface(INetd.LOCAL_NET_ID, iface);
                return;
            } catch (ServiceSpecificException e) {
                if (e.errorCode == EBUSY && i < maxAttempts) {
                    SystemClock.sleep(pollingIntervalMs);
                    continue;
                }

                Log.e(TAG, "Retry Netd#networkAddInterface failure: " + e);
                throw e;
            }
        }
    }

    /** Reset interface for tethering. */
    public static void untetherInterface(final INetd netd, String iface)
            throws RemoteException, ServiceSpecificException {
        try {
            netd.tetherInterfaceRemove(iface);
        } finally {
            netd.networkRemoveInterface(INetd.LOCAL_NET_ID, iface);
        }
    }
}
