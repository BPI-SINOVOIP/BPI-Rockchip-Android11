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

import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.UUID;

import java.io.ByteArrayOutputStream;

import com.android.cts.verifier.R;

import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothSocket;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.ParcelUuid;
import android.text.TextUtils;
import android.util.Log;
import android.widget.Toast;

public class BleCocClientService extends Service {

    public static final boolean DEBUG = true;
    public static final String TAG = "BleCocClientService";

    private static final int TRANSPORT_MODE_FOR_SECURE_CONNECTION = BluetoothDevice.TRANSPORT_LE;

    public static final String BLE_LE_CONNECTED =
            "com.android.cts.verifier.bluetooth.BLE_LE_CONNECTED";
    public static final String BLE_GOT_PSM =
            "com.android.cts.verifier.bluetooth.BLE_GOT_PSM";
    public static final String BLE_COC_CONNECTED =
            "com.android.cts.verifier.bluetooth.BLE_COC_CONNECTED";
    public static final String BLE_CONNECTION_TYPE_CHECKED =
            "com.android.cts.verifier.bluetooth.BLE_CONNECTION_TYPE_CHECKED";
    public static final String BLE_DATA_8BYTES_SENT =
            "com.android.cts.verifier.bluetooth.BLE_DATA_8BYTES_SENT";
    public static final String BLE_DATA_8BYTES_READ =
            "com.android.cts.verifier.bluetooth.BLE_DATA_8BYTES_READ";
    public static final String BLE_DATA_LARGEBUF_READ =
            "com.android.cts.verifier.bluetooth.BLE_DATA_LARGEBUF_READ";
    public static final String BLE_LE_DISCONNECTED =
            "com.android.cts.verifier.bluetooth.BLE_LE_DISCONNECTED";

    public static final String BLE_BLUETOOTH_MISMATCH_SECURE =
            "com.android.cts.verifier.bluetooth.BLE_BLUETOOTH_MISMATCH_SECURE";
    public static final String BLE_BLUETOOTH_MISMATCH_INSECURE =
            "com.android.cts.verifier.bluetooth.BLE_BLUETOOTH_MISMATCH_INSECURE";
    public static final String BLE_BLUETOOTH_DISABLED =
            "com.android.cts.verifier.bluetooth.BLE_BLUETOOTH_DISABLED";
    public static final String BLE_GATT_CONNECTED =
            "com.android.cts.verifier.bluetooth.BLE_GATT_CONNECTED";
    public static final String BLE_BLUETOOTH_DISCONNECTED =
            "com.android.cts.verifier.bluetooth.BLE_BLUETOOTH_DISCONNECTED";
    public static final String BLE_CLIENT_ERROR =
            "com.android.cts.verifier.bluetooth.BLE_CLIENT_ERROR";
    public static final String EXTRA_COMMAND =
            "com.android.cts.verifier.bluetooth.EXTRA_COMMAND";
    public static final String EXTRA_WRITE_VALUE =
            "com.android.cts.verifier.bluetooth.EXTRA_WRITE_VALUE";
    public static final String EXTRA_BOOL =
            "com.android.cts.verifier.bluetooth.EXTRA_BOOL";

    // Literal for Client Action
    public static final String BLE_COC_CLIENT_ACTION_LE_INSECURE_CONNECT =
            "com.android.cts.verifier.bluetooth.BLE_COC_CLIENT_ACTION_LE_INSECURE_CONNECT";
    public static final String BLE_COC_CLIENT_ACTION_LE_SECURE_CONNECT =
            "com.android.cts.verifier.bluetooth.BLE_COC_CLIENT_ACTION_LE_SECURE_CONNECT";
    public static final String BLE_COC_CLIENT_ACTION_GET_PSM =
            "com.android.cts.verifier.bluetooth.BLE_COC_CLIENT_ACTION_GET_PSM";
    public static final String BLE_COC_CLIENT_ACTION_COC_CLIENT_CONNECT =
            "com.android.cts.verifier.bluetooth.BLE_COC_CLIENT_ACTION_COC_CLIENT_CONNECT";
    public static final String BLE_COC_CLIENT_ACTION_CHECK_CONNECTION_TYPE =
            "com.android.cts.verifier.bluetooth.BLE_COC_CLIENT_ACTION_CHECK_CONNECTION_TYPE";
    public static final String BLE_COC_CLIENT_ACTION_SEND_DATA_8BYTES =
            "com.android.cts.verifier.bluetooth.BLE_COC_CLIENT_ACTION_SEND_DATA_8BYTES";
    public static final String BLE_COC_CLIENT_ACTION_READ_DATA_8BYTES =
            "com.android.cts.verifier.bluetooth.BLE_COC_CLIENT_ACTION_READ_DATA_8BYTES";
    public static final String BLE_COC_CLIENT_ACTION_EXCHANGE_DATA =
            "com.android.cts.verifier.bluetooth.BLE_COC_CLIENT_ACTION_EXCHANGE_DATA";
    public static final String BLE_COC_CLIENT_ACTION_CLIENT_CONNECT =
            "com.android.cts.verifier.bluetooth.BLE_COC_CLIENT_ACTION_CLIENT_CONNECT";
    public static final String BLE_COC_CLIENT_ACTION_CLIENT_CONNECT_SECURE =
            "com.android.cts.verifier.bluetooth.BLE_COC_CLIENT_ACTION_CLIENT_CONNECT_SECURE";
    public static final String BLE_CLIENT_ACTION_CLIENT_DISCONNECT =
            "com.android.cts.verifier.bluetooth.BLE_CLIENT_ACTION_CLIENT_DISCONNECT";

