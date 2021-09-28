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

import com.android.car.BLEStreamProtos.BLEOperationProto.OperationType;

/**
 * Handles the streaming of BLE messages to a specific {@link android.bluetooth.BluetoothDevice}.
 *
 * <p>This stream will handle if messages to a particular peripheral need to be split into
 * multiple messages or if the messages can be sent all at once. Internally, it will have its own
 * protocol for how the split messages are structured.
 */
interface BleMessageStream {
   /** Registers the given callback to be notified of various events within the stream. */
    void registerCallback(@NonNull BleMessageStreamCallback callback);

    /** Unregisters the given callback from being notified of stream events. */
    void unregisterCallback(@NonNull BleMessageStreamCallback callback);

    /** Sets the maximum size of a message that can be sent. */
    void setMaxWriteSize(int maxWriteSize);

    /** Returns the maximum size of a message that can be sent. */
    int getMaxWriteSize();

    /**
     * Writes the given message to the write characteristic set on this stream to the
     * {@code BleutoothDevice} associated with this stream.
     *
     * <p>The given message will adhere to the max write size set on this stream. If the message is
     * larger than this size, then this stream should take the appropriate actions necessary to
     * chunk the message to the device so that no parts of the message is dropped.
     *
     * <p>If there was an error, then this stream will notify the [callback] of this stream via a
     * call to its {@code onWriteMessageError} method.
     *
     * @param message The message to send.
     * @param operationType The {@link OperationType} of this message.
     * @param isPayloadEncrypted {@code true} if the message to send has been encrypted.
     */
    void writeMessage(@NonNull byte[] message, @NonNull OperationType operationType,
            boolean isPayloadEncrypted);
}
