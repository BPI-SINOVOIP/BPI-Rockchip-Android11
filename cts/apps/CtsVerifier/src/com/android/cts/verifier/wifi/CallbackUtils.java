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

package com.android.cts.verifier.wifi;

import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.util.Log;
import android.util.Pair;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Blocking callbacks for Wi-Fi and Connectivity Manager.
 */
public class CallbackUtils {
    private static final boolean DBG = true;
    private static final String TAG = "CallbackUtils";
    public static final int DEFAULT_CALLBACK_TIMEOUT_MS = 15_000;

    /**
     * Utility NetworkCallback- provides mechanism to block execution with the
     * waitForAttach method.
     */
    public static class NetworkCallback extends ConnectivityManager.NetworkCallback {
        private final int mCallbackTimeoutInMs;

        private CountDownLatch mOnAvailableBlocker = new CountDownLatch(1);
        private CountDownLatch mOnUnAvailableBlocker = new CountDownLatch(1);
        private CountDownLatch mOnLostBlocker = new CountDownLatch(1);
        // This is invoked multiple times, so initialize only when waitForCapabilitiesChanged() is
        // invoked.
        private CountDownLatch mOnCapabilitiesChangedBlocker = null;
        private Network mNetwork;
        private NetworkCapabilities mNetworkCapabilities;

        public NetworkCallback() {
            mCallbackTimeoutInMs = DEFAULT_CALLBACK_TIMEOUT_MS;
        }

        public NetworkCallback(int callbackTimeoutInMs) {
            mCallbackTimeoutInMs = callbackTimeoutInMs;
        }

        @Override
        public void onAvailable(Network network, NetworkCapabilities networkCapabilities,
                LinkProperties linkProperties, boolean isBlocked) {
            if (DBG) Log.v(TAG, "onAvailable");
            mNetwork = network;
            mNetworkCapabilities = networkCapabilities;
            mOnAvailableBlocker.countDown();
        }

        @Override
        public void onCapabilitiesChanged(Network network, NetworkCapabilities networkCapabilities) {
            if (DBG) Log.v(TAG, "onCapabilitiesChanged");
            mNetwork = network;
            mNetworkCapabilities = networkCapabilities;
            if (mOnCapabilitiesChangedBlocker != null) mOnCapabilitiesChangedBlocker.countDown();
        }

        @Override
        public void onUnavailable() {
            if (DBG) Log.v(TAG, "onUnavailable");
            mOnUnAvailableBlocker.countDown();
        }

        @Override
        public void onLost(Network network) {
            if (DBG) Log.v(TAG, "onLost");
            mNetwork = network;
            mOnLostBlocker.countDown();
        }

        public Network getNetwork() {
            return mNetwork;
        }

        public NetworkCapabilities getNetworkCapabilities() {
            return mNetworkCapabilities;
        }

        /**
         * Wait (blocks) for {@link #onAvailable(Network)} or timeout.
         *
         * @return A pair of values: whether the callback was invoked and the Network object
         * created when successful - null otherwise.
         */
        public Pair<Boolean, Network> waitForAvailable() throws InterruptedException {
            if (mOnAvailableBlocker.await(mCallbackTimeoutInMs, TimeUnit.MILLISECONDS)) {
                return Pair.create(true, mNetwork);
            }
            return Pair.create(false, null);
        }

        /**
         * Wait (blocks) for {@link #onUnavailable()} or timeout.
         *
         * @return true whether the callback was invoked.
         */
        public boolean waitForUnavailable() throws InterruptedException {
            return mOnUnAvailableBlocker.await(mCallbackTimeoutInMs, TimeUnit.MILLISECONDS);
        }

        /**
         * Wait (blocks) for {@link #onLost(Network)} or timeout.
         *
         * @return true whether the callback was invoked.
         */
        public boolean waitForLost() throws InterruptedException {
            return mOnLostBlocker.await(mCallbackTimeoutInMs, TimeUnit.MILLISECONDS);
        }

        /**
         * Wait (blocks) for {@link #onCapabilitiesChanged(Network, NetworkCapabilities)} or
         * timeout.
         *
         * @return true whether the callback was invoked.
         */
        public boolean waitForCapabilitiesChanged() throws InterruptedException {
            mOnCapabilitiesChangedBlocker = new CountDownLatch(1);
            return mOnCapabilitiesChangedBlocker.await(mCallbackTimeoutInMs, TimeUnit.MILLISECONDS);
        }
    }
}