    private static final UUID SERVICE_UUID =
            UUID.fromString("00009999-0000-1000-8000-00805f9b34fb");

    /**
     * UUID of the GATT Read Characteristics for LE_PSM value.
     */
    public static final UUID LE_PSM_CHARACTERISTIC_UUID =
            UUID.fromString("2d410339-82b6-42aa-b34e-e2e01df8cc1a");

    public static final String WRITE_VALUE = "CLIENT_TEST";
    private static final String NOTIFY_VALUE = "NOTIFY_TEST";
    private int mBleState = BluetoothProfile.STATE_DISCONNECTED;
    private static final int EXECUTION_DELAY = 1500;

    // current test category
    private String mCurrentAction;

    private BluetoothManager mBluetoothManager;
    private BluetoothAdapter mBluetoothAdapter;
    private BluetoothDevice mDevice;
    private BluetoothGatt mBluetoothGatt;
    private BluetoothLeScanner mScanner;
    private Handler mHandler;
    private boolean mSecure;
    private boolean mValidityService;
    private int mPsm;
    private BluetoothChatService mChatService;
    private int mNextReadExpectedLen = -1;
    private String mNextReadCompletionIntent;
    private int mTotalReadLen = 0;
    private byte mNextReadByte;
    private int mNextWriteExpectedLen = -1;
    private String mNextWriteCompletionIntent = null;

    // Handler for communicating task with peer.
    private TestTaskQueue mTaskQueue;

    @Override
    public void onCreate() {
        super.onCreate();

        registerReceiver(mBondStatusReceiver,
                         new IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED));

        mBluetoothManager = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
        mBluetoothAdapter = mBluetoothManager.getAdapter();
        mScanner = mBluetoothAdapter.getBluetoothLeScanner();
        mHandler = new Handler();

