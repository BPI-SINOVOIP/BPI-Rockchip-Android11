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

package com.android.car.dialer.bluetooth;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothHeadsetClient;
import android.bluetooth.BluetoothProfile;
import android.content.Context;

import androidx.annotation.Nullable;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

import com.android.car.dialer.log.L;

/**
 * Binds {@link BluetoothHeadsetClient profile}. It provides the connected {@link
 * BluetoothHeadsetClient profile object} and the connected state {@link LiveData}.
 */
public class BluetoothHeadsetClientProvider {
    private static final String TAG = "CD.BHCProvider";
    private static BluetoothHeadsetClientProvider sInstance;
    private final MutableLiveData<Boolean> mIsBluetoothHeadsetClientConnected;
    private BluetoothHeadsetClient mBluetoothHeadsetClient;

    /** Returns the singleton instance of the {@link BluetoothHeadsetClientProvider} */
    public static BluetoothHeadsetClientProvider singleton(Context context) {
        if (sInstance == null) {
            sInstance = new BluetoothHeadsetClientProvider(context);
        }
        return sInstance;
    }

    private BluetoothHeadsetClientProvider(Context context) {
        mIsBluetoothHeadsetClientConnected = new MutableLiveData<>();
        mIsBluetoothHeadsetClientConnected.observeForever(
                isConnected -> L.d(TAG, "BluetoothHeadsetClient is connected."));
        BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        if (bluetoothAdapter != null) {
            bluetoothAdapter.getProfileProxy(context, new BluetoothProfile.ServiceListener() {
                @Override
                public void onServiceConnected(int profile, BluetoothProfile proxy) {
                    // On bind or turning bluetooth on will trigger onServiceConnected.
                    if (profile == BluetoothProfile.HEADSET_CLIENT) {
                        mBluetoothHeadsetClient = (BluetoothHeadsetClient) proxy;
                        mIsBluetoothHeadsetClientConnected.setValue(true);
                    }
                }

                @Override
                public void onServiceDisconnected(int profile) {
                    // Turning bluetooth off will trigger onServiceDisconnected.
                    if (profile == BluetoothProfile.HEADSET_CLIENT) {
                        mBluetoothHeadsetClient = null;
                        mIsBluetoothHeadsetClientConnected.setValue(false);
                    }
                }
            }, BluetoothProfile.HEADSET_CLIENT);
        }
    }

    /**
     * Returns the {@link BluetoothHeadsetClient}. It might be null if bluetooth is not supported on
     * this device or the profile object hasn't been connected.
     */
    @Nullable
    public BluetoothHeadsetClient get() {
        return mBluetoothHeadsetClient;
    }

    /**
     * Returns the state {@link LiveData} if the {@link BluetoothHeadsetClient} has been connected.
     */
    public LiveData<Boolean> isBluetoothHeadsetClientConnected() {
        return mIsBluetoothHeadsetClientConnected;
    }
}
