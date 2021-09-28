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

import android.net.util.SocketUtils;

import androidx.annotation.NonNull;

import com.android.networkstack.apishim.common.SocketUtilsShim;

import java.net.SocketAddress;

/**
 * Implementation of SocketUtilsShim for API 29.
 */
public class SocketUtilsShimImpl implements SocketUtilsShim {
    protected SocketUtilsShimImpl() {}

    /**
     * Get a new instance of {@link SocketUtilsShim}.
     *
     * Use com.android.networkstack.apishim.SocketUtilsShim#newInstance()
     * (non-API29 version) instead, to use the correct shims depending on build SDK.
     */
    public static SocketUtilsShim newInstance() {
        return new SocketUtilsShimImpl();
    }

    @NonNull
    @Override
    public SocketAddress makePacketSocketAddress(
            int protocol, int ifIndex, @NonNull byte[] hwAddr) {
        // Not available for API <= 29: fallback to older behavior.
        return SocketUtils.makePacketSocketAddress(ifIndex, hwAddr);
    }
}
