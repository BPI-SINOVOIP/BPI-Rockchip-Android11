/*
 * Copyright (C) 2016 The Android Open Source Project
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

import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.Observer;
import androidx.lifecycle.Transformations;

import com.android.car.dialer.Constants;
import com.android.car.dialer.livedata.BluetoothPairListLiveData;
import com.android.car.dialer.livedata.BluetoothStateLiveData;
import com.android.car.dialer.livedata.HfpDeviceListLiveData;
import com.android.car.dialer.log.L;

import java.util.List;

/**
 * Class that responsible for getting status of bluetooth connections.
 */
public class UiBluetoothMonitor {
    private static final String TAG = "CD.BtMonitor";

    private static UiBluetoothMonitor sUiBluetoothMonitor;

    private final Context mContext;
    private final TelecomManager mTelecomManager;

    private BluetoothPairListLiveData mPairListLiveData;
    private BluetoothStateLiveData mBluetoothStateLiveData;
    private HfpDeviceListLiveData mHfpDeviceListLiveData;

    private Observer mPairListObserver;
    private Observer mBluetoothStateObserver;
    private Observer<List<BluetoothDevice>> mHfpDeviceListObserver;

    /**
     * Initialized a globally accessible {@link UiBluetoothMonitor} which can be retrieved by {@link
     * #get}.
     *
     * @param applicationContext Application context.
     */
    public static UiBluetoothMonitor init(Context applicationContext) {
        if (sUiBluetoothMonitor == null) {
            sUiBluetoothMonitor = new UiBluetoothMonitor(applicationContext);
        }

        return get();
    }

    /**
     * Gets the global {@link UiBluetoothMonitor} instance. Make sure {@link #init(Context)} is
     * called before calling this method.
     */
    public static UiBluetoothMonitor get() {
        if (sUiBluetoothMonitor == null) {
            throw new IllegalStateException(
                    "Call UiBluetoothMonitor.init(Context) before calling this function");
        }
        return sUiBluetoothMonitor;
    }

    private UiBluetoothMonitor(Context applicationContext) {
        mContext = applicationContext;
        mTelecomManager = mContext.getSystemService(TelecomManager.class);

        mPairListLiveData = new BluetoothPairListLiveData(mContext);
        mBluetoothStateLiveData = new BluetoothStateLiveData(mContext);
        mHfpDeviceListLiveData = new HfpDeviceListLiveData(mContext);

        mPairListObserver = o -> L.i(TAG, "PairList is updated");
        mBluetoothStateObserver = o -> L.i(TAG, "BluetoothState is updated");
        mHfpDeviceListObserver = deviceList -> {
            L.i(TAG, "HfpDeviceList is updated");
            BluetoothDevice bluetoothDevice =
                    deviceList == null || deviceList.isEmpty() ? null : deviceList.get(0);
            PhoneAccountHandle phoneAccountHandle = getPhoneAccountHandleForDevice(bluetoothDevice);
            mTelecomManager.setUserSelectedOutgoingPhoneAccount(phoneAccountHandle);
        };

        mPairListLiveData.observeForever(mPairListObserver);
        mBluetoothStateLiveData.observeForever(mBluetoothStateObserver);
        mHfpDeviceListLiveData.observeForever(mHfpDeviceListObserver);
    }

    /**
     * Stops the {@link UiBluetoothMonitor}. Call this function when Dialer goes to background.
     * {@link #get()} won't return a valid {@link UiBluetoothMonitor} after calling this function.
     */
    public void tearDown() {
        removeObserver(mPairListLiveData, mPairListObserver);
        removeObserver(mBluetoothStateLiveData, mBluetoothStateObserver);
        removeObserver(mHfpDeviceListLiveData, mHfpDeviceListObserver);

        sUiBluetoothMonitor = null;
    }

    /**
     * Returns a LiveData which monitors the paired device list changes.
     */
    public BluetoothPairListLiveData getPairListLiveData() {
        return mPairListLiveData;
    }

    /**
     * Returns a LiveData which monitors the Bluetooth state changes.
     */
    public BluetoothStateLiveData getBluetoothStateLiveData() {
        return mBluetoothStateLiveData;
    }

    /**
     * Returns a SingleLiveEvent which monitors whether to refresh Dialer.
     */
    public HfpDeviceListLiveData getHfpDeviceListLiveData() {
        return mHfpDeviceListLiveData;
    }

    /**
     * Returns a LiveData which monitors the first HFP Bluetooth device on the connected device
     * list.
     */
    public LiveData<BluetoothDevice> getFirstHfpConnectedDevice() {
        return Transformations.map(mHfpDeviceListLiveData, (devices) ->
                devices != null && !devices.isEmpty()
                        ? devices.get(0)
                        : null);
    }

    /** Returns a {@link LiveData} which monitors if there are any connected HFP devices. */
    public LiveData<Boolean> hasHfpDeviceConnected() {
        return Transformations.map(mHfpDeviceListLiveData,
                devices -> devices != null && !devices.isEmpty());
    }

    private void removeObserver(LiveData liveData, Observer observer) {
        if (liveData != null && liveData.hasObservers()) {
            liveData.removeObserver(observer);
        }
    }

    private PhoneAccountHandle getPhoneAccountHandleForDevice(
            @Nullable BluetoothDevice bluetoothDevice) {
        if (bluetoothDevice == null) {
            return null;
        }

        List<PhoneAccountHandle> phoneAccountHandleList =
                mTelecomManager.getCallCapablePhoneAccounts();
        for (PhoneAccountHandle phoneAccountHandle : phoneAccountHandleList) {
            if (Constants.HFP_CLIENT_CONNECTION_SERVICE_CLASS_NAME.equals(
                    phoneAccountHandle.getComponentName().getClassName())) {
                if (TextUtils.equals(phoneAccountHandle.getId(), bluetoothDevice.getAddress())) {
                    return phoneAccountHandle;
                }
            }
        }
        return null;
    }
}
