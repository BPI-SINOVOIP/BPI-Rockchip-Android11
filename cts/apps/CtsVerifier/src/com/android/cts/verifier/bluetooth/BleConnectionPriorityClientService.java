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
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.content.Intent;
import android.os.DeadObjectException;
import android.os.Handler;
import android.os.IBinder;
import android.os.ParcelUuid;
import android.util.Log;
import android.widget.Toast;

import java.util.Arrays;
import java.util.Date;
import java.util.List;
import java.util.Set;
import java.util.Timer;
import java.util.TimerTask;
import java.util.UUID;

public class BleConnectionPriorityClientService extends Service {
    public static final boolean DEBUG = true;
    public static final String TAG = "BlePriorityClient";

    public static final String ACTION_BLUETOOTH_DISABLED =
            "com.android.cts.verifier.bluetooth.action.BLUETOOTH_DISABLED";

    public static final String ACTION_CONNECTION_SERVICES_DISCOVERED =
            "com.android.cts.verifier.bluetooth.action.CONNECTION_SERVICES_DISCOVERED";

    public static final String ACTION_BLUETOOTH_MISMATCH_SECURE =
            "com.android.cts.verifier.bluetooth.action.ACTION_BLUETOOTH_MISMATCH_SECURE";
    public static final String ACTION_BLUETOOTH_MISMATCH_INSECURE =
            "com.android.cts.verifier.bluetooth.action.ACTION_BLUETOOTH_MISMATCH_INSECURE";

    public static final String ACTION_CONNECTION_PRIORITY_START =
            "com.android.cts.verifier.bluetooth.action.CONNECTION_PRIORITY_LOW_POWER";

    public static final String ACTION_CONNECTION_PRIORITY_FINISH =
            "com.android.cts.verifier.bluetooth.action.CONNECTION_PRIORITY_FINISH";

    public static final String ACTION_CLIENT_CONNECT_SECURE =
            "com.android.cts.verifier.bluetooth.action.CLIENT_CONNECT_SECURE";

    public static final String ACTION_DISCONNECT =
            "com.android.cts.verifier.bluetooth.action.DISCONNECT";
    public static final String ACTION_FINISH_DISCONNECT =
            "com.android.cts.verifier.bluetooth.action.FINISH_DISCONNECT";

    public static final int NOT_UNDER_TEST = -1;

    private BluetoothManager mBluetoothManager;
    private BluetoothAdapter mBluetoothAdapter;
    private BluetoothGatt mBluetoothGatt;
    private BluetoothLeScanner mScanner;
    private Handler mHandler;
    private Timer mConnectionTimer;
    private Context mContext;

    private String mAction;
    private boolean mSecure;

    private int mPriority = NOT_UNDER_TEST;
    private int interval_low = 0;
    private int interval_balanced = 0;
    private int interval_high = 0;

    private TestTaskQueue mTaskQueue;

    @Override
    public void onCreate() {
        super.onCreate();

        mTaskQueue = new TestTaskQueue(getClass().getName() + "__taskHandlerThread");

        mBluetoothManager = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
        mBluetoothAdapter = mBluetoothManager.getAdapter();
        mScanner = mBluetoothAdapter.getBluetoothLeScanner();
        mHandler = new Handler();
        mContext = this;

        startScan();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mBluetoothGatt != null) {
            try {
                mBluetoothGatt.disconnect();
                mBluetoothGatt.close();
            } catch (Exception e) {}
            finally {
                mBluetoothGatt = null;
            }
        }
        stopScan();