        mTaskQueue = new TestTaskQueue(getClass().getName() + "_taskHandlerThread");
    }

    @Override
    public int onStartCommand(final Intent intent, int flags, int startId) {
        if (!mBluetoothAdapter.isEnabled()) {
            notifyBluetoothDisabled();
        } else {
            mTaskQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    onTestFinish(intent.getAction());
                }
            }, EXECUTION_DELAY);
        }
        return START_NOT_STICKY;
    }

    private void onTestFinish(String action) {
        mCurrentAction = action;
        if (mCurrentAction != null) {
            switch (mCurrentAction) {
                case BLE_COC_CLIENT_ACTION_LE_INSECURE_CONNECT:
                    mSecure = false;
                    startScan();
                    break;
                case BLE_COC_CLIENT_ACTION_LE_SECURE_CONNECT:
                    mSecure = true;
                    startScan();
                    break;
                case BLE_COC_CLIENT_ACTION_GET_PSM:
                    startLeDiscovery();
                    break;
                case BLE_COC_CLIENT_ACTION_COC_CLIENT_CONNECT:
                    leCocClientConnect();
                    break;
                case BLE_COC_CLIENT_ACTION_CHECK_CONNECTION_TYPE:
                    leCheckConnectionType();
                    break;
                case BLE_COC_CLIENT_ACTION_SEND_DATA_8BYTES:
                    sendData8bytes();
                    break;
                case BLE_COC_CLIENT_ACTION_READ_DATA_8BYTES:
                    readData8bytes();
                    break;
                case BLE_COC_CLIENT_ACTION_EXCHANGE_DATA:
                    sendDataLargeBuf();
                    readDataLargeBuf();
                    break;
                case BLE_CLIENT_ACTION_CLIENT_DISCONNECT:
                    if (mBluetoothGatt != null) {
                        mBluetoothGatt.disconnect();
                    }
                    if (mChatService != null) {
                        mChatService.stop();
                    }
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
        if (mBluetoothGatt != null) {
            mBluetoothGatt.disconnect();
            mBluetoothGatt.close();
            mBluetoothGatt = null;
        }
        stopScan();
        unregisterReceiver(mBondStatusReceiver);

        if (mChatService != null) {
            mChatService.stop();
        }

        mTaskQueue.quit();
    }

    public static BluetoothGatt connectGatt(BluetoothDevice device, Context context,
                                            boolean autoConnect, boolean isSecure,
                                            BluetoothGattCallback callback) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (isSecure) {
                if (TRANSPORT_MODE_FOR_SECURE_CONNECTION == BluetoothDevice.TRANSPORT_AUTO) {
                    Toast.makeText(context, "connectGatt(transport=AUTO)", Toast.LENGTH_SHORT)
                    .show();
                } else {
                    Toast.makeText(context, "connectGatt(transport=LE)", Toast.LENGTH_SHORT).show();
                }
                return device.connectGatt(context, autoConnect, callback,
                                          TRANSPORT_MODE_FOR_SECURE_CONNECTION);
            } else {
                Toast.makeText(context, "connectGatt(transport=LE)", Toast.LENGTH_SHORT).show();
                return device.connectGatt(context, autoConnect, callback,
                                          BluetoothDevice.TRANSPORT_LE);
            }
        } else {
            Toast.makeText(context, "connectGatt", Toast.LENGTH_SHORT).show();
            return device.connectGatt(context, autoConnect, callback);
        }
    }

    private void readCharacteristic(UUID uuid) {
        BluetoothGattCharacteristic characteristic = getCharacteristic(uuid);
        if (characteristic != null) {
            mBluetoothGatt.readCharacteristic(characteristic);
        }
    }

    private void notifyError(String message) {
        showMessage(message);
        Log.e(TAG, message);

        Intent intent = new Intent(BLE_CLIENT_ERROR);
        sendBroadcast(intent);
    }

    private void notifyMismatchSecure() {
        Intent intent = new Intent(BLE_BLUETOOTH_MISMATCH_SECURE);
        sendBroadcast(intent);
    }

    private void notifyMismatchInsecure() {
        Intent intent = new Intent(BLE_BLUETOOTH_MISMATCH_INSECURE);
        sendBroadcast(intent);
    }

    private void notifyBluetoothDisabled() {
        Intent intent = new Intent(BLE_BLUETOOTH_DISABLED);
        sendBroadcast(intent);
    }

    private void notifyConnected() {
        showMessage("Bluetooth LE GATT connected");
        Intent intent = new Intent(BLE_LE_CONNECTED);
        sendBroadcast(intent);
    }

    private void startLeDiscovery() {
        // Start Service Discovery
        if (mBluetoothGatt != null && mBleState == BluetoothProfile.STATE_CONNECTED) {
            mBluetoothGatt.discoverServices();
        } else {
            showMessage("Bluetooth LE GATT not connected.");
        }
    }

    private void notifyDisconnected() {
        showMessage("Bluetooth LE disconnected");
        Intent intent = new Intent(BLE_BLUETOOTH_DISCONNECTED);
        sendBroadcast(intent);
    }

    private void notifyServicesDiscovered() {
        showMessage("Service discovered");
        // Find the LE_COC_PSM characteristics
        if (DEBUG) {
            Log.d(TAG, "notifyServicesDiscovered: Next step is to read the PSM char.");
        }
        readCharacteristic(LE_PSM_CHARACTERISTIC_UUID);
    }

    private BluetoothGattService getService() {
        BluetoothGattService service = null;

        if (mBluetoothGatt != null) {
            service = mBluetoothGatt.getService(SERVICE_UUID);
            if (service == null) {
                showMessage("GATT Service not found");
            }
        }
        return service;
    }

    private BluetoothGattCharacteristic getCharacteristic(UUID uuid) {
        BluetoothGattCharacteristic characteristic = null;

        BluetoothGattService service = getService();
        if (service != null) {
            characteristic = service.getCharacteristic(uuid);
            if (characteristic == null) {
                showMessage("Characteristic not found");
            }
        }
        return characteristic;
    }

    private void showMessage(final String msg) {
        mHandler.post(new Runnable() {
            public void run() {
                Toast.makeText(BleCocClientService.this, msg, Toast.LENGTH_SHORT).show();
            }
        });
    }

    private final BluetoothGattCallback mGattCallbacks = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            if (DEBUG) {
                Log.d(TAG, "onConnectionStateChange: status=" + status + ", newState=" + newState);
            }
            if (status == BluetoothGatt.GATT_SUCCESS) {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    mBleState = newState;
                    int bondState = gatt.getDevice().getBondState();
                    boolean bonded = false;
                    BluetoothDevice target = gatt.getDevice();
                    Set<BluetoothDevice> pairedDevices = mBluetoothAdapter.getBondedDevices();
                    if (!pairedDevices.isEmpty()) {
                        for (BluetoothDevice device : pairedDevices) {
                            if (device.getAddress().equals(target.getAddress())) {
                                bonded = true;
                                break;
                            }
                        }
                    }
                    if (mSecure && ((bondState == BluetoothDevice.BOND_NONE) || !bonded)) {
                        // not pairing and execute Secure Test
                        Log.e(TAG, "BluetoothGattCallback.onConnectionStateChange: "
                              + "Not paired but execute secure test");
                        mBluetoothGatt.disconnect();
                        notifyMismatchSecure();
                    } else if (!mSecure && ((bondState != BluetoothDevice.BOND_NONE) || bonded)) {
                        // already pairing and execute Insecure Test
                        Log.e(TAG, "BluetoothGattCallback.onConnectionStateChange: "
                              + "Paired but execute insecure test");
                        mBluetoothGatt.disconnect();
                        notifyMismatchInsecure();
                    } else {
                        notifyConnected();
                    }
                } else if (status == BluetoothProfile.STATE_DISCONNECTED) {
                    mBleState = newState;
                    mSecure = false;
                    mBluetoothGatt.close();
                    notifyDisconnected();
                }
            } else {
                showMessage("Failed to connect: " + status + " , newState = " + newState);
                mBluetoothGatt.close();
                mBluetoothGatt = null;
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            if (DEBUG) {
                Log.d(TAG, "onServicesDiscovered: status=" + status);
            }
            if ((status == BluetoothGatt.GATT_SUCCESS) &&
                (mBluetoothGatt.getService(SERVICE_UUID) != null)) {
                notifyServicesDiscovered();
            }
        }

        @Override
        public void onCharacteristicRead(BluetoothGatt gatt,
                BluetoothGattCharacteristic characteristic, int status) {
            UUID uid = characteristic.getUuid();
            if (DEBUG) {
                Log.d(TAG, "onCharacteristicRead: status=" + status + ", uuid=" + uid);
            }
            if (status == BluetoothGatt.GATT_SUCCESS) {
                String value = characteristic.getStringValue(0);
                if (characteristic.getUuid().equals(LE_PSM_CHARACTERISTIC_UUID)) {
                    mPsm = characteristic.getIntValue(BluetoothGattCharacteristic.FORMAT_UINT8, 0);
                    if (DEBUG) {
                        Log.d(TAG, "onCharacteristicRead: reading PSM=" + mPsm);
                    }
                    Intent intent = new Intent(BLE_GOT_PSM);
                    sendBroadcast(intent);
                } else {
                    if (DEBUG) {
                        Log.d(TAG, "onCharacteristicRead: Note: unknown uuid=" + uid);
                    }
                }
            } else if (status == BluetoothGatt.GATT_READ_NOT_PERMITTED) {
                notifyError("Not Permission Read: " + status + " : " + uid);
            } else if (status == BluetoothGatt.GATT_INSUFFICIENT_AUTHENTICATION) {
                notifyError("Not Authentication Read: " + status + " : " + uid);
            } else {
                notifyError("Failed to read characteristic: " + status + " : " + uid);
            }
        }
    };

    private final ScanCallback mScanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            if (mBluetoothGatt == null) {
                // verify the validity of the advertisement packet.
                mValidityService = false;
                List<ParcelUuid> uuids = result.getScanRecord().getServiceUuids();
                for (ParcelUuid uuid : uuids) {
                    if (uuid.getUuid().equals(BleCocServerService.ADV_COC_SERVICE_UUID)) {
                        if (DEBUG) {
                            Log.d(TAG, "onScanResult: Found ADV with LE CoC Service UUID.");
                        }
                        mValidityService = true;
                        break;
                    }
                }
                if (mValidityService) {
                    stopScan();

                    BluetoothDevice device = result.getDevice();
                    if (DEBUG) {
                        Log.d(TAG, "onScanResult: Found ADV with CoC UUID on device="
                              + device);
                    }
                    if (mSecure) {
                        if (device.getBondState() != BluetoothDevice.BOND_BONDED) {
                            if (!device.createBond()) {
                                notifyError("Failed to call create bond");
                            }
                        } else {
                            mDevice = device;
                            mBluetoothGatt = connectGatt(result.getDevice(), BleCocClientService.this, false,
                                                         mSecure, mGattCallbacks);
                        }
                    } else {
                        mDevice = device;
                        mBluetoothGatt = connectGatt(result.getDevice(), BleCocClientService.this, false, mSecure,
                                                     mGattCallbacks);
                    }
                } else {
                    notifyError("No valid service in Advertisement");
                }
            }
        }
    };

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

    private void sendMessage(byte[] buf) {
        mChatService.write(buf);
    }

    private void handleMessageWrite(Message msg) {
        byte[] buffer = (byte[]) msg.obj;
        int len = buffer.length;

        showMessage("LE Coc Client wrote " + len + " bytes");
        if (len == mNextWriteExpectedLen) {
            if (mNextWriteCompletionIntent != null) {
                Intent intent = new Intent(mNextWriteCompletionIntent);
                sendBroadcast(intent);
            }
        } else {
            Log.d(TAG, "handleMessageWrite: unrecognized length=" + len);
        }
    }

    private class ChatHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            if (DEBUG) {
                Log.d(TAG, "ChatHandler.handleMessage: msg=" + msg);
            }
            int state = msg.arg1;
            switch (msg.what) {
            case BluetoothChatService.MESSAGE_STATE_CHANGE:
                if (state == BluetoothChatService.STATE_CONNECTED) {
                    // LE CoC is established
                    notifyLeCocClientConnected();
                }
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

    private void notifyLeCocClientConnected() {
        if (DEBUG) {
            Log.d(TAG, "notifyLeCocClientConnected: device=" + mDevice + ", mSecure=" + mSecure);
        }
        showMessage("Bluetooth LE Coc connected");
        Intent intent = new Intent(BLE_COC_CONNECTED);
        sendBroadcast(intent);
    }

    private void leCocClientConnect() {
        if (DEBUG) {
            Log.d(TAG, "leCocClientConnect: device=" + mDevice + ", mSecure=" + mSecure);
        }
        if (mDevice == null) {
            Log.e(TAG, "leCocClientConnect: mDevice is null");
            return;
        }
        // Construct BluetoothChatService with useBle=true parameter
        mChatService = new BluetoothChatService(this, new ChatHandler(), true);
        mChatService.connect(mDevice, mSecure, mPsm);
    }

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
        showMessage("LE Coc Connection Type Checked");
        Intent intent = new Intent(BLE_CONNECTION_TYPE_CHECKED);
        sendBroadcast(intent);
    }

    private void sendData8bytes() {
        if (DEBUG) Log.d(TAG, "sendData8bytes");

        final byte[] buf = new byte[]{1, 2, 3, 4, 5, 6, 7, 8};
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

    private void readData8bytes() {
        mNextReadExpectedLen = 8;
        mNextReadCompletionIntent = BLE_DATA_8BYTES_READ;
        mNextReadByte = 1;
    }

    private void readDataLargeBuf() {
        mNextReadExpectedLen = BleCocServerService.TEST_DATA_EXCHANGE_BUFSIZE;
        mNextReadCompletionIntent = BLE_DATA_LARGEBUF_READ;
        mNextReadByte = 1;
    }

    private void startScan() {
        if (DEBUG) Log.d(TAG, "startScan");
        List<ScanFilter> filter = Arrays.asList(new ScanFilter.Builder().setServiceUuid(
                new ParcelUuid(BleCocServerService.ADV_COC_SERVICE_UUID)).build());
        ScanSettings setting = new ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build();
        mScanner.startScan(filter, setting, mScanCallback);
    }

    private void stopScan() {
        if (DEBUG) Log.d(TAG, "stopScan");
        if (mScanner != null) {
            mScanner.stopScan(mScanCallback);
        }
    }

    private final BroadcastReceiver mBondStatusReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(BluetoothDevice.ACTION_BOND_STATE_CHANGED)) {
                BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                int state = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,
                                               BluetoothDevice.BOND_NONE);
                switch (state) {
                    case BluetoothDevice.BOND_BONDED:
                        if (mBluetoothGatt == null) {
                            if (DEBUG) {
                                Log.d(TAG, "onReceive:BOND_BONDED: calling connectGatt. device="
                                             + device + ", mSecure=" + mSecure);
                            }
                            mDevice = device;
                            mBluetoothGatt = connectGatt(device, BleCocClientService.this, false, mSecure,
                                                         mGattCallbacks);
                        }
                        break;
                    case BluetoothDevice.BOND_NONE:
                        notifyError("Failed to create bond");
                        break;
                    case BluetoothDevice.BOND_BONDING:
                        // fall through
                    default:
                        // wait for next state
                        break;
                }
            }
        }
    };
}
