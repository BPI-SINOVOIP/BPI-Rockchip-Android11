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

import android.annotation.IntDef;
import android.annotation.Nullable;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.content.Context;
import android.os.Handler;
import android.os.ParcelUuid;
import android.util.Log;

import androidx.collection.SimpleArrayMap;

import com.android.car.BLEStreamProtos.BLEOperationProto.OperationType;
import com.android.car.BLEStreamProtos.VersionExchangeProto.BLEVersionExchange;
import com.android.car.R;
import com.android.car.Utils;
import com.android.car.protobuf.InvalidProtocolBufferException;
import com.android.internal.annotations.VisibleForTesting;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.TimeUnit;

/**
 * A BLE Service that is used for communicating with the trusted peer device. This extends from a
 * more generic {@link BlePeripheralManager} and has more context on the BLE requirements for the
 * Trusted device feature. It has knowledge on the GATT services and characteristics that are
 * specific to the Trusted Device feature.
 */
class CarTrustAgentBleManager implements BleMessageStreamCallback, BlePeripheralManager.Callback,
        BlePeripheralManager.OnCharacteristicWriteListener {
    private static final String TAG = "CarTrustBLEManager";

    /**
     * The UUID of the Client Characteristic Configuration Descriptor. This descriptor is
     * responsible for specifying if a characteristic can be subscribed to for notifications.
     *
     * @see <a href="https://www.bluetooth.com/specifications/gatt/descriptors/">
     * GATT Descriptors</a>
     */
    private static final UUID CLIENT_CHARACTERISTIC_CONFIG =
            UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");

    /**
     * Reserved bytes for an ATT write request payload.
     *
     * <p>The attribute protocol uses 3 bytes to encode the command type and attribute ID. These
     * bytes need to be subtracted from the reported MTU size and the resulting value will
     * represent the total amount of bytes that can be sent in a write.
     */
    private static final int ATT_PAYLOAD_RESERVED_BYTES = 3;

    /** @hide */
    @IntDef(prefix = {"TRUSTED_DEVICE_OPERATION_"}, value = {
            TRUSTED_DEVICE_OPERATION_NONE,
            TRUSTED_DEVICE_OPERATION_ENROLLMENT,
            TRUSTED_DEVICE_OPERATION_UNLOCK
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TrustedDeviceOperation {
    }

    private static final int TRUSTED_DEVICE_OPERATION_NONE = 0;
    private static final int TRUSTED_DEVICE_OPERATION_ENROLLMENT = 1;
    private static final int TRUSTED_DEVICE_OPERATION_UNLOCK = 2;
    @VisibleForTesting
    static final long BLE_MESSAGE_RETRY_DELAY_MS = TimeUnit.SECONDS.toMillis(2);

    private final Context mContext;
    private final BlePeripheralManager mBlePeripheralManager;

    @TrustedDeviceOperation
    private int mCurrentTrustedDeviceOperation = TRUSTED_DEVICE_OPERATION_NONE;
    private String mOriginalBluetoothName;
    private byte[] mUniqueId;

    /**
     * The maximum amount of bytes that can be written over BLE.
     *
     * <p>This initial value is 20 because BLE has a default write of 23 bytes. However, 3 bytes
     * are subtracted due to bytes being reserved for the command type and attribute ID.
     *
     * @see #ATT_PAYLOAD_RESERVED_BYTES
     */
    private int mMaxWriteSize = 20;

    @VisibleForTesting
    int mBleMessageRetryLimit = 20;

    private final List<BleEventCallback> mBleEventCallbacks = new ArrayList<>();
    private AdvertiseCallback mEnrollmentAdvertisingCallback;
    private SendMessageCallback mSendMessageCallback;

    // Enrollment Service and Characteristic UUIDs
    private UUID mEnrollmentServiceUuid;
    private UUID mEnrollmentClientWriteUuid;
    private UUID mEnrollmentServerWriteUuid;
    private BluetoothGattService mEnrollmentGattService;

    // Unlock Service and Characteristic UUIDs
    private UUID mUnlockServiceUuid;
    private UUID mUnlockClientWriteUuid;
    private UUID mUnlockServerWriteUuid;
    private BluetoothGattService mUnlockGattService;

    @Nullable
    private BleMessageStream mMessageStream;
    private final Handler mHandler = new Handler();

    // A map of enrollment/unlock client write uuid -> listener
    private final SimpleArrayMap<UUID, DataReceivedListener> mDataReceivedListeners =
            new SimpleArrayMap<>();

    CarTrustAgentBleManager(Context context, BlePeripheralManager blePeripheralManager) {
        mContext = context;
        mBlePeripheralManager = blePeripheralManager;
        mBlePeripheralManager.registerCallback(this);
    }

    /**
     * This should be called before starting unlock advertising
     */
    void setUniqueId(UUID uniqueId) {
        mUniqueId = Utils.uuidToBytes(uniqueId);
    }

    void cleanup() {
        mBlePeripheralManager.cleanup();
    }

    void stopGattServer() {
        mBlePeripheralManager.stopGattServer();
    }

    // Overriding some of the {@link BLEManager} methods to be specific for Trusted Device feature.
    @Override
    public void onRemoteDeviceConnected(BluetoothDevice device) {
        if (mBleEventCallbacks.isEmpty()) {
            Log.e(TAG, "No valid BleEventCallback for trust device.");
            return;
        }

        // Retrieving device name only happens in enrollment, the retrieved device name will be
        // stored in sharedPreference for further use.
        if (mCurrentTrustedDeviceOperation == TRUSTED_DEVICE_OPERATION_ENROLLMENT
                && device.getName() == null) {
            mBlePeripheralManager.retrieveDeviceName(device);
        }

        if (mMessageStream != null) {
            mMessageStream.unregisterCallback(this);
            mMessageStream = null;
        }

        mSendMessageCallback = null;

        mBlePeripheralManager.addOnCharacteristicWriteListener(this);
        mBleEventCallbacks.forEach(bleEventCallback ->
                bleEventCallback.onRemoteDeviceConnected(device));
    }

    @Override
    public void onRemoteDeviceDisconnected(BluetoothDevice device) {
        mBlePeripheralManager.removeOnCharacteristicWriteListener(this);
        mBleEventCallbacks.forEach(bleEventCallback ->
                bleEventCallback.onRemoteDeviceDisconnected(device));

        if (mMessageStream != null) {
            mMessageStream.unregisterCallback(this);
            mMessageStream = null;
        }

        mSendMessageCallback = null;
    }

    @Override
    public void onDeviceNameRetrieved(@Nullable String deviceName) {
        mBleEventCallbacks.forEach(bleEventCallback ->
                bleEventCallback.onClientDeviceNameRetrieved(deviceName));
    }

    @Override
    public void onMtuSizeChanged(int size) {
        mMaxWriteSize = size - ATT_PAYLOAD_RESERVED_BYTES;

        if (mMessageStream != null) {
            mMessageStream.setMaxWriteSize(mMaxWriteSize);
        }

        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "MTU size changed to: " + size
                    + "; setting max payload size to: " + mMaxWriteSize);
        }
    }

    @Override
    public void onCharacteristicWrite(BluetoothDevice device,
            BluetoothGattCharacteristic characteristic, byte[] value) {
        UUID uuid = characteristic.getUuid();
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "onCharacteristicWrite received uuid: " + uuid);
        }

        if (mMessageStream == null) {
            resolveBLEVersion(device, value, uuid);
            return;
        }

        Log.e(TAG, "Received a message but message stream has already been created. "
                + "Was this manager not unregistered as a listener for writes?");
    }

    @VisibleForTesting
    void setBleMessageRetryLimit(int limit) {
        mBleMessageRetryLimit = limit;
    }

    private void resolveBLEVersion(BluetoothDevice device, byte[] value,
            UUID clientCharacteristicUUID) {
        BluetoothGattCharacteristic writeCharacteristic =
                getCharacteristicForWrite(clientCharacteristicUUID);

        if (writeCharacteristic == null) {
            Log.e(TAG, "Invalid write UUID (" + clientCharacteristicUUID
                    + ") during version exchange; disconnecting from remote device.");
            disconnectRemoteDevice();
            return;
        }

        BluetoothGattCharacteristic readCharacteristic =
                clientCharacteristicUUID.equals(mEnrollmentClientWriteUuid)
                        ? mEnrollmentGattService.getCharacteristic(clientCharacteristicUUID)
                        : mUnlockGattService.getCharacteristic(clientCharacteristicUUID);

        // If this occurs, then there is a bug in the retrieval code above.
        if (readCharacteristic == null) {
            Log.e(TAG, "No read characteristic corresponding to UUID ("
                    + clientCharacteristicUUID + "). Cannot listen for messages. Disconnecting.");
            disconnectRemoteDevice();
            return;
        }

        BLEVersionExchange deviceVersion;
        try {
            deviceVersion = BLEVersionExchange.parseFrom(value);
        } catch (InvalidProtocolBufferException e) {
            disconnectRemoteDevice();
            Log.e(TAG, "Could not parse version exchange message", e);
            return;
        }

        mMessageStream = BLEVersionExchangeResolver.resolveToStream(
                deviceVersion, device, mBlePeripheralManager, writeCharacteristic,
                readCharacteristic);
        mMessageStream.setMaxWriteSize(mMaxWriteSize);
        mMessageStream.registerCallback(this);

        if (mMessageStream == null) {
            Log.e(TAG, "No supported version found during version exchange. "
                    +  "Could not create message stream.");
            disconnectRemoteDevice();
            return;
        }

        // No need for this manager to listen for any writes; the stream will handle that from now
        // on.
        mBlePeripheralManager.removeOnCharacteristicWriteListener(this);

        // The message stream is not used to send the IHU's version, but will be used for
        // any subsequent messages.
        BLEVersionExchange headunitVersion = BLEVersionExchangeResolver.makeVersionExchange();
        setValueOnCharacteristicAndNotify(device, headunitVersion.toByteArray(),
                writeCharacteristic);

        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "Sent supported version to the phone.");
        }
    }

    /**
     * Setup the BLE GATT server for Enrollment. The GATT server for Enrollment comprises of one
     * GATT Service and 2 characteristics - one for the phone to write to and one for the head unit
     * to write to.
     */
    void setupEnrollmentBleServer() {
        mEnrollmentServiceUuid = UUID.fromString(
                mContext.getString(R.string.enrollment_service_uuid));
        mEnrollmentClientWriteUuid = UUID.fromString(
                mContext.getString(R.string.enrollment_client_write_uuid));
        mEnrollmentServerWriteUuid = UUID.fromString(
                mContext.getString(R.string.enrollment_server_write_uuid));

        mEnrollmentGattService = new BluetoothGattService(mEnrollmentServiceUuid,
                BluetoothGattService.SERVICE_TYPE_PRIMARY);

        // Characteristic the connected bluetooth device will write to.
        BluetoothGattCharacteristic clientCharacteristic =
                new BluetoothGattCharacteristic(mEnrollmentClientWriteUuid,
                        BluetoothGattCharacteristic.PROPERTY_WRITE
                                | BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE,
                        BluetoothGattCharacteristic.PERMISSION_WRITE);

        // Characteristic that this manager will write to.
        BluetoothGattCharacteristic serverCharacteristic =
                new BluetoothGattCharacteristic(mEnrollmentServerWriteUuid,
                        BluetoothGattCharacteristic.PROPERTY_NOTIFY,
                        BluetoothGattCharacteristic.PERMISSION_READ);

        addDescriptorToCharacteristic(serverCharacteristic);

        mEnrollmentGattService.addCharacteristic(clientCharacteristic);
        mEnrollmentGattService.addCharacteristic(serverCharacteristic);
    }

    /**
     * Setup the BLE GATT server for Unlocking the Head unit. The GATT server for this phase also
     * comprises of 1 Service and 2 characteristics. However both the token and the handle are sent
     * from the phone to the head unit.
     */
    void setupUnlockBleServer() {
        mUnlockServiceUuid = UUID.fromString(mContext.getString(R.string.unlock_service_uuid));
        mUnlockClientWriteUuid = UUID
                .fromString(mContext.getString(R.string.unlock_client_write_uuid));
        mUnlockServerWriteUuid = UUID
                .fromString(mContext.getString(R.string.unlock_server_write_uuid));

        mUnlockGattService = new BluetoothGattService(mUnlockServiceUuid,
                BluetoothGattService.SERVICE_TYPE_PRIMARY);

        // Characteristic the connected bluetooth device will write to.
        BluetoothGattCharacteristic clientCharacteristic = new BluetoothGattCharacteristic(
                mUnlockClientWriteUuid,
                BluetoothGattCharacteristic.PROPERTY_WRITE
                        | BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE,
                BluetoothGattCharacteristic.PERMISSION_WRITE);

        // Characteristic that this manager will write to.
        BluetoothGattCharacteristic serverCharacteristic = new BluetoothGattCharacteristic(
                mUnlockServerWriteUuid,
                BluetoothGattCharacteristic.PROPERTY_NOTIFY,
                BluetoothGattCharacteristic.PERMISSION_READ);

        addDescriptorToCharacteristic(serverCharacteristic);

        mUnlockGattService.addCharacteristic(clientCharacteristic);
        mUnlockGattService.addCharacteristic(serverCharacteristic);
    }

    @Override
    public void onMessageReceivedError(UUID uuid) {
        Log.e(TAG, "Error parsing the message from the client on UUID: " + uuid);
    }

    @Override
    public void onMessageReceived(byte[] message, UUID uuid) {
        if (mDataReceivedListeners.containsKey(uuid)) {
            mDataReceivedListeners.get(uuid).onDataReceived(message);
        }
    }

    @Override
    public void onWriteMessageError() {
        if (mSendMessageCallback != null) {
            mSendMessageCallback.onSendMessageFailure();
        }
    }

    private void addDescriptorToCharacteristic(BluetoothGattCharacteristic characteristic) {
        BluetoothGattDescriptor descriptor = new BluetoothGattDescriptor(
                CLIENT_CHARACTERISTIC_CONFIG,
                BluetoothGattDescriptor.PERMISSION_READ | BluetoothGattDescriptor.PERMISSION_WRITE);
        descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
        characteristic.addDescriptor(descriptor);
    }

    /**
     * Begins advertising for enrollment
     *
     * @param deviceName device name to advertise
     * @param enrollmentAdvertisingCallback callback for advertiser
     */
    void startEnrollmentAdvertising(@Nullable String deviceName,
            AdvertiseCallback enrollmentAdvertisingCallback) {
        if (enrollmentAdvertisingCallback == null) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Enrollment Advertising not started: "
                        + "enrollmentAdvertisingCallback is null");
            }
            return;
        }
        mCurrentTrustedDeviceOperation = TRUSTED_DEVICE_OPERATION_ENROLLMENT;
        mEnrollmentAdvertisingCallback = enrollmentAdvertisingCallback;
        // Replace name to ensure it is small enough to be advertised
        if (deviceName != null) {
            BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
            if (mOriginalBluetoothName == null) {
                mOriginalBluetoothName = adapter.getName();
            }
            adapter.setName(deviceName);
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Changing bluetooth adapter name from "
                        + mOriginalBluetoothName + " to " + deviceName);
            }
        }

        attemptAdvertising();
    }

    private void attemptAdvertising() {
        // Validate the adapter name change has happened. If not, try again after delay.
        if (mOriginalBluetoothName != null
                && BluetoothAdapter.getDefaultAdapter().getName().equals(mOriginalBluetoothName)) {
            mHandler.postDelayed(this::attemptAdvertising, BLE_MESSAGE_RETRY_DELAY_MS);
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Adapter name change has not taken affect prior to advertising attempt. "
                        + "Trying again.");
            }
            return;
        }

        mBlePeripheralManager.startAdvertising(mEnrollmentGattService,
                new AdvertiseData.Builder()
                        .setIncludeDeviceName(true)
                        .addServiceUuid(new ParcelUuid(mEnrollmentServiceUuid))
                        .build(),
                mEnrollmentAdvertisingCallback);
    }

    void stopEnrollmentAdvertising() {
        if (mOriginalBluetoothName != null) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Changing bluetooth adapter name back to "
                        + mOriginalBluetoothName);
            }
            BluetoothAdapter.getDefaultAdapter().setName(mOriginalBluetoothName);
            mOriginalBluetoothName = null;
        }
        if (mEnrollmentAdvertisingCallback != null) {
            mBlePeripheralManager.stopAdvertising(mEnrollmentAdvertisingCallback);
        }
    }

    void startUnlockAdvertising() {
        if (mUniqueId == null) {
            Log.e(TAG, "unique id is null");
            return;
        }
        mCurrentTrustedDeviceOperation = TRUSTED_DEVICE_OPERATION_UNLOCK;
        mBlePeripheralManager.startAdvertising(mUnlockGattService,
                new AdvertiseData.Builder()
                        .setIncludeDeviceName(false)
                        .addServiceData(new ParcelUuid(mUnlockServiceUuid), mUniqueId)
                        .addServiceUuid(new ParcelUuid(mUnlockServiceUuid))
                        .build(),
                mUnlockAdvertisingCallback);
    }

    void stopUnlockAdvertising() {
        mCurrentTrustedDeviceOperation = TRUSTED_DEVICE_OPERATION_NONE;
        mBlePeripheralManager.stopAdvertising(mUnlockAdvertisingCallback);
    }

    void disconnectRemoteDevice() {
        mBlePeripheralManager.stopGattServer();
    }

    void sendMessage(byte[] message, OperationType operation, boolean isPayloadEncrypted,
            SendMessageCallback callback) {
        if (mMessageStream == null) {
            Log.e(TAG, "Request to send message, but no valid message stream.");
            return;
        }

        mSendMessageCallback = callback;

        mMessageStream.writeMessage(message, operation, isPayloadEncrypted);
    }

    /**
     * Sets the given message on the specified characteristic.
     *
     * <p>Upon successfully setting of the value, any listeners on the characteristic will be
     * notified that its value has changed.
     *
     * @param device         The device has own the given characteristic.
     * @param message        The message to set as the characteristic's value.
     * @param characteristic The characteristic to set the value on.
     */
    private void setValueOnCharacteristicAndNotify(BluetoothDevice device, byte[] message,
            BluetoothGattCharacteristic characteristic) {
        characteristic.setValue(message);
        mBlePeripheralManager.notifyCharacteristicChanged(device, characteristic, false);
    }

    /**
     * Returns the characteristic that can be written to based on the given UUID.
     *
     * <p>The UUID will be one that corresponds to either enrollment or unlock. This method will
     * return the write characteristic for enrollment or unlock respectively.
     *
     * @return The write characteristic or {@code null} if the UUID is invalid.
     */
    @Nullable
    private BluetoothGattCharacteristic getCharacteristicForWrite(UUID uuid) {
        if (uuid.equals(mEnrollmentClientWriteUuid)) {
            return mEnrollmentGattService.getCharacteristic(mEnrollmentServerWriteUuid);
        }

        if (uuid.equals(mUnlockClientWriteUuid)) {
            return mUnlockGattService.getCharacteristic(mUnlockServerWriteUuid);
        }

        return null;
    }

    private final AdvertiseCallback mUnlockAdvertisingCallback = new AdvertiseCallback() {
        @Override
        public void onStartSuccess(AdvertiseSettings settingsInEffect) {
            super.onStartSuccess(settingsInEffect);
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Unlock Advertising onStartSuccess");
            }
        }

        @Override
        public void onStartFailure(int errorCode) {
            Log.e(TAG, "Failed to advertise, errorCode: " + errorCode);
            super.onStartFailure(errorCode);
            if (errorCode == AdvertiseCallback.ADVERTISE_FAILED_ALREADY_STARTED) {
                return;
            }
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Start unlock advertising fail, retry to advertising..");
            }
            setupUnlockBleServer();
            startUnlockAdvertising();
        }
    };

    /**
     * Adds the given listener to be notified when the characteristic with the given uuid has
     * received data.
     */
    void addDataReceivedListener(UUID uuid, DataReceivedListener listener) {
        mDataReceivedListeners.put(uuid, listener);
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "Register DataReceivedListener: " + listener + "for uuid "
                    + uuid.toString());
        }
    }

    void removeDataReceivedListener(UUID uuid) {
        if (!mDataReceivedListeners.containsKey(uuid)) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "No DataReceivedListener for uuid " + uuid.toString()
                        + " to unregister.");
            }
            return;
        }
        mDataReceivedListeners.remove(uuid);
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "Unregister DataReceivedListener for uuid " + uuid.toString());
        }
    }

    void addBleEventCallback(BleEventCallback callback) {
        mBleEventCallbacks.add(callback);
    }

    void removeBleEventCallback(BleEventCallback callback) {
        if (!mBleEventCallbacks.remove(callback)) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Remove BleEventCallback that does not exist.");
            }
        }
    }

    /**
     * Callback to be invoked for enrollment events
     */
    interface SendMessageCallback {

        /**
         * Called when enrollment handshake needs to be terminated
         */
        void onSendMessageFailure();
    }

    /**
     * Callback to be invoked when BLE receives data from remote device.
     */
    interface DataReceivedListener {

        /**
         * Called when data has been received from a remote device.
         *
         * @param value received data
         */
        void onDataReceived(byte[] value);
    }

    /**
     * The interface that device service has to implement to get notified when BLE events occur
     */
    interface BleEventCallback {

        /**
         * Called when a remote device is connected
         *
         * @param device the remote device
         */
        void onRemoteDeviceConnected(BluetoothDevice device);

        /**
         * Called when a remote device is disconnected
         *
         * @param device the remote device
         */
        void onRemoteDeviceDisconnected(BluetoothDevice device);

        /**
         * Called when the device name of the remote device is retrieved
         *
         * @param deviceName device name of the remote device
         */
        void onClientDeviceNameRetrieved(String deviceName);
    }
}
