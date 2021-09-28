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

package com.android.cts.verifier.bluetooth;

import android.app.Service;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
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
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.ParcelUuid;
import android.util.Log;
import android.widget.Toast;

import java.util.Timer;
import java.util.TimerTask;
import java.util.UUID;

public class BleConnectionPriorityServerService extends Service {
    public static final boolean DEBUG = true;
    public static final String TAG = "BlePriorityServer";

    public static final String ACTION_BLUETOOTH_DISABLED =
            "com.android.cts.verifier.bluetooth.action.BLUETOOTH_DISABLED";

    public static final String ACTION_CONNECTION_PRIORITY_FINISH =
            "com.android.cts.verifier.bluetooth.action.ACTION_CONNECTION_PRIORITY_FINISH";

    public static final String ACTION_START_CONNECTION_PRIORITY_TEST =
            "com.android.cts.verifier.bluetooth.action.ACTION_START_CONNECTION_PRIORITY_TEST";

    public static final UUID ADV_SERVICE_UUID=
            UUID.fromString("00002222-0000-1000-8000-00805f9b34fb");

    private BluetoothManager mBluetoothManager;
    private BluetoothGattServer mGattServer;
    private BluetoothGattService mService;
    private BluetoothDevice mDevice;
    private Handler mHandler;
    private BluetoothLeAdvertiser mAdvertiser;
    private long mReceiveWriteCount;

    private int interval_low = 0;
    private int interval_balanced = 0;
    private int interval_high = 0;

    @Override
    public void onCreate() {
        super.onCreate();

        mBluetoothManager = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
        mAdvertiser = mBluetoothManager.getAdapter().getBluetoothLeAdvertiser();
        mGattServer = mBluetoothManager.openGattServer(this, mCallbacks);
        mDevice = null;
        mHandler = new Handler();

        if (!mBluetoothManager.getAdapter().isEnabled()) {
            notifyBluetoothDisabled();
        } else if (mGattServer == null) {
            notifyOpenFail();
        } else if (mAdvertiser == null) {
            notifyAdvertiseUnsupported();
        } else {
            startAdvertise();
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        stopAdvertise();
        if (mGattServer == null) {
            return;
        }
        if (mDevice != null) {
            mGattServer.cancelConnection(mDevice);
        }
        mGattServer.clearServices();
        mGattServer.close();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_NOT_STICKY;
    }

    private void notifyBluetoothDisabled() {
        if (DEBUG) {
            Log.d(TAG, "notifyBluetoothDisabled");
        }
        Intent intent = new Intent(ACTION_BLUETOOTH_DISABLED);
        sendBroadcast(intent);
    }

    private void notifyTestStart() {
        Intent intent = new Intent(BleConnectionPriorityServerService.ACTION_START_CONNECTION_PRIORITY_TEST);
        sendBroadcast(intent);
    }

    private void notifyOpenFail() {
        if (DEBUG) {
            Log.d(TAG, "notifyOpenFail");
        }
        Intent intent = new Intent(BleServerService.BLE_OPEN_FAIL);
        sendBroadcast(intent);
    }

    private void notifyAdvertiseUnsupported() {
        if (DEBUG) {
            Log.d(TAG, "notifyAdvertiseUnsupported");
        }
        Intent intent = new Intent(BleServerService.BLE_ADVERTISE_UNSUPPORTED);
        sendBroadcast(intent);
    }

    private void notifyConnected() {
        if (DEBUG) {
            Log.d(TAG, "notifyConnected");
        }
    }

    private void notifyDisconnected() {
        if (DEBUG) {
            Log.d(TAG, "notifyDisconnected");
        }
    }

    private void showMessage(final String msg) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(getApplicationContext(), msg, Toast.LENGTH_SHORT).show();
            }
        });
    }

    private final BluetoothGattServerCallback mCallbacks = new BluetoothGattServerCallback() {
        @Override
        public void onConnectionStateChange(BluetoothDevice device, int status, int newState) {
            if (DEBUG) {
                Log.d(TAG, "onConnectionStateChange: newState=" + newState);
            }
            if (status == BluetoothGatt.GATT_SUCCESS) {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    mDevice = device;
                    notifyConnected();
                } else if (status == BluetoothProfile.STATE_DISCONNECTED) {
                    notifyDisconnected();
                    mDevice = null;
                }
            }

            //onConnectionUpdated is hidden callback, can't be marked as @Override.
            // We must have a call to it, otherwise compiler will delete it during optimization.
            if (status == 0xFFEFFEE) {
                // This should never execute, but will make compiler not remove onConnectionUpdated 
                onConnectionUpdated(null, 0, 0, 0, 0);
                throw new IllegalStateException("This should never happen!");
            }

        }

        // @Override uncomment once this becomes public API
        public void onConnectionUpdated(BluetoothDevice device, int interval, int latency, int timeout, int status) {
            Log.i(TAG, "onConnectionUpdated() status=" + status + ", interval=" + interval);

            if (status != 0) {
               interval_low = interval_balanced = interval_high = 0;
               return;
            }

            // since we don't know when the test started, wait for three descending interval values.
            // Even though conneciton is updated by service discovery, it never happen three times
            // descending in any scenario. 

            // shift all values
            interval_low = interval_balanced;
            interval_balanced = interval_high;
            interval_high = interval;

            // If we end up with three descending values, test is passed.
            if (interval_low > interval_balanced && interval_balanced > interval_high) {
                showMessage("intervals: " + interval_low +" > " + interval_balanced + " > " + interval_high);
                notifyMeasurementFinished();
            }
        }
    };

    private void notifyMeasurementFinished() {
        Intent intent = new Intent();
        intent.setAction(ACTION_CONNECTION_PRIORITY_FINISH);
        sendBroadcast(intent);
    }

    private final AdvertiseCallback mAdvertiseCallback = new AdvertiseCallback() {
        @Override
        public void onStartFailure(int errorCode) {
            super.onStartFailure(errorCode);
            if (errorCode == ADVERTISE_FAILED_FEATURE_UNSUPPORTED) {
                notifyAdvertiseUnsupported();
            } else {
                notifyOpenFail();
            }
        }
    };

    private void startAdvertise() {
        if (DEBUG) {
            Log.d(TAG, "startAdvertise");
        }
        AdvertiseData data = new AdvertiseData.Builder()
                .addServiceData(new ParcelUuid(ADV_SERVICE_UUID), new byte[]{1, 2, 3})
                .addServiceUuid(new ParcelUuid(ADV_SERVICE_UUID))
                .build();
        AdvertiseSettings setting = new AdvertiseSettings.Builder()
                .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
                .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM)
                .setConnectable(true)
                .build();
        mAdvertiser.startAdvertising(setting, data, mAdvertiseCallback);
    }

    private void stopAdvertise() {
        if (DEBUG) {
            Log.d(TAG, "stopAdvertise");
        }
        if (mAdvertiser != null) {
            mAdvertiser.stopAdvertising(mAdvertiseCallback);
        }
    }

}
