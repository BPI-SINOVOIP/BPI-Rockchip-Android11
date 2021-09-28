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
package com.android.car.trust;

import static android.bluetooth.BluetoothProfile.GATT_SERVER;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.util.Log;

import com.android.car.Utils;

import java.util.UUID;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * A generic class that manages BLE peripheral operations like start/stop advertising, notifying
 * connects/disconnects and reading/writing values to GATT characteristics.
 *
 * @deprecated Enrolling a trusted device through car service is no longer a supported feature and
 * these APIs will be removed in the next Android release.
 */
@Deprecated
public class BlePeripheralManager {
    private static final String TAG = BlePeripheralManager.class.getSimpleName();

    private static final int BLE_RETRY_LIMIT = 5;
    private static final int BLE_RETRY_INTERVAL_MS = 1000;

    private static final int GATT_SERVER_RETRY_LIMIT = 20;
    private static final int GATT_SERVER_RETRY_DELAY_MS = 200;

    // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth
    // .service.generic_access.xml
    private static final UUID GENERIC_ACCESS_PROFILE_UUID =
            UUID.fromString("00001800-0000-1000-8000-00805f9b34fb");
    //https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth
    // .characteristic.gap.device_name.xml
    private static final UUID DEVICE_NAME_UUID =
            UUID.fromString("00002a00-0000-1000-8000-00805f9b34fb");

    private final Handler mHandler = new Handler();

    private final Context mContext;
    private final CopyOnWriteArrayList<Callback> mCallbacks = new CopyOnWriteArrayList<>();
    private final CopyOnWriteArrayList<OnCharacteristicWriteListener> mWriteListeners =
            new CopyOnWriteArrayList<>();
    private final CopyOnWriteArrayList<OnCharacteristicReadListener> mReadListeners =
            new CopyOnWriteArrayList<>();

    private int mMtuSize = 20;

    private BluetoothManager mBluetoothManager;
    private BluetoothLeAdvertiser mAdvertiser;
    private BluetoothGattServer mGattServer;
    private BluetoothGatt mBluetoothGatt;
    private int mAdvertiserStartCount;
    private int mGattServerRetryStartCount;
    private BluetoothGattService mBluetoothGattService;
    private AdvertiseCallback mAdvertiseCallback;
    private AdvertiseData mAdvertiseData;


    BlePeripheralManager(Context context) {
        mContext = context;
    }

    /**
     * Registers the given callback to be notified of various events within the
     * {@link BlePeripheralManager}.
     *
     * @param callback The callback to be notified.
     */
    void registerCallback(@NonNull Callback callback) {
        mCallbacks.add(callback);
    }

    /**
     * Unregisters a previously registered callback.
     *
     * @param callback The callback to unregister.
     */
    void unregisterCallback(@NonNull Callback callback) {
        mCallbacks.remove(callback);
    }

    /**
     * Adds a listener to be notified of a write to characteristics.
     *
     * @param listener The listener to notify.
     */
    void addOnCharacteristicWriteListener(@NonNull OnCharacteristicWriteListener listener) {
        mWriteListeners.add(listener);
    }

    /**
     * Removes the given listener from being notified of characteristic writes.
     *
     * @param listener The listener to remove.
     */
    void removeOnCharacteristicWriteListener(@NonNull OnCharacteristicWriteListener listener) {
        mWriteListeners.remove(listener);
    }

    /**
     * Adds a listener to be notified of reads to characteristics.
     *
     * @param listener The listener to notify.
     */
    void addOnCharacteristicReadListener(@NonNull OnCharacteristicReadListener listener) {
        mReadListeners.add(listener);
    }

    /**
     * Removes the given listener from being notified of characteristic reads.
     *
     * @param listener The listener to remove.
     */
    void removeOnCharacteristicReadistener(@NonNull OnCharacteristicReadListener listener) {
        mReadListeners.remove(listener);
    }

    /**
     * Returns the current MTU size.
     *
     * @return The size of the MTU in bytes.
     */
    int getMtuSize() {
        return mMtuSize;
    }

