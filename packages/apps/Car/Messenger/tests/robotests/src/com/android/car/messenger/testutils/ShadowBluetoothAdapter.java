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

package com.android.car.messenger.testutils;

import android.annotation.Nullable;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;
import org.robolectric.shadows.ShadowApplication;

import java.util.HashMap;
import java.util.Map;

@Implements(BluetoothAdapter.class)
public class ShadowBluetoothAdapter extends org.robolectric.shadows.ShadowBluetoothAdapter {

    private static boolean mBluetoothEnabled = true;
    private static Map<String, BluetoothDevice> mDeviceAddressToDeviceMap = new HashMap<>();

    @Nullable
    @Implementation
    protected static synchronized BluetoothAdapter getDefaultAdapter() {
        if (mBluetoothEnabled) {
            return (BluetoothAdapter) ShadowApplication.getInstance().getBluetoothAdapter();
        }
        return null;
    }

    public static void setBluetoothEnabled(boolean enabled) {
        mBluetoothEnabled = enabled;
    }

    public static void addRemoteDevice(BluetoothDevice device) {
        mDeviceAddressToDeviceMap.put(device.getAddress(), device);
    }

    @Implementation
    protected BluetoothDevice getRemoteDevice(String address) throws IllegalArgumentException {
        if (!mDeviceAddressToDeviceMap.containsKey(address)) {
            throw new IllegalArgumentException();
        }
        return mDeviceAddressToDeviceMap.get(address);
    }

    @Resetter
    public static void reset() {
        mBluetoothEnabled = true;
        mDeviceAddressToDeviceMap.clear();
    }
}
