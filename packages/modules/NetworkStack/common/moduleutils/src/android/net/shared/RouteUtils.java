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

import static android.net.RouteInfo.RTN_THROW;
import static android.net.RouteInfo.RTN_UNICAST;
import static android.net.RouteInfo.RTN_UNREACHABLE;

import android.net.INetd;
import android.net.IpPrefix;
import android.net.RouteInfo;
import android.os.RemoteException;
import android.os.ServiceSpecificException;

import java.util.List;

/** @hide*/
public class RouteUtils {

    /** Used to modify the specified route. */
    public enum ModifyOperation {
        ADD,
        REMOVE,
    }

    private static String findNextHop(final RouteInfo route) {
        final String nextHop;
        switch (route.getType()) {
            case RTN_UNICAST:
                if (route.hasGateway()) {
                    nextHop = route.getGateway().getHostAddress();
                } else {
                    nextHop = INetd.NEXTHOP_NONE;
                }
                break;
            case RTN_UNREACHABLE:
                nextHop = INetd.NEXTHOP_UNREACHABLE;
                break;
            case RTN_THROW:
                nextHop = INetd.NEXTHOP_THROW;
                break;
            default:
                nextHop = INetd.NEXTHOP_NONE;
                break;
        }
        return nextHop;
    }

    /** Add |routes| to local network. */
    public static void addRoutesToLocalNetwork(final INetd netd, final String iface,
            final List<RouteInfo> routes) {

        for (RouteInfo route : routes) {
            if (!route.isDefaultRoute()) {
                modifyRoute(netd, ModifyOperation.ADD, INetd.LOCAL_NET_ID, route);
            }
        }

        // IPv6 link local should be activated always.
        modifyRoute(netd, ModifyOperation.ADD, INetd.LOCAL_NET_ID,
                new RouteInfo(new IpPrefix("fe80::/64"), null, iface, RTN_UNICAST));
    }

    /** Remove routes from local network. */
    public static int removeRoutesFromLocalNetwork(final INetd netd, final List<RouteInfo> routes) {
        int failures = 0;

        for (RouteInfo route : routes) {
            try {
                modifyRoute(netd, ModifyOperation.REMOVE, INetd.LOCAL_NET_ID, route);
            } catch (IllegalStateException e) {
                failures++;
            }
        }

        return failures;
    }

    /** Add or remove |route|. */
    public static void modifyRoute(final INetd netd, final ModifyOperation op, final int netId,
            final RouteInfo route) {
        final String ifName = route.getInterface();
        final String dst = route.getDestination().toString();
        final String nextHop = findNextHop(route);

        try {
            switch(op) {
                case ADD:
                    netd.networkAddRoute(netId, ifName, dst, nextHop);
                    break;
                case REMOVE:
                    netd.networkRemoveRoute(netId, ifName, dst, nextHop);
                    break;
                default:
                    throw new IllegalStateException("Unsupported modify operation:" + op);
            }
        } catch (RemoteException | ServiceSpecificException e) {
            throw new IllegalStateException(e);
        }
    }

}