    /**
     * Starts the GATT server with the given {@link BluetoothGattService} and begins
     * advertising.
     *
     * <p>It is possible that BLE service is still in TURNING_ON state when this method is invoked.
     * Therefore, several retries will be made to ensure advertising is started.
     *
     * @param service           {@link BluetoothGattService} that will be discovered by clients
     * @param data              {@link AdvertiseData} data to advertise
     * @param advertiseCallback {@link AdvertiseCallback} callback for advertiser
     */
    void startAdvertising(BluetoothGattService service, AdvertiseData data,
            AdvertiseCallback advertiseCallback) {
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "startAdvertising: " + service.getUuid().toString());
        }
        if (!mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) {
            Log.e(TAG, "Attempted start advertising, but system does not support BLE. "
                    + "Ignoring");
            return;
        }

        mBluetoothGattService = service;
        mAdvertiseCallback = advertiseCallback;
        mAdvertiseData = data;
        mGattServerRetryStartCount = 0;
        mBluetoothManager = (BluetoothManager) mContext.getSystemService(
                Context.BLUETOOTH_SERVICE);
        openGattServer();
    }

    /**
     * Stops the GATT server from advertising.
     *
     * @param advertiseCallback The callback that is associated with the advertisement.
     */
    void stopAdvertising(AdvertiseCallback advertiseCallback) {
        if (mAdvertiser != null) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "stopAdvertising: ");
            }
            mAdvertiser.stopAdvertising(advertiseCallback);
        }
    }

    /**
     * Notifies the characteristic change via {@link BluetoothGattServer}
     */
    void notifyCharacteristicChanged(@NonNull BluetoothDevice device,
            @NonNull BluetoothGattCharacteristic characteristic, boolean confirm) {
        if (mGattServer == null) {
            return;
        }

        boolean result = mGattServer.notifyCharacteristicChanged(device, characteristic, confirm);

        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "notifyCharacteristicChanged succeeded: " + result);
        }
    }

    /**
     * Connect the Gatt server of the remote device to retrieve device name.
     */
    final void retrieveDeviceName(BluetoothDevice device) {
        mBluetoothGatt = device.connectGatt(mContext, false, mGattCallback);
    }

    /**
     * Returns the currently opened GATT server within this manager.
     *
     * @return An opened GATT server or {@code null} if none have been opened.
     */
    @Nullable
    BluetoothGattServer getGattServer() {
        return mGattServer;
    }

    /**
     * Cleans up the BLE GATT server state.
     */
    void cleanup() {
        // Stops the advertiser, scanner and GATT server. This needs to be done to avoid leaks.
        if (mAdvertiser != null) {
            mAdvertiser.cleanup();
        }

        if (mGattServer != null) {
            mGattServer.clearServices();
            try {
                for (BluetoothDevice d : mBluetoothManager.getConnectedDevices(GATT_SERVER)) {
                    mGattServer.cancelConnection(d);
                }
            } catch (UnsupportedOperationException e) {
                Log.e(TAG, "Error getting connected devices", e);
            } finally {
                stopGattServer();
            }
        }
    }

    /**
     * Close the GATT Server
     */
    void stopGattServer() {
        if (mGattServer == null) {
            return;
        }
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "stopGattServer");
        }
        if (mBluetoothGatt != null) {
            mBluetoothGatt.disconnect();
        }
        mGattServer.close();
        mGattServer = null;
    }

    private void openGattServer() {
        // Only open one Gatt server.
        if (mGattServer != null) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Gatt Server created, retry count: " + mGattServerRetryStartCount);
            }
            mGattServer.clearServices();
            mGattServer.addService(mBluetoothGattService);
            AdvertiseSettings settings = new AdvertiseSettings.Builder()
                    .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
                    .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_HIGH)
                    .setConnectable(true)
                    .build();
            mAdvertiserStartCount = 0;
            startAdvertisingInternally(settings, mAdvertiseData, mAdvertiseCallback);
            mGattServerRetryStartCount = 0;
        } else if (mGattServerRetryStartCount < GATT_SERVER_RETRY_LIMIT) {
            mGattServer = mBluetoothManager.openGattServer(mContext, mGattServerCallback);
            mGattServerRetryStartCount++;
            mHandler.postDelayed(() -> openGattServer(), GATT_SERVER_RETRY_DELAY_MS);
        } else {
            Log.e(TAG, "Gatt server not created - exceeded retry limit.");
        }
    }

    private void startAdvertisingInternally(AdvertiseSettings settings, AdvertiseData data,
            AdvertiseCallback advertiseCallback) {
        if (BluetoothAdapter.getDefaultAdapter() != null) {
            mAdvertiser = BluetoothAdapter.getDefaultAdapter().getBluetoothLeAdvertiser();
        }

        if (mAdvertiser != null) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Advertiser created, retry count: " + mAdvertiserStartCount);
            }
            mAdvertiser.startAdvertising(settings, data, advertiseCallback);
            mAdvertiserStartCount = 0;
        } else if (mAdvertiserStartCount < BLE_RETRY_LIMIT) {
            mHandler.postDelayed(
                    () -> startAdvertisingInternally(settings, data, advertiseCallback),
                    BLE_RETRY_INTERVAL_MS);
            mAdvertiserStartCount += 1;
        } else {
            Log.e(TAG, "Cannot start BLE Advertisement.  BT Adapter: "
                    + BluetoothAdapter.getDefaultAdapter() + " Advertise Retry count: "
                    + mAdvertiserStartCount);
        }
    }

    private final BluetoothGattServerCallback mGattServerCallback =
            new BluetoothGattServerCallback() {
                @Override
                public void onConnectionStateChange(BluetoothDevice device, int status,
                        int newState) {
                    if (Log.isLoggable(TAG, Log.DEBUG)) {
                        Log.d(TAG, "BLE Connection State Change: " + newState);
                    }
                    switch (newState) {
                        case BluetoothProfile.STATE_CONNECTED:
                            for (Callback callback : mCallbacks) {
                                callback.onRemoteDeviceConnected(device);
                            }
                            break;
                        case BluetoothProfile.STATE_DISCONNECTED:
                            for (Callback callback : mCallbacks) {
                                callback.onRemoteDeviceDisconnected(device);
                            }
                            break;
                        default:
                            Log.w(TAG,
                                    "Connection state not connecting or disconnecting; ignoring: "
                                            + newState);
                    }
                }

                @Override
                public void onServiceAdded(int status, BluetoothGattService service) {
                    if (Log.isLoggable(TAG, Log.DEBUG)) {
                        Log.d(TAG,
                                "Service added status: " + status + " uuid: " + service.getUuid());
                    }
                }

                @Override
                public void onCharacteristicReadRequest(BluetoothDevice device, int requestId,
                        int offset, BluetoothGattCharacteristic characteristic) {
                    if (Log.isLoggable(TAG, Log.DEBUG)) {
                        Log.d(TAG, "Read request for characteristic: " + characteristic.getUuid());
                    }

                    boolean isSuccessful = mGattServer.sendResponse(device, requestId,
                            BluetoothGatt.GATT_SUCCESS, offset, characteristic.getValue());

                    if (isSuccessful) {
                        for (OnCharacteristicReadListener listener : mReadListeners) {
                            listener.onCharacteristicRead(device, characteristic);
                        }
                    } else {
                        stopGattServer();
                        for (Callback callback : mCallbacks) {
                            callback.onRemoteDeviceDisconnected(device);
                        }
                    }
                }

                @Override
                public void onCharacteristicWriteRequest(BluetoothDevice device, int requestId,
                        BluetoothGattCharacteristic characteristic, boolean preparedWrite,
                        boolean responseNeeded, int offset, byte[] value) {
                    if (Log.isLoggable(TAG, Log.DEBUG)) {
                        Log.d(TAG, "Write request for characteristic: " + characteristic.getUuid()
                                + "value: " + Utils.byteArrayToHexString(value));
                    }

                    mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS,
                            offset, value);

                    for (OnCharacteristicWriteListener listener : mWriteListeners) {
                        listener.onCharacteristicWrite(device, characteristic, value);
                    }
                }

                @Override
                public void onDescriptorWriteRequest(BluetoothDevice device, int requestId,
                        BluetoothGattDescriptor descriptor, boolean preparedWrite,
                        boolean responseNeeded, int offset, byte[] value) {
                    if (Log.isLoggable(TAG, Log.DEBUG)) {
                        Log.d(TAG, "Write request for descriptor: " + descriptor.getUuid()
                                + "; value: " + Utils.byteArrayToHexString(value));
                    }

                    mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS,
                            offset, value);
                }

                @Override
                public void onMtuChanged(BluetoothDevice device, int mtu) {
                    if (Log.isLoggable(TAG, Log.DEBUG)) {
                        Log.d(TAG, "onMtuChanged: " + mtu + " for device " + device.getAddress());
                    }

                    mMtuSize = mtu;

                    for (Callback callback : mCallbacks) {
                        callback.onMtuSizeChanged(mtu);
                    }
                }

            };

    private final BluetoothGattCallback mGattCallback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Gatt Connection State Change: " + newState);
            }
            switch (newState) {
                case BluetoothProfile.STATE_CONNECTED:
                    if (Log.isLoggable(TAG, Log.DEBUG)) {
                        Log.d(TAG, "Gatt connected");
                    }
                    mBluetoothGatt.discoverServices();
                    break;
                case BluetoothProfile.STATE_DISCONNECTED:
                    if (Log.isLoggable(TAG, Log.DEBUG)) {
                        Log.d(TAG, "Gatt Disconnected");
                    }
                    break;
                default:
                    if (Log.isLoggable(TAG, Log.DEBUG)) {
                        Log.d(TAG,
                                "Connection state not connecting or disconnecting; ignoring: "
                                        + newState);
                    }
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Gatt Services Discovered");
            }
            BluetoothGattService gapService = mBluetoothGatt.getService(
                    GENERIC_ACCESS_PROFILE_UUID);
            if (gapService == null) {
                Log.e(TAG, "Generic Access Service is Null");
                return;
            }
            BluetoothGattCharacteristic deviceNameCharacteristic = gapService.getCharacteristic(
                    DEVICE_NAME_UUID);
            if (deviceNameCharacteristic == null) {
                Log.e(TAG, "Device Name Characteristic is Null");
                return;
            }
            mBluetoothGatt.readCharacteristic(deviceNameCharacteristic);
        }

        @Override
        public void onCharacteristicRead(BluetoothGatt gatt,
                BluetoothGattCharacteristic characteristic, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                String deviceName = characteristic.getStringValue(0);
                if (Log.isLoggable(TAG, Log.DEBUG)) {
                    Log.d(TAG, "BLE Device Name: " + deviceName);
                }

                for (Callback callback : mCallbacks) {
                    callback.onDeviceNameRetrieved(deviceName);
                }
            } else {
                Log.e(TAG, "Reading GAP Failed: " + status);
            }
        }
    };

    /**
     * Interface to be notified of various events within the {@link BlePeripheralManager}.
     */
    interface Callback {
         /**
         * Triggered when the name of the remote device is retrieved.
         *
         * @param deviceName Name of the remote device.
         */
        void onDeviceNameRetrieved(@Nullable String deviceName);

        /**
         * Triggered if a remote client has requested to change the MTU for a given connection.
         *
         * @param size The new MTU size.
         */
        void onMtuSizeChanged(int size);

        /**
         * Triggered when a device (GATT client) connected.
         *
         * @param device Remote device that connected on BLE.
         */
        void onRemoteDeviceConnected(@NonNull BluetoothDevice device);

        /**
         * Triggered when a device (GATT client) disconnected.
         *
         * @param device Remote device that disconnected on BLE.
         */
        void onRemoteDeviceDisconnected(@NonNull BluetoothDevice device);
    }

    /**
     * An interface for classes that wish to be notified of writes to a characteristic.
     */
    interface OnCharacteristicWriteListener {
        /**
         * Triggered when this BlePeripheralManager receives a write request from a remote device.
         *
         * @param device The bluetooth device that holds the characteristic.
         * @param characteristic The characteristic that was written to.
         * @param value The value that was written.
         */
        void onCharacteristicWrite(
                @NonNull BluetoothDevice device,
                @NonNull BluetoothGattCharacteristic characteristic,
                @NonNull byte[] value);
    }

    /**
     * An interface for classes that wish to be notified of reads on a characteristic.
     */
    interface OnCharacteristicReadListener {
        /**
         * Triggered when this BlePeripheralManager receives a read request from a remote device.
         *
         * @param device The bluetooth device that holds the characteristic.
         * @param characteristic The characteristic that was read from.
         */
        void onCharacteristicRead(
                @NonNull BluetoothDevice device,
                @NonNull BluetoothGattCharacteristic characteristic);
    }
}
