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

package com.android.car.companiondevicesupport.activity;

import android.annotation.NonNull;
import android.annotation.Nullable;

import com.android.car.companiondevicesupport.api.external.AssociatedDevice;

/** Class that contains the details of an associated device. */
class AssociatedDeviceDetails {
    private final String mDeviceId;

    private final String mDeviceAddress;

    private final String mDeviceName;

    private final boolean mIsConnectionEnabled;

    private final boolean mIsConnected;

    AssociatedDeviceDetails(@NonNull AssociatedDevice device, boolean isConnected) {
        mDeviceId = device.getDeviceId();
        mDeviceAddress = device.getDeviceAddress();
        mDeviceName = device.getDeviceName();
        mIsConnectionEnabled = device.isConnectionEnabled();
        mIsConnected = isConnected;
    }

    /** Get the device id. */
    @NonNull
    String getDeviceId() {
        return mDeviceId;
    }

    /** Get the name of the associated device. */
    @Nullable
    String getDeviceName() {
        return mDeviceName;
    }

    /** Get the device address. */
    @NonNull
    String getDeviceAddress() {
        return mDeviceAddress;
    }

    /** {@code true} if the connection is enabled for the device. */
    boolean isConnectionEnabled() {
        return mIsConnectionEnabled;
    }

    /** {@code true} if the device is connected. */
    boolean isConnected() {
        return mIsConnected;
    }

    /** Get {@link AssociatedDevice}. */
    @NonNull
    AssociatedDevice getAssociatedDevice() {
        return new AssociatedDevice(mDeviceId, mDeviceAddress, mDeviceName, mIsConnectionEnabled);
    }
}
