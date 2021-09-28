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

package android.net.ipsec.ike.cts;

import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_VPN;
import static android.net.NetworkCapabilities.NET_CAPABILITY_TRUSTED;
import static android.net.NetworkCapabilities.TRANSPORT_TEST;

import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkRequest;
import android.net.TestNetworkManager;
import android.os.IBinder;
import android.os.RemoteException;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;

// TODO(b/148689509): Share this class with net CTS test (e.g. IpSecManagerTunnelTest)
public class TestNetworkUtils {
    private static final int TIMEOUT_MS = 500;

    /** Callback to receive requested test network. */
    public static class TestNetworkCallback extends ConnectivityManager.NetworkCallback {
        private final CompletableFuture<Network> futureNetwork = new CompletableFuture<>();

        @Override
        public void onAvailable(Network network) {
            futureNetwork.complete(network);
        }

        public Network getNetworkBlocking() throws Exception {
            return futureNetwork.get(TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }
    }

    /**
     * Set up test network.
     *
     * <p>Caller MUST have MANAGE_TEST_NETWORKS permission to use this method.
     *
     * @param connMgr ConnectivityManager to request network.
     * @param testNetworkMgr TestNetworkManager to set up test network.
     * @param ifname the name of the interface to be used for the Network LinkProperties.
     * @param binder a binder object guarding the lifecycle of this test network.
     * @return TestNetworkCallback to retrieve the test network.
     * @throws RemoteException if test network setup failed.
     * @see android.net.TestNetworkManager
     */
    public static TestNetworkCallback setupAndGetTestNetwork(
            ConnectivityManager connMgr,
            TestNetworkManager testNetworkMgr,
            String ifname,
            IBinder binder)
            throws RemoteException {
        NetworkRequest nr =
                new NetworkRequest.Builder()
                        .addTransportType(TRANSPORT_TEST)
                        .removeCapability(NET_CAPABILITY_TRUSTED)
                        .removeCapability(NET_CAPABILITY_NOT_VPN)
                        .setNetworkSpecifier(ifname)
                        .build();

        TestNetworkCallback cb = new TestNetworkCallback();
        connMgr.requestNetwork(nr, cb);

        // Setup the test network after network request is filed to prevent Network from being
        // reaped due to no requests matching it.
        testNetworkMgr.setupTestNetwork(ifname, binder);

        return cb;
    }
}
