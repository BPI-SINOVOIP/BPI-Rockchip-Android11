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

package com.android.car.dialer.livedata;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadsetClient;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import androidx.lifecycle.LiveData;
import androidx.lifecycle.MediatorLiveData;

import com.android.car.dialer.bluetooth.BluetoothHeadsetClientProvider;

import java.util.Collections;
import java.util.List;

/** {@link LiveData} that monitors the hfp connected devices. */
public class HfpDeviceListLiveData extends MediatorLiveData<List<BluetoothDevice>> {
    private final Context mContext;
    private final BluetoothAdapter mBluetoothAdapter;
    private final BluetoothHeadsetClientProvider mBluetoothHeadsetClientProvider;
    private final IntentFilter mIntentFilter;

    private BroadcastReceiver mBluetoothStateReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            update();
        }
    };

    /** Creates a new {@link HfpDeviceListLiveData}. Call on main thread. */
    public HfpDeviceListLiveData(Context context) {
        mContext = context;

        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        mBluetoothHeadsetClientProvider = BluetoothHeadsetClientProvider.singleton(context);
        addSource(mBluetoothHeadsetClientProvider.isBluetoothHeadsetClientConnected(),
                isConnected -> update());

        mIntentFilter = new IntentFilter();
        mIntentFilter.addAction(BluetoothHeadsetClient.ACTION_CONNECTION_STATE_CHANGED);
    }

    @Override
    protected void onActive() {
        super.onActive();
        if (mBluetoothAdapter != null) {
            update();
            mContext.registerReceiver(mBluetoothStateReceiver, mIntentFilter);
        }
    }

    @Override
    protected void onInactive() {
        if (mBluetoothAdapter != null) {
            mContext.unregisterReceiver(mBluetoothStateReceiver);
        }
        super.onInactive();
    }

    private void update() {
        if (mBluetoothHeadsetClientProvider.get() != null) {
            setValue(mBluetoothHeadsetClientProvider.get().getConnectedDevices());
        } else {
            setValue(Collections.emptyList());
        }
    }
}
