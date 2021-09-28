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

package com.android.networkstack.apishim.api29;

import android.net.Network;

import androidx.annotation.NonNull;

import com.android.networkstack.apishim.common.NetworkShim;
import com.android.networkstack.apishim.common.UnsupportedApiLevelException;
/**
 * Implementation of NetworkShim for API 29.
 */
public class NetworkShimImpl implements NetworkShim {
    @NonNull
    protected final Network mNetwork;

    protected NetworkShimImpl(@NonNull Network network) {
        mNetwork = network;
    }

    /**
     * Get a new instance of {@link NetworkShim}.
     *
     * Use com.android.networkstack.apishim.NetworkShimImpl#newInstance()
     * (non-API29 version) instead, to use the correct shims depending on build SDK.
     */
    public static NetworkShim newInstance(@NonNull Network network) {
        return new NetworkShimImpl(network);
    }

    /**
     * Get the netId of the network.
     * @throws UnsupportedApiLevelException if API is not available in this API level.
     */
    @Override
    public int getNetId() throws UnsupportedApiLevelException {
        // Not supported for API 29.
        throw new UnsupportedApiLevelException("Not supported in API 29.");
    }
}