        mTaskQueue.quit();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void notifyBluetoothDisabled() {
        Intent intent = new Intent(ACTION_BLUETOOTH_DISABLED);
        sendBroadcast(intent);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null) {
            mAction = intent.getAction();
            if (mAction != null) {
                switch (mAction) {
                case ACTION_CLIENT_CONNECT_SECURE:
                    mSecure = true;
                    break;
                case ACTION_CONNECTION_PRIORITY_START:
                    myRequestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER);
                    break;
                case ACTION_DISCONNECT:
                    if (mBluetoothGatt != null) {
                        mBluetoothGatt.disconnect();
                    } else {
                        notifyDisconnect();
                    }
                    break;
                }
            }
        }
        return START_NOT_STICKY;
    }

    private void myRequestConnectionPriority(final int priority) {
        mTaskQueue.addTask(new Runnable() {
                                @Override
                                public void run() {
                                    mPriority = priority;
                                    mBluetoothGatt.requestConnectionPriority(mPriority);
                                    //continue in onConnectionUpdated() callback
                                }
                            });
    }


    private void sleep(int millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
            Log.e(TAG, "Error in thread sleep", e);
        }
    }

    private void showMessage(final String msg) {
        mHandler.post(new Runnable() {
            public void run() {
                Toast.makeText(BleConnectionPriorityClientService.this, msg, Toast.LENGTH_SHORT).show();
            }
        });
    }

    private final BluetoothGattCallback mGattCallbacks = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            if (DEBUG) Log.d(TAG, "onConnectionStateChange");
            if (status == BluetoothGatt.GATT_SUCCESS) {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    int bond = gatt.getDevice().getBondState();
                    boolean bonded = false;
                    BluetoothDevice target = gatt.getDevice();
                    Set<BluetoothDevice> pairedDevices = mBluetoothAdapter.getBondedDevices();
                    if (pairedDevices.size() > 0) {
                        for (BluetoothDevice device : pairedDevices) {
                            if (device.getAddress().equals(target.getAddress())) {
                                bonded = true;
                                break;
                            }
                        }
                    }
                    if (mSecure && ((bond == BluetoothDevice.BOND_NONE) || !bonded)) {
                        // not pairing and execute Secure Test
                        mBluetoothGatt.disconnect();
                        notifyMismatchSecure();
                    } else if (!mSecure && ((bond != BluetoothDevice.BOND_NONE) || bonded)) {
                        // already pairing nad execute Insecure Test
                        mBluetoothGatt.disconnect();
                        notifyMismatchInsecure();
                    } else {
                        showMessage("Bluetooth LE connected");
                        mBluetoothGatt.discoverServices();
                    }
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    showMessage("Bluetooth LE disconnected");

                    notifyDisconnect();
                }
            } else {
                showMessage("Failed to connect");
                mBluetoothGatt.close();
                mBluetoothGatt = null;
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            if (DEBUG){
                Log.d(TAG, "onServiceDiscovered");
            }
            if (status == BluetoothGatt.GATT_SUCCESS) {
                showMessage("Service discovered");
                Intent intent = new Intent(ACTION_CONNECTION_SERVICES_DISCOVERED);
                sendBroadcast(intent);
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
        public void onConnectionUpdated(BluetoothGatt gatt, int interval, int latency, int timeout,
            int status) {
            if (mPriority == NOT_UNDER_TEST) return;

            if (status != 0) {
                showMessage("onConnectionUpdated() error, status=" + status );
                Log.e(TAG, "onConnectionUpdated() status=" + status);
                return;
            }

            Log.i(TAG, "onConnectionUpdated() status=" + status + ", interval=" + interval);
            if (mPriority == BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER) {
                interval_low = interval;
                myRequestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_BALANCED);
            } else if (mPriority == BluetoothGatt.CONNECTION_PRIORITY_BALANCED) {
                interval_balanced = interval;
                myRequestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH);
            } else if (mPriority == BluetoothGatt.CONNECTION_PRIORITY_HIGH) {
                interval_high = interval;

                if (interval_low < interval_balanced || interval_balanced < interval_high) {
                   showMessage("interval value should be descending - failure!");
                   Log.e(TAG, "interval values should be descending: interval_low=" + interval_low +
                            ", interval_balanced=" + interval_balanced + ", interval_high=" + interval_high);
                   return;
                }

                showMessage("intervals: " + interval_low +" > " + interval_balanced + " > " + interval_high);
         
                Intent intent = new Intent();
                intent.setAction(ACTION_CONNECTION_PRIORITY_FINISH);
                sendBroadcast(intent);
        
                mPriority = NOT_UNDER_TEST;
            }
        }
    };

    private final ScanCallback mScanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            if (mBluetoothGatt == null) {
                stopScan();
                mBluetoothGatt = BleClientService.connectGatt(result.getDevice(), mContext, false, mSecure, mGattCallbacks);
            }
        }
    };

    private void startScan() {
        if (DEBUG) {
            Log.d(TAG, "startScan");
        }
        if (!mBluetoothAdapter.isEnabled()) {
            notifyBluetoothDisabled();
        } else {
            List<ScanFilter> filter = Arrays.asList(new ScanFilter.Builder().setServiceUuid(
                    new ParcelUuid(BleConnectionPriorityServerService.ADV_SERVICE_UUID)).build());
            ScanSettings setting = new ScanSettings.Builder()
                    .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build();
            mScanner.startScan(filter, setting, mScanCallback);
        }
    }

    private void stopScan() {
        if (DEBUG) {
            Log.d(TAG, "stopScan");
        }
        if (mScanner != null) {
            mScanner.stopScan(mScanCallback);
        }
    }

    private void notifyMismatchSecure() {
        Intent intent = new Intent(ACTION_BLUETOOTH_MISMATCH_SECURE);
        sendBroadcast(intent);
    }

    private void notifyMismatchInsecure() {
        Intent intent = new Intent(ACTION_BLUETOOTH_MISMATCH_INSECURE);
        sendBroadcast(intent);
    }

    private void notifyDisconnect() {
        Intent intent = new Intent(ACTION_FINISH_DISCONNECT);
        sendBroadcast(intent);
    }
}
