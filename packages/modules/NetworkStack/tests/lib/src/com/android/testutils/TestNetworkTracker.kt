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

package com.android.testutils

import android.content.Context
import android.net.ConnectivityManager
import android.net.ConnectivityManager.NetworkCallback
import android.net.LinkAddress
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.StringNetworkSpecifier
import android.net.TestNetworkInterface
import android.net.TestNetworkManager
import android.os.Binder
import java.util.concurrent.CompletableFuture
import java.util.concurrent.TimeUnit

/**
 * Create a test network based on a TUN interface.
 *
 * This method will block until the test network is available. Requires
 * [android.Manifest.permission.CHANGE_NETWORK_STATE] and
 * [android.Manifest.permission.MANAGE_TEST_NETWORKS].
 */
fun initTestNetwork(context: Context, interfaceAddr: LinkAddress, setupTimeoutMs: Long = 10_000L):
        TestNetworkTracker {
    val tnm = context.getSystemService(TestNetworkManager::class.java)
    val iface = tnm.createTunInterface(arrayOf(interfaceAddr))
    return TestNetworkTracker(context, iface, tnm, setupTimeoutMs)
}

/**
 * Utility class to create and track test networks.
 *
 * This class is not thread-safe.
 */
class TestNetworkTracker internal constructor(
    val context: Context,
    val iface: TestNetworkInterface,
    tnm: TestNetworkManager,
    setupTimeoutMs: Long
) {
    private val cm = context.getSystemService(ConnectivityManager::class.java)
    private val binder = Binder()

    private val networkCallback: NetworkCallback
    val network: Network

    init {
        val networkFuture = CompletableFuture<Network>()
        val networkRequest = NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_TEST)
                // Test networks do not have NOT_VPN or TRUSTED capabilities by default
                .removeCapability(NetworkCapabilities.NET_CAPABILITY_NOT_VPN)
                .removeCapability(NetworkCapabilities.NET_CAPABILITY_TRUSTED)
                .setNetworkSpecifier(StringNetworkSpecifier(iface.interfaceName))
                .build()
        networkCallback = object : NetworkCallback() {
            override fun onAvailable(network: Network) {
                networkFuture.complete(network)
            }
        }
        cm.requestNetwork(networkRequest, networkCallback)

        try {
            tnm.setupTestNetwork(iface.interfaceName, binder)
            network = networkFuture.get(setupTimeoutMs, TimeUnit.MILLISECONDS)
        } catch (e: Throwable) {
            teardown()
            throw e
        }
    }

    fun teardown() {
        cm.unregisterNetworkCallback(networkCallback)
    }
}