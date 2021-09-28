/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.car.messenger.common;

import java.util.Map;
import java.util.Objects;

/**
 * A composite key used for {@link Map} lookups, using two strings for
 * checking equality and hashing.
 */
public abstract class CompositeKey {
    private final String mDeviceId;
    private final String mSubKey;

    protected CompositeKey(String deviceId, String subKey) {
        mDeviceId = deviceId;
        mSubKey = subKey;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }

        if (!(o instanceof CompositeKey)) {
            return false;
        }

        CompositeKey that = (CompositeKey) o;
        return Objects.equals(mDeviceId, that.mDeviceId)
                && Objects.equals(mSubKey, that.mSubKey);
    }

    /**
     * Returns true if the device address of this composite key equals {@code deviceId}.
     *
     * @param deviceId the device address which is compared to this key's device address
     * @return true if the device addresses match
     */
    public boolean matches(String deviceId) {
        return mDeviceId.equals(deviceId);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mDeviceId, mSubKey);
    }

    @Override
    public String toString() {
        return String.format("%s, deviceId: %s, subKey: %s",
                getClass().getSimpleName(), mDeviceId, mSubKey);
    }

    /** Returns this composite key's device address. */
    public String getDeviceId() {
        return mDeviceId;
    }

    /** Returns this composite key's sub key. */
    public String getSubKey() {
        return mSubKey;
    }
}
