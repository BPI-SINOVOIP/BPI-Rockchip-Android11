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

package com.android.car.vms;

import android.annotation.Nullable;
import android.util.ArrayMap;

import com.android.internal.annotations.GuardedBy;

import java.util.ArrayList;
import java.util.Arrays;

class VmsProviderInfoStore {
    private final Object mLock = new Object();
    @GuardedBy("mLock")
    private final ArrayMap<InfoWrapper, Integer> mProvidersIds = new ArrayMap<>();
    @GuardedBy("mLock")
    private final ArrayList<InfoWrapper> mProvidersInfo = new ArrayList<>();

    private static class InfoWrapper {
        private final byte[] mInfo;

        InfoWrapper(byte[] info) {
            mInfo = info;
        }

        public byte[] getInfo() {
            return mInfo.clone();
        }

        @Override
        public boolean equals(Object o) {
            if (!(o instanceof InfoWrapper)) {
                return false;
            }
            InfoWrapper p = (InfoWrapper) o;
            return Arrays.equals(this.mInfo, p.mInfo);
        }

        @Override
        public int hashCode() {
            return Arrays.hashCode(mInfo);
        }
    }

    /**
     * Retrieves the provider ID for the given provider information. If the provider information
     * has not previously been seen, it will be assigned a new provider ID.
     *
     * @param providerInfo Provider information to query or register.
     * @return Provider ID for the given provider information.
     */
    int getProviderId(byte[] providerInfo) {
        Integer providerId;
        InfoWrapper wrappedProviderInfo = new InfoWrapper(providerInfo);
        synchronized (mLock) {
            // Check if provider is already registered
            providerId = mProvidersIds.get(wrappedProviderInfo);
            if (providerId == null) {
                // Add the new provider and assign it the next ID
                mProvidersInfo.add(wrappedProviderInfo);
                providerId = mProvidersInfo.size();
                mProvidersIds.put(wrappedProviderInfo, providerId);
            }
        }
        return providerId;
    }

    /**
     * Returns the provider info associated with the given provider ID.
     * @param providerId Provider ID to query.
     * @return Provider info associated with the ID, or null if provider ID is unknown.
     */
    @Nullable
    byte[] getProviderInfo(int providerId) {
        synchronized (mLock) {
            return providerId < 1 || providerId > mProvidersInfo.size()
                    ? null
                    : mProvidersInfo.get(providerId - 1).getInfo();
        }
    }
}
