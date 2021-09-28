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

import android.annotation.NonNull;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGattCharacteristic;
import android.os.Handler;
import android.util.Log;

import com.android.car.BLEStreamProtos.BLEMessageProto.BLEMessage;
import com.android.car.BLEStreamProtos.BLEOperationProto.OperationType;
import com.android.car.protobuf.InvalidProtocolBufferException;
import com.android.internal.annotations.VisibleForTesting;

import java.io.IOException;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Version 1 of the message stream.
 */
class BleMessageStreamV1 implements BleMessageStream {
    private static final String TAG = "BleMessageStreamV1";

    @VisibleForTesting
    static final int BLE_MESSAGE_RETRY_LIMIT = 5;

    /**
     * The delay in milliseconds before a failed message is retried for sending.
     *
     * <p>This delay is only present for a message has been chunked. Each part of this chunked
     * message requires an ACK from the remote device before the next chunk is sent. If this ACK is
     * not received, then a second message is sent after this delay.
     *
     * <p>The value of this delay is 2 seconds to allow for 1 second to notify the remote device of
     * a new message and 1 second to wait for an ACK.
     */
    private static final long BLE_MESSAGE_RETRY_DELAY_MS = TimeUnit.SECONDS.toMillis(2);

    private final Handler mHandler;
    private final BlePeripheralManager mBlePeripheralManager;
    private final BluetoothDevice mDevice;
    private final BluetoothGattCharacteristic mWriteCharacteristic;
    private final BluetoothGattCharacteristic mReadCharacteristic;

    // Explicitly using an ArrayDequeue here for performance when used as a queue.
    private final Deque<BLEMessage> mMessageQueue = new ArrayDeque<>();
    private final BLEMessagePayloadStream mPayloadStream = new BLEMessagePayloadStream();

    /** The number of times that a message to send has been retried. */
    private int mRetryCount = 0;

    /**
     * The maximum write size for a single message.
     *
     * <p>By default, this value is 20 because the smaller possible write size over BLE is 23 bytes.
     * However, 3 bytes need to be subtracted due to them being used by the header of the BLE
     * packet. Thus, the final value is 20.
     */
    private int mMaxWriteSize = 20;

    private final List<BleMessageStreamCallback> mCallbacks = new ArrayList<>();

    BleMessageStreamV1(@NonNull Handler handler, @NonNull BlePeripheralManager blePeripheralManager,
            @NonNull BluetoothDevice device,
            @NonNull BluetoothGattCharacteristic writeCharacteristic,
            @NonNull BluetoothGattCharacteristic readCharacteristic) {
        mHandler = handler;
        mBlePeripheralManager = blePeripheralManager;
        mDevice = device;
        mWriteCharacteristic = writeCharacteristic;
        mReadCharacteristic = readCharacteristic;

        mBlePeripheralManager.addOnCharacteristicWriteListener(this::onCharacteristicWrite);
    }

    /** Registers the given callback to be notified of various events within the stream. */
    @Override
    public void registerCallback(@NonNull BleMessageStreamCallback callback) {
        mCallbacks.add(callback);
    }

    /** Unregisters the given callback from being notified of stream events. */
    @Override
    public void unregisterCallback(@NonNull BleMessageStreamCallback callback) {
        mCallbacks.remove(callback);
    }

    /** Sets the maximum size of a message that can be sent. */
    @Override
    public void setMaxWriteSize(int maxWriteSize) {
        mMaxWriteSize = maxWriteSize;
    }

    /** Returns the maximum size of a message that can be sent. */
    @Override
    public int getMaxWriteSize() {
        return mMaxWriteSize;
    }

