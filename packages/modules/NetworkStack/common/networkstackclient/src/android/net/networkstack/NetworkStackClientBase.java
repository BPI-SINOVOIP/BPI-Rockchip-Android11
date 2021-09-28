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

package android.net.networkstack;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.net.IIpMemoryStoreCallbacks;
import android.net.INetworkMonitorCallbacks;
import android.net.INetworkStackConnector;
import android.net.Network;
import android.net.dhcp.DhcpServingParamsParcel;
import android.net.dhcp.IDhcpServerCallbacks;
import android.net.ip.IIpClientCallbacks;
import android.os.RemoteException;

import com.android.internal.annotations.GuardedBy;

import java.util.ArrayList;
import java.util.function.Consumer;

/**
 * Utility class to obtain and communicate with the NetworkStack module.
 */
public abstract class NetworkStackClientBase {
    @NonNull
    @GuardedBy("mPendingNetStackRequests")
    private final ArrayList<Consumer<INetworkStackConnector>> mPendingNetStackRequests =
            new ArrayList<>();

    @Nullable
    @GuardedBy("mPendingNetStackRequests")
    private INetworkStackConnector mConnector;

    /**
     * Create a DHCP server according to the specified parameters.
     *
     * <p>The server will be returned asynchronously through the provided callbacks.
     */
    public void makeDhcpServer(final String ifName, final DhcpServingParamsParcel params,
            final IDhcpServerCallbacks cb) {
        requestConnector(connector -> {
            try {
                connector.makeDhcpServer(ifName, params, cb);
            } catch (RemoteException e) {
                throw new IllegalStateException("Could not create DhcpServer", e);
            }
        });
    }

    /**
     * Create an IpClient on the specified interface.
     *
     * <p>The IpClient will be returned asynchronously through the provided callbacks.
     */
    public void makeIpClient(String ifName, IIpClientCallbacks cb) {
        requestConnector(connector -> {
            try {
                connector.makeIpClient(ifName, cb);
            } catch (RemoteException e) {
                throw new IllegalStateException("Could not create IpClient", e);
            }
        });
    }

    /**
     * Create a NetworkMonitor.
     *
     * <p>The INetworkMonitor will be returned asynchronously through the provided callbacks.
     */
    public void makeNetworkMonitor(Network network, String name, INetworkMonitorCallbacks cb) {
        requestConnector(connector -> {
            try {
                connector.makeNetworkMonitor(network, name, cb);
            } catch (RemoteException e) {
                throw new IllegalStateException("Could not create NetworkMonitor", e);
            }
        });
    }

    /**
     * Get an instance of the IpMemoryStore.
     *
     * <p>The IpMemoryStore will be returned asynchronously through the provided callbacks.
     */
    public void fetchIpMemoryStore(IIpMemoryStoreCallbacks cb) {
        requestConnector(connector -> {
            try {
                connector.fetchIpMemoryStore(cb);
            } catch (RemoteException e) {
                throw new IllegalStateException("Could not fetch IpMemoryStore", e);
            }
        });
    }

    protected void requestConnector(@NonNull Consumer<INetworkStackConnector> request) {
        final INetworkStackConnector connector;
        synchronized (mPendingNetStackRequests) {
            connector = mConnector;
            if (connector == null) {
                mPendingNetStackRequests.add(request);
                return;
            }
        }

        request.accept(connector);
    }

    /**
     * Call this method once the NetworkStack is connected.
     *
     * <p>This method will cause pending oneway Binder calls for the NetworkStack to be processed on
     * the calling thread.
     */
    protected void onNetworkStackConnected(@NonNull INetworkStackConnector connector) {
        // Process the connector wait queue in order, including any items that are added
        // while processing.
        while (true) {
            final ArrayList<Consumer<INetworkStackConnector>> requests;
            synchronized (mPendingNetStackRequests) {
                requests = new ArrayList<>(mPendingNetStackRequests);
                mPendingNetStackRequests.clear();
            }

            for (Consumer<INetworkStackConnector> consumer : requests) {
                consumer.accept(connector);
            }

            synchronized (mPendingNetStackRequests) {
                if (mPendingNetStackRequests.size() == 0) {
                    // Once mConnector is non-null, no more tasks will be queued.
                    mConnector = connector;
                    return;
                }
            }
        }
    }

    /**
     * Used in subclasses for diagnostics (dumpsys) purposes.
     * @return How many requests for the network stack are currently pending.
     */
    protected int getQueueLength() {
        synchronized (mPendingNetStackRequests) {
            return mPendingNetStackRequests.size();
        }
    }
}
