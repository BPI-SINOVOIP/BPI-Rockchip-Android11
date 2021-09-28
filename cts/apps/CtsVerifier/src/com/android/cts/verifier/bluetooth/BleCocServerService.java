/*
 * Copyright 2018 The Android Open Source Project
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
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.ParcelUuid;
import android.util.Log;
import android.widget.Toast;

import com.android.cts.verifier.R;

import java.io.IOException;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Set;
import java.util.Timer;
import java.util.UUID;

public class BleCocServerService extends Service {

    public static final boolean DEBUG = true;
    public static final String TAG = "BleCocServerService";

    public static final int COMMAND_ADD_SERVICE = 0;
    public static final int COMMAND_WRITE_CHARACTERISTIC = 1;
    public static final int COMMAND_WRITE_DESCRIPTOR = 2;

    public static final int TEST_DATA_EXCHANGE_BUFSIZE = 8 * 1024;

    public static final String BLE_BLUETOOTH_MISMATCH_SECURE =
            "com.android.cts.verifier.bluetooth.BLE_BLUETOOTH_MISMATCH_SECURE";
    public static final String BLE_BLUETOOTH_MISMATCH_INSECURE =
            "com.android.cts.verifier.bluetooth.BLE_BLUETOOTH_MISMATCH_INSECURE";
    public static final String BLE_BLUETOOTH_DISABLED =
            "com.android.cts.verifier.bluetooth.BLE_BLUETOOTH_DISABLED";

    public static final String BLE_ACTION_COC_SERVER_INSECURE =
            "com.android.cts.verifier.bluetooth.BLE_ACTION_COC_SERVER_INSECURE";
    public static final String BLE_ACTION_COC_SERVER_SECURE =
            "com.android.cts.verifier.bluetooth.BLE_ACTION_COC_SERVER_SECURE";

    public static final String BLE_ACTION_SERVER_SECURE =
            "com.android.cts.verifier.bluetooth.BLE_ACTION_SERVER_SECURE";
    public static final String BLE_ACTION_SERVER_NON_SECURE =
            "com.android.cts.verifier.bluetooth.BLE_ACTION_SERVER_NON_SECURE";

    public static final String BLE_LE_CONNECTED =
            "com.android.cts.verifier.bluetooth.BLE_LE_CONNECTED";
    public static final String BLE_COC_LISTENER_CREATED =
            "com.android.cts.verifier.bluetooth.BLE_COC_LISTENER_CREATED";
    public static final String BLE_PSM_READ =
            "com.android.cts.verifier.bluetooth.BLE_PSM_READ";
    public static final String BLE_COC_CONNECTED =
            "com.android.cts.verifier.bluetooth.BLE_COC_CONNECTED";
    public static final String BLE_CONNECTION_TYPE_CHECKED =
            "com.android.cts.verifier.bluetooth.BLE_CONNECTION_TYPE_CHECKED";
    public static final String BLE_DATA_8BYTES_READ =
            "com.android.cts.verifier.bluetooth.BLE_DATA_8BYTES_READ";
    public static final String BLE_DATA_LARGEBUF_READ =
            "com.android.cts.verifier.bluetooth.BLE_DATA_LARGEBUF_READ";
    public static final String BLE_DATA_8BYTES_SENT =
            "com.android.cts.verifier.bluetooth.BLE_DATA_8BYTES_SENT";
    public static final String BLE_LE_DISCONNECTED =
            "com.android.cts.verifier.bluetooth.BLE_LE_DISCONNECTED";
    public static final String BLE_COC_SERVER_ACTION_SEND_DATA_8BYTES =
            "com.android.cts.verifier.bluetooth.BLE_COC_SERVER_ACTION_SEND_DATA_8BYTES";
    public static final String BLE_COC_SERVER_ACTION_EXCHANGE_DATA =
            "com.android.cts.verifier.bluetooth.BLE_COC_SERVER_ACTION_EXCHANGE_DATA";
    public static final String BLE_COC_SERVER_ACTION_DISCONNECT =
            "com.android.cts.verifier.bluetooth.BLE_COC_SERVER_ACTION_DISCONNECT";

    public static final String BLE_SERVER_DISCONNECTED =
            "com.android.cts.verifier.bluetooth.BLE_SERVER_DISCONNECTED";
    public static final String BLE_OPEN_FAIL =
            "com.android.cts.verifier.bluetooth.BLE_OPEN_FAIL";
    public static final String BLE_ADVERTISE_UNSUPPORTED =
            "com.android.cts.verifier.bluetooth.BLE_ADVERTISE_UNSUPPORTED";
    public static final String BLE_ADD_SERVICE_FAIL =
            "com.android.cts.verifier.bluetooth.BLE_ADD_SERVICE_FAIL";

    private static final UUID SERVICE_UUID =
            UUID.fromString("00009999-0000-1000-8000-00805f9b34fb");
    private static final UUID CHARACTERISTIC_UUID =
            UUID.fromString("00009998-0000-1000-8000-00805f9b34fb");
    private static final UUID CHARACTERISTIC_RESULT_UUID =
            UUID.fromString("00009974-0000-1000-8000-00805f9b34fb");
    private static final UUID UPDATE_CHARACTERISTIC_UUID =
            UUID.fromString("00009997-0000-1000-8000-00805f9b34fb");
    private static final UUID DESCRIPTOR_UUID =
            UUID.fromString("00009996-0000-1000-8000-00805f9b34fb");
    public static final UUID ADV_COC_SERVICE_UUID=
            UUID.fromString("00003334-0000-1000-8000-00805f9b34fb");

    private static final UUID SERVICE_UUID_ADDITIONAL =
            UUID.fromString("00009995-0000-1000-8000-00805f9b34fb");
    private static final UUID SERVICE_UUID_INCLUDED =
            UUID.fromString("00009994-0000-1000-8000-00805f9b34fb");

    // Variable for registration permission of Descriptor
    private static final UUID DESCRIPTOR_NO_READ_UUID =
            UUID.fromString("00009973-0000-1000-8000-00805f9b34fb");
    private static final UUID DESCRIPTOR_NO_WRITE_UUID =
            UUID.fromString("00009972-0000-1000-8000-00805f9b34fb");
    private static final UUID DESCRIPTOR_NEED_ENCRYPTED_READ_UUID =
            UUID.fromString("00009969-0000-1000-8000-00805f9b34fb");
    private static final UUID DESCRIPTOR_NEED_ENCRYPTED_WRITE_UUID =
            UUID.fromString("00009968-0000-1000-8000-00805f9b34fb");

    private static final int CONN_INTERVAL = 150;   // connection interval 150ms

    private static final int EXECUTION_DELAY = 1500;

    // Delay of notification when secure test failed to start.
    private static final long NOTIFICATION_DELAY_OF_SECURE_TEST_FAILURE = 5 * 1000;

    public static final String WRITE_VALUE = "SERVER_TEST";
    private static final String NOTIFY_VALUE = "NOTIFY_TEST";
    private static final String INDICATE_VALUE = "INDICATE_TEST";
    public static final String READ_NO_PERMISSION = "READ_NO_CHAR";
    public static final String WRITE_NO_PERMISSION = "WRITE_NO_CHAR";
    public static final String DESCRIPTOR_READ_NO_PERMISSION = "READ_NO_DESC";
    public static final String DESCRIPTOR_WRITE_NO_PERMISSION = "WRITE_NO_DESC";

    private BluetoothManager mBluetoothManager;
    private BluetoothGattServer mGattServer;
    private BluetoothGattService mService;
    private BluetoothDevice mDevice;
    private Handler mHandler;
    private BluetoothLeAdvertiser mAdvertiser;
    private boolean mSecure;
    private int mMtuSize = -1;

    private BluetoothServerSocket mServerSocket;
    private int mPsm = -1;
    private BluetoothGattCharacteristic mLePsmCharacteristic;
    BluetoothChatService mChatService;

    private int mNextReadExpectedLen = -1;
    private String mNextReadCompletionIntent;
    private int mTotalReadLen = 0;
    private byte mNextReadByte;
    private int mNextWriteExpectedLen = -1;
    private String mNextWriteCompletionIntent = null;

    // Handler for communicating task with peer.
    private TestTaskQueue mTaskQueue;

    // current test category
    private String mCurrentAction;

    // Task to notify failure of starting secure test.
    //   Secure test calls BluetoothDevice#createBond() when devices were not paired.
    //   createBond() causes onConnectionStateChange() twice, and it works as strange sequence.
    //   At the first onConnectionStateChange(), target device is not paired (bond state is
    //   BluetoothDevice.BOND_NONE).
    //   At the second onConnectionStateChange(), target devices is paired (bond state is
    //   BluetoothDevice.BOND_BONDED).
    //   CTS Verifier will perform lazy check of bond state. Verifier checks bond state
    //   after NOTIFICATION_DELAY_OF_SECURE_TEST_FAILURE from the first onConnectionStateChange().
    private Runnable mNotificationTaskOfSecureTestStartFailure;

    @Override
    public void onCreate() {
        super.onCreate();

        mTaskQueue = new TestTaskQueue(getClass().getName() + "_taskHandlerThread");

        mBluetoothManager = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
        mAdvertiser = mBluetoothManager.getAdapter().getBluetoothLeAdvertiser();
        mGattServer = mBluetoothManager.openGattServer(this, mCallbacks);

        mService = createService();

        mDevice = null;

        mHandler = new Handler();
        if (!mBluetoothManager.getAdapter().isEnabled()) {
            notifyBluetoothDisabled();
        } else if (mGattServer == null) {
            notifyOpenFail();
        } else if (mAdvertiser == null) {
            notifyAdvertiseUnsupported();
        } else {
            // start adding services
            mSecure = false;
            if (!mGattServer.addService(mService)) {
                notifyAddServiceFail();
            }
        }
    }

    private void notifyBluetoothDisabled() {
        Intent intent = new Intent(BLE_BLUETOOTH_DISABLED);
        sendBroadcast(intent);
    }

    private void notifyMismatchSecure() {
        Intent intent = new Intent(BLE_BLUETOOTH_MISMATCH_SECURE);
        sendBroadcast(intent);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String action = intent.getAction();
        if (action != null) {
            if (DEBUG) {
                Log.d(TAG, "onStartCommand: action=" + action);
            }
            mTaskQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    onTestFinish(intent.getAction());
                }
            }, EXECUTION_DELAY);
        }
        return START_NOT_STICKY;
    }

    private void startServerTest(boolean secure) {
        mSecure = secure;

        if (mBluetoothManager.getAdapter().isEnabled() && (mChatService == null)) {
            createChatService();
        }

        if (mBluetoothManager.getAdapter().isEnabled() && (mAdvertiser != null)) {
            startAdvertise();
        }
    }

    private void sendMessage(byte[] buf) {
        mChatService.write(buf);
    }

    private void sendData8bytes() {
        if (DEBUG) Log.d(TAG, "sendData8bytes");

        final byte[] buf = new byte[]{1,2,3,4,5,6,7,8};
        mNextWriteExpectedLen = 8;
        mNextWriteCompletionIntent = BLE_DATA_8BYTES_SENT;
        sendMessage(buf);
    }

    private void sendDataLargeBuf() {
        final int len = BleCocServerService.TEST_DATA_EXCHANGE_BUFSIZE;
        if (DEBUG) Log.d(TAG, "sendDataLargeBuf of size=" + len);

        byte[] buf = new byte[len];
        for (int i = 0; i < len; i++) {
            buf[i] = (byte)(i + 1);
        }
        mNextWriteExpectedLen = len;
        mNextWriteCompletionIntent = null;
        sendMessage(buf);
    }

    private void onTestFinish(String action) {
        mCurrentAction = action;
        if (mCurrentAction != null) {
            switch (mCurrentAction) {
                case BLE_ACTION_COC_SERVER_INSECURE:
                    startServerTest(false);
                    break;
                case BLE_ACTION_COC_SERVER_SECURE:
                    startServerTest(true);
                    break;
                case BLE_COC_SERVER_ACTION_SEND_DATA_8BYTES:
                    sendData8bytes();
                    break;
                case BLE_COC_SERVER_ACTION_EXCHANGE_DATA:
                    sendDataLargeBuf();
                    readDataLargeBuf();
                    break;
                case BLE_COC_SERVER_ACTION_DISCONNECT:
                    if (mChatService != null) {
                        mChatService.stop();
                    }
                    notifyDisconnected();
                    break;
                default:
                    Log.e(TAG, "Error: Unhandled or invalid action=" + mCurrentAction);
            }
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (mChatService != null) {
            mChatService.stop();
        }

        cancelNotificationTaskOfSecureTestStartFailure();
        stopAdvertise();

        mTaskQueue.quit();

        if (mGattServer == null) {
           return;
        }
        if (mDevice != null) {
            mGattServer.cancelConnection(mDevice);
        }
        mGattServer.clearServices();
        mGattServer.close();
    }

    private void notifyOpenFail() {
        if (DEBUG) {
            Log.d(TAG, "notifyOpenFail");
        }
        Intent intent = new Intent(BLE_OPEN_FAIL);
        sendBroadcast(intent);
    }

    private void notifyAddServiceFail() {
        if (DEBUG) {
            Log.d(TAG, "notifyAddServiceFail");
        }
        Intent intent = new Intent(BLE_ADD_SERVICE_FAIL);
        sendBroadcast(intent);
    }

    private void notifyAdvertiseUnsupported() {
        if (DEBUG) {
            Log.d(TAG, "notifyAdvertiseUnsupported");
        }
        Intent intent = new Intent(BLE_ADVERTISE_UNSUPPORTED);
        sendBroadcast(intent);
    }

    private void notifyConnected() {
        if (DEBUG) {
            Log.d(TAG, "notifyConnected");
        }
        Intent intent = new Intent(BLE_LE_CONNECTED);
        sendBroadcast(intent);
    }

    private void notifyDisconnected() {
        if (DEBUG) {
            Log.d(TAG, "notifyDisconnected");
        }
        Intent intent = new Intent(BLE_SERVER_DISCONNECTED);
        sendBroadcast(intent);
    }

    private BluetoothGattService createService() {
        BluetoothGattService service =
                new BluetoothGattService(SERVICE_UUID, BluetoothGattService.SERVICE_TYPE_PRIMARY);
        BluetoothGattCharacteristic characteristic =
                new BluetoothGattCharacteristic(CHARACTERISTIC_UUID, 0x0A, 0x11);
        characteristic.setValue(WRITE_VALUE.getBytes());

        BluetoothGattDescriptor descriptor = new BluetoothGattDescriptor(DESCRIPTOR_UUID, 0x11);
        descriptor.setValue(WRITE_VALUE.getBytes());
        characteristic.addDescriptor(descriptor);

        BluetoothGattDescriptor descriptor_permission =
            new BluetoothGattDescriptor(DESCRIPTOR_NO_READ_UUID, 0x10);
        characteristic.addDescriptor(descriptor_permission);

        descriptor_permission = new BluetoothGattDescriptor(DESCRIPTOR_NO_WRITE_UUID, 0x01);
        characteristic.addDescriptor(descriptor_permission);

        service.addCharacteristic(characteristic);

        // Registered the characteristic of PSM Value
        mLePsmCharacteristic =
                new BluetoothGattCharacteristic(BleCocClientService.LE_PSM_CHARACTERISTIC_UUID,
                                                BluetoothGattCharacteristic.PROPERTY_READ,
                                                BluetoothGattCharacteristic.PERMISSION_READ);
        service.addCharacteristic(mLePsmCharacteristic);

        return service;
    }

    private void showMessage(final String msg) {
        mHandler.post(new Runnable() {
            public void run() {
                Toast.makeText(BleCocServerService.this, msg, Toast.LENGTH_SHORT).show();
            }
        });
    }

    private synchronized void cancelNotificationTaskOfSecureTestStartFailure() {
        if (mNotificationTaskOfSecureTestStartFailure != null) {
            mHandler.removeCallbacks(mNotificationTaskOfSecureTestStartFailure);
            mNotificationTaskOfSecureTestStartFailure = null;
        }
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
                    boolean bonded = false;
                    Set<BluetoothDevice> pairedDevices =
                        mBluetoothManager.getAdapter().getBondedDevices();
                    if (pairedDevices.size() > 0) {
                        for (BluetoothDevice target : pairedDevices) {
                            if (target.getAddress().equals(device.getAddress())) {
                                bonded = true;
                                break;
                            }
                        }
                    }

                    if (mSecure && ((device.getBondState() == BluetoothDevice.BOND_NONE) ||
                                    !bonded)) {
                        // not pairing and execute Secure Test
                        Log.e(TAG, "BluetoothGattServerCallback.onConnectionStateChange: "
                              + "Not paired but execute secure test");
                        cancelNotificationTaskOfSecureTestStartFailure();
                    } else if (!mSecure && ((device.getBondState() != BluetoothDevice.BOND_NONE)
                                            || bonded)) {
                        // already pairing and execute Insecure Test
                        Log.e(TAG, "BluetoothGattServerCallback.onConnectionStateChange: "
                              + "Paired but execute insecure test");
                    } else {
                        cancelNotificationTaskOfSecureTestStartFailure();
                    }
                    notifyConnected();
                } else if (status == BluetoothProfile.STATE_DISCONNECTED) {
                    notifyDisconnected();
                    mDevice = null;
                }
            }
        }

        @Override
        public void onCharacteristicReadRequest(BluetoothDevice device, int requestId, int offset,
                BluetoothGattCharacteristic characteristic) {
            if (mGattServer == null) {
                if (DEBUG) {
                    Log.d(TAG, "GattServer is null, return");
                }
                return;
            }
            if (DEBUG) {
                Log.d(TAG, "onCharacteristicReadRequest()");
            }

            boolean finished = false;
            byte[] value = null;
            if (mMtuSize > 0) {
                byte[] buf = characteristic.getValue();
                if (buf != null) {
                    int len = Math.min((buf.length - offset), mMtuSize);
                    if (len > 0) {
                        value = Arrays.copyOfRange(buf, offset, (offset + len));
                    }
                    finished = ((offset + len) >= buf.length);
                    if (finished) {
                        Log.d(TAG, "sent whole data: " + (new String(characteristic.getValue())));
                    }
                }
            } else {
                value = characteristic.getValue();
                finished = true;
            }

            mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, value);

            UUID uid = characteristic.getUuid();
            if (uid.equals(BleCocClientService.LE_PSM_CHARACTERISTIC_UUID)) {
                Log.d(TAG, "onCharacteristicReadRequest: reading PSM");
            }
        }

    };

    private void leCheckConnectionType() {
        if (mChatService == null) {
            Log.e(TAG, "leCheckConnectionType: no LE Coc connection");
            return;
        }
        int type = mChatService.getSocketConnectionType();
        if (type != BluetoothSocket.TYPE_L2CAP) {
            Log.e(TAG, "leCheckConnectionType: invalid connection type=" + type);
            return;
        }
        showMessage("LE CoC Connection Type Checked");
        Intent intent = new Intent(BLE_CONNECTION_TYPE_CHECKED);
        sendBroadcast(intent);
    }

    private void readData8bytes() {
        mNextReadExpectedLen = 8;
        mTotalReadLen = 0;
        mNextReadCompletionIntent = BLE_DATA_8BYTES_READ;
        mNextReadByte = 1;
    }

    private void readDataLargeBuf() {
        mNextReadExpectedLen = BleCocServerService.TEST_DATA_EXCHANGE_BUFSIZE;
        mTotalReadLen = 0;
        mNextReadCompletionIntent = BLE_DATA_LARGEBUF_READ;
        mNextReadByte = 1;
    }

    private void processChatStateChange(int newState) {
        Intent intent;
        if (DEBUG) {
            Log.d(TAG, "processChatStateChange: newState=" + newState);
        }
        switch (newState) {
        case BluetoothChatService.STATE_LISTEN:
            intent = new Intent(BLE_COC_LISTENER_CREATED);
            sendBroadcast(intent);
            break;
        case BluetoothChatService.STATE_CONNECTED:
            intent = new Intent(BLE_COC_CONNECTED);
            sendBroadcast(intent);

            // Check the connection type
            leCheckConnectionType();

            // Prepare the next data read
            readData8bytes();
            break;
        }
    }

    private boolean checkReadBufContent(byte[] buf, int len) {
        // Check that the content is correct
        for (int i = 0; i < len; i++) {
            if (buf[i] != mNextReadByte) {
                Log.e(TAG, "handleMessageRead: Error: wrong byte content. buf["
                      + i + "]=" + buf[i] + " not equal to " + mNextReadByte);
                return false;
            }
            mNextReadByte++;
        }
        return true;
    }

    private void handleMessageRead(Message msg) {
        byte[] buf = (byte[])msg.obj;
        int len = msg.arg1;
        if (len <= 0) {
            return;
        }
        mTotalReadLen += len;
        if (DEBUG) {
            Log.d(TAG, "handleMessageRead: receive buffer of length=" + len + ", mTotalReadLen="
                  + mTotalReadLen + ", mNextReadExpectedLen=" + mNextReadExpectedLen);
        }

        if (mNextReadExpectedLen == mTotalReadLen) {
            if (!checkReadBufContent(buf, len)) {
                mNextReadExpectedLen = -1;
                return;
            }
            showMessage("Read " + len + " bytes");
            if (DEBUG) {
                Log.d(TAG, "handleMessageRead: broadcast intent " + mNextReadCompletionIntent);
            }
            Intent intent = new Intent(mNextReadCompletionIntent);
            sendBroadcast(intent);
            mNextReadExpectedLen = -1;
            mNextReadCompletionIntent = null;
            mTotalReadLen = 0;
        } else if (mNextReadExpectedLen > mTotalReadLen) {
            if (!checkReadBufContent(buf, len)) {
                mNextReadExpectedLen = -1;
                return;
            }
        } else if (mNextReadExpectedLen < mTotalReadLen) {
            Log.e(TAG, "handleMessageRead: Unexpected receive buffer of length=" + len
                  + ", expected len=" + mNextReadExpectedLen);
        }
    }

    private void handleMessageWrite(Message msg) {
        byte[] buffer = (byte[]) msg.obj;
        int len = buffer.length;
        showMessage("LE CoC Server wrote " + len + " bytes" + ", mNextWriteExpectedLen="
                    + mNextWriteExpectedLen);
        if (len == mNextWriteExpectedLen) {
            if (mNextWriteCompletionIntent != null) {
                Intent intent = new Intent(mNextWriteCompletionIntent);
                sendBroadcast(intent);
            }
        } else {
            Log.d(TAG, "handleMessageWrite: unrecognized length=" + len);
        }
        mNextWriteCompletionIntent = null;
        mNextWriteExpectedLen = -1;
    }

    private class ChatHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            if (DEBUG) {
                Log.d(TAG, "ChatHandler.handleMessage: msg=" + msg);
            }
            switch (msg.what) {
                case BluetoothChatService.MESSAGE_STATE_CHANGE:
                    processChatStateChange(msg.arg1);
                    break;
                case BluetoothChatService.MESSAGE_READ:
                    handleMessageRead(msg);
                    break;
                case BluetoothChatService.MESSAGE_WRITE:
                    handleMessageWrite(msg);
                    break;
            }
        }
    }

    /* Start the Chat Service to create the Bluetooth Server Socket for LE CoC */
    private void createChatService() {

        mChatService = new BluetoothChatService(this, new ChatHandler(), true);
        mChatService.start(mSecure);
        mPsm = mChatService.getPsm(mSecure);
        if (DEBUG) {
            Log.d(TAG, "createChatService: assigned PSM=" + mPsm);
        }
        if (mPsm > 0x00ff) {
            Log.e(TAG, "createChatService: Invalid PSM=" + mPsm);
        }
        // Notify that the PSM is read
        Intent intent = new Intent(BLE_PSM_READ);
        sendBroadcast(intent);

        // Set the PSM value in the PSM characteristics in the GATT Server.
        mLePsmCharacteristic.setValue(mPsm, BluetoothGattCharacteristic.FORMAT_UINT8, 0);
    }

    private void startAdvertise() {
        if (DEBUG) {
            Log.d(TAG, "startAdvertise");
        }
        AdvertiseData data = new AdvertiseData.Builder()
            .addServiceData(new ParcelUuid(ADV_COC_SERVICE_UUID), new byte[]{1,2,3})
            .addServiceUuid(new ParcelUuid(ADV_COC_SERVICE_UUID))
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

    private final AdvertiseCallback mAdvertiseCallback = new AdvertiseCallback(){
        @Override
        public void onStartFailure(int errorCode) {
            // Implementation for API Test.
            super.onStartFailure(errorCode);
            if (DEBUG) {
                Log.d(TAG, "onStartFailure");
            }

            if (errorCode == ADVERTISE_FAILED_FEATURE_UNSUPPORTED) {
                notifyAdvertiseUnsupported();
            } else {
                notifyOpenFail();
            }
        }

        @Override
        public void onStartSuccess(AdvertiseSettings settingsInEffect) {
            // Implementation for API Test.
            super.onStartSuccess(settingsInEffect);
            if (DEBUG) {
                Log.d(TAG, "onStartSuccess");
            }
        }
    };

    /*protected*/ static void dumpService(BluetoothGattService service, int level) {
        String indent = "";
        for (int i = 0; i < level; ++i) {
            indent += "  ";
        }

        Log.d(TAG, indent + "[service]");
        Log.d(TAG, indent + "UUID: " + service.getUuid());
        Log.d(TAG, indent + "  [characteristics]");
        for (BluetoothGattCharacteristic ch : service.getCharacteristics()) {
            Log.d(TAG, indent + "    UUID: " + ch.getUuid());
            Log.d(TAG, indent + "      properties: "
                  + String.format("0x%02X", ch.getProperties()));
            Log.d(TAG, indent + "      permissions: "
                  + String.format("0x%02X", ch.getPermissions()));
            Log.d(TAG, indent + "      [descriptors]");
            for (BluetoothGattDescriptor d : ch.getDescriptors()) {
                Log.d(TAG, indent + "        UUID: " + d.getUuid());
                Log.d(TAG, indent + "          permissions: "
                      + String.format("0x%02X", d.getPermissions()));
            }
        }

        if (service.getIncludedServices() != null) {
            Log.d(TAG, indent + "  [included services]");
            for (BluetoothGattService s : service.getIncludedServices()) {
                dumpService(s, level + 1);
            }
        }
    }
}