    /**
     * Writes the given message to the write characteristic of this stream.
     *
     * <p>This method will handle the chunking of messages based on maximum write size assigned to
     * this stream.. If there is an error during the send, any callbacks on this stream will be
     * notified of the error.
     *
     * @param message The message to send.
     * @param operationType The {@link OperationType} of this message.
     * @param isPayloadEncrypted {@code true} if the message to send has been encrypted.
     */
    @Override
    public void writeMessage(@NonNull byte[] message, @NonNull OperationType operationType,
            boolean isPayloadEncrypted) {
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "Writing message to device with name: " + mDevice.getName());
        }

        List<BLEMessage> bleMessages = BLEMessageV1Factory.makeBLEMessages(message, operationType,
                mMaxWriteSize, isPayloadEncrypted);

        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "Number of messages to send to device: " + bleMessages.size());
        }

        // Each write will override previous messages.
        if (!mMessageQueue.isEmpty()) {
            mMessageQueue.clear();
            Log.w(TAG, "Request to write a new message when there are still messages in the "
                    + "queue.");
        }

        mMessageQueue.addAll(bleMessages);

        writeNextMessageInQueue();
    }

    /**
     * Processes a message from the client and notifies any callbacks of the success of this
     * call.
     */
    @VisibleForTesting
    void onCharacteristicWrite(@NonNull BluetoothDevice device,
            @NonNull BluetoothGattCharacteristic characteristic, @NonNull byte[] value) {
        if (!mDevice.equals(device)) {
            Log.w(TAG, "Received a message from a device (" + device.getAddress() + ") that is not "
                    + "the expected device (" + mDevice.getAddress() + ") registered to this "
                    + "stream. Ignoring.");
            return;
        }

        if (!characteristic.getUuid().equals(mReadCharacteristic.getUuid())) {
            Log.w(TAG, "Received a write to a characteristic (" + characteristic.getUuid()
                    + ") that is not the expected UUID (" + mReadCharacteristic.getUuid()
                    + "). Ignoring.");
            return;
        }

        BLEMessage bleMessage;
        try {
            bleMessage = BLEMessage.parseFrom(value);
        } catch (InvalidProtocolBufferException e) {
            Log.e(TAG, "Can not parse BLE message from client.", e);

            for (BleMessageStreamCallback callback : mCallbacks) {
                callback.onMessageReceivedError(characteristic.getUuid());
            }
            return;
        }

        if (bleMessage.getOperation() == OperationType.ACK) {
            handleClientAckMessage();
            return;
        }

        try {
            mPayloadStream.write(bleMessage);
        } catch (IOException e) {
            Log.e(TAG, "Unable to parse the BLE message's payload from client.", e);

            for (BleMessageStreamCallback callback : mCallbacks) {
                callback.onMessageReceivedError(characteristic.getUuid());
            }
            return;
        }

        // If it's not complete, make sure the client knows that this message was received.
        if (!mPayloadStream.isComplete()) {
            sendAcknowledgmentMessage();
            return;
        }

        for (BleMessageStreamCallback callback : mCallbacks) {
            callback.onMessageReceived(mPayloadStream.toByteArray(), characteristic.getUuid());
        }

        mPayloadStream.reset();
    }

    /**
     * Writes the next message in the message queue to the write characteristic.
     *
     * <p>If the message queue is empty, then this method will do nothing.
     */
    private void writeNextMessageInQueue() {
        // This should not happen in practice since this method is private and should only be called
        // for a non-empty queue.
        if (mMessageQueue.isEmpty()) {
            Log.e(TAG, "Call to write next message in queue, but the message queue is empty.");
            return;
        }

        if (mMessageQueue.size() == 1) {
            writeValueAndNotify(mMessageQueue.remove().toByteArray());
            return;
        }

        mHandler.post(mSendMessageWithTimeoutRunnable);
    }

    private void handleClientAckMessage() {
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "Received ACK from client. Attempting to write next message in queue.");
        }

        mHandler.removeCallbacks(mSendMessageWithTimeoutRunnable);
        mRetryCount = 0;

        if (mMessageQueue.isEmpty()) {
            Log.e(TAG, "Received ACK, but the message queue is empty. Ignoring.");
            return;
        }

        // Previous message has been sent successfully so we can start the next message.
        mMessageQueue.remove();
        writeNextMessageInQueue();
    }

    private void sendAcknowledgmentMessage() {
        writeValueAndNotify(BLEMessageV1Factory.makeAcknowledgementMessage().toByteArray());
    }

    /**
     * Convenience method to write the given message to the {@link #mWriteCharacteristic} of this
     * class. After writing, this method will also send notifications to any listening devices that
     * the write was made.
     */
    private void writeValueAndNotify(@NonNull byte[] message) {
        mWriteCharacteristic.setValue(message);

        mBlePeripheralManager.notifyCharacteristicChanged(mDevice, mWriteCharacteristic,
                /* confirm= */ false);
    }

    /**
     * A runnable that will write the message at the head of the {@link #mMessageQueue} and set up
     * a timeout for receiving an ACK for that write.
     *
     * <p>If the timeout is reached before an ACK is received, the message write is retried.
     */
    private final Runnable mSendMessageWithTimeoutRunnable = new Runnable() {
        @Override
        public void run() {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Sending BLE message; retry count: " + mRetryCount);
            }

            if (mRetryCount < BLE_MESSAGE_RETRY_LIMIT) {
                writeValueAndNotify(mMessageQueue.peek().toByteArray());
                mRetryCount++;
                mHandler.postDelayed(this, BLE_MESSAGE_RETRY_DELAY_MS);
                return;
            }

            mHandler.removeCallbacks(this);
            mRetryCount = 0;
            mMessageQueue.clear();

            Log.e(TAG, "Error during BLE message sending - exceeded retry limit.");

            for (BleMessageStreamCallback callback : mCallbacks) {
                callback.onWriteMessageError();
            }
        }
    };
}
