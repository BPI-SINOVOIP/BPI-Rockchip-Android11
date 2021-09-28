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

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Matchers.anyLong;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGattCharacteristic;
import android.os.Handler;

import com.android.car.BLEStreamProtos.BLEMessageProto.BLEMessage;
import com.android.car.BLEStreamProtos.BLEOperationProto.OperationType;
import com.android.car.protobuf.InvalidProtocolBufferException;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnitRunner;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;
import java.util.UUID;

/**
 * Unit test for the {@link BleMessageStreamV1}.
 *
 * <p>Run:
 * {@code atest CarServiceUnitTest:BleMessageStreamV1Test}
 */
@RunWith(MockitoJUnitRunner.class)
public class BleMessageStreamV1Test {
    private static final String ADDRESS_MOCK = "00:11:22:33:AA:BB";

    // The UUID values here are arbitrary.
    private static final UUID WRITE_UUID = UUID.fromString("9a138a69-7c29-400f-9e71-fc29516f9f8b");
    private static final UUID READ_UUID = UUID.fromString("3e344860-e688-4cce-8411-16161b61ad57");

    private BleMessageStreamV1 mBleMessageStream;
    private BluetoothDevice mBluetoothDevice;

    @Mock BlePeripheralManager mBlePeripheralManager;
    @Mock BleMessageStreamCallback mCallbackMock;
    @Mock Handler mHandlerMock;
    @Mock BluetoothGattCharacteristic mWriteCharacteristicMock;
    @Mock BluetoothGattCharacteristic mReadCharacteristicMock;

    @Before
    public void setUp() {
        // Mock so that handler will run anything that is posted to it.
        when(mHandlerMock.post(any(Runnable.class))).thenAnswer(invocation -> {
            invocation.<Runnable>getArgument(0).run();
            return null;
        });

        // Ensure the mock characteristics return valid UUIDs.
        when(mWriteCharacteristicMock.getUuid()).thenReturn(WRITE_UUID);
        when(mReadCharacteristicMock.getUuid()).thenReturn(READ_UUID);

        mBluetoothDevice = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(ADDRESS_MOCK);
        mBleMessageStream = new BleMessageStreamV1(
                mHandlerMock, mBlePeripheralManager, mBluetoothDevice, mWriteCharacteristicMock,
                mReadCharacteristicMock);
        mBleMessageStream.registerCallback(mCallbackMock);
    }

    @Test
    public void writeMessage_noChunkingRequired_sendsCorrectMessage() {
        // Ensure that there is enough space to fit the message.
        mBleMessageStream.setMaxWriteSize(512);

        byte[] message = "message".getBytes();
        boolean isPayloadEncrypted = true;
        OperationType operationType = OperationType.CLIENT_MESSAGE;

        mBleMessageStream.writeMessage(message, operationType, isPayloadEncrypted);

        BLEMessage expectedMessage = BLEMessageV1Factory.makeBLEMessage(
                message, operationType, isPayloadEncrypted);

        // Verify that the message was written.
        verify(mWriteCharacteristicMock).setValue(expectedMessage.toByteArray());

        // Verify that there is also a notification of the characteristic change.
        verify(mBlePeripheralManager).notifyCharacteristicChanged(mBluetoothDevice,
                mWriteCharacteristicMock, false);
    }

    @Test
    public void writeMessage_chunkingRequired_sendsCorrectMessage()
            throws InvalidProtocolBufferException, IOException {
        boolean isPayloadEncrypted = true;
        OperationType operationType = OperationType.CLIENT_MESSAGE;

        int maxSize = 20;
        int requiredWrites = 9;
        byte[] message = makeMessage(requiredWrites * maxSize);
        int headerSize = BLEMessageV1Factory.getProtoHeaderSize(
                operationType, message.length, isPayloadEncrypted);

        // Make sure the payload can't fit into one chunk.
        mBleMessageStream.setMaxWriteSize(maxSize + headerSize);
        mBleMessageStream.writeMessage(message, operationType, isPayloadEncrypted);

        // Each part of the message requires an ACK except the last one.
        int numOfAcks = requiredWrites - 1;

        for (int i = 0; i < numOfAcks; i++) {
            mBleMessageStream.onCharacteristicWrite(
                    mBluetoothDevice,
                    mReadCharacteristicMock,
                    BLEMessageV1Factory.makeAcknowledgementMessage().toByteArray());
        }

        // Each ACK should trigger a canceling of the retry runnable.
        verify(mHandlerMock, times(numOfAcks)).removeCallbacks(any(Runnable.class));

        ArgumentCaptor<byte[]> messageCaptor = ArgumentCaptor.forClass(byte[].class);

        verify(mWriteCharacteristicMock, times(requiredWrites)).setValue(
                messageCaptor.capture());
        verify(mBlePeripheralManager, times(requiredWrites))
                .notifyCharacteristicChanged(mBluetoothDevice,
                        mWriteCharacteristicMock, false);

        List<byte[]> writtenBytes = messageCaptor.getAllValues();
        ByteArrayOutputStream reassembledMessageStream = new ByteArrayOutputStream();

        // Verify the packet numbers.
        for (int i = 0; i < writtenBytes.size(); i++) {
            BLEMessage bleMessage = BLEMessage.parseFrom(writtenBytes.get(i));

            assertThat(bleMessage.getPacketNumber()).isEqualTo(i + 1);
            assertThat(bleMessage.getTotalPackets()).isEqualTo(writtenBytes.size());
            assertThat(bleMessage.getIsPayloadEncrypted()).isTrue();

            reassembledMessageStream.write(bleMessage.getPayload().toByteArray());
        }

        // Verify the reassembled message.
        assertThat(reassembledMessageStream.toByteArray()).isEqualTo(message);
    }

    @Test
    public void writeMessage_chunkingRequired_retriesSamePayloadIfNoAckReceived()
            throws InvalidProtocolBufferException, IOException {
        // Execute delayed runnables immediately to simulate an ACK timeout.
        mockHandlerToExecuteDelayedRunnablesImmediately();

        boolean isPayloadEncrypted = true;
        OperationType operationType = OperationType.CLIENT_MESSAGE;

        int maxSize = 20;
        int requiredWrites = 9;
        byte[] message = makeMessage(requiredWrites * maxSize);
        int headerSize = BLEMessageV1Factory.getProtoHeaderSize(
                operationType, message.length, isPayloadEncrypted);

        // Make sure the payload can't fit into one chunk.
        mBleMessageStream.setMaxWriteSize(maxSize + headerSize);
        mBleMessageStream.writeMessage(message, operationType, isPayloadEncrypted);

        ArgumentCaptor<byte[]> messageCaptor = ArgumentCaptor.forClass(byte[].class);

        // Because there is no ACK, the write should be retried up to the limit.
        verify(mWriteCharacteristicMock, times(BleMessageStreamV1.BLE_MESSAGE_RETRY_LIMIT))
                .setValue(messageCaptor.capture());
        verify(mBlePeripheralManager, times(BleMessageStreamV1.BLE_MESSAGE_RETRY_LIMIT))
                .notifyCharacteristicChanged(mBluetoothDevice, mWriteCharacteristicMock, false);

        List<byte[]> writtenBytes = messageCaptor.getAllValues();
        List<byte[]> writtenPayloads = new ArrayList<>();

        for (int i = 0; i < writtenBytes.size(); i++) {
            BLEMessage bleMessage = BLEMessage.parseFrom(writtenBytes.get(i));

            // The same packet should be written.
            assertThat(bleMessage.getPacketNumber()).isEqualTo(1);
            assertThat(bleMessage.getTotalPackets()).isEqualTo(requiredWrites);
            assertThat(bleMessage.getIsPayloadEncrypted()).isTrue();

            writtenPayloads.add(bleMessage.getPayload().toByteArray());
        }

        // Verify that the same payload is being written.
        for (byte[] payload : writtenPayloads) {
            assertThat(payload).isEqualTo(writtenPayloads.get(0));
        }
    }

    @Test
    public void writeMessage_chunkingRequired_notifiesCallbackIfNoAck()
            throws InvalidProtocolBufferException, IOException {
        // Execute delayed runnables immediately to simulate an ACK timeout.
        mockHandlerToExecuteDelayedRunnablesImmediately();

        boolean isPayloadEncrypted = true;
        OperationType operationType = OperationType.CLIENT_MESSAGE;

        int maxSize = 20;
        int requiredWrites = 9;
        byte[] message = makeMessage(requiredWrites * maxSize);
        int headerSize = BLEMessageV1Factory.getProtoHeaderSize(
                operationType, message.length, isPayloadEncrypted);

        // Make sure the payload can't fit into one chunk.
        mBleMessageStream.setMaxWriteSize(maxSize + headerSize);
        mBleMessageStream.writeMessage(message, operationType, isPayloadEncrypted);

        // Because there is no ACK, the write should be retried up to the limit.
        verify(mWriteCharacteristicMock, times(BleMessageStreamV1.BLE_MESSAGE_RETRY_LIMIT))
                .setValue(any(byte[].class));
        verify(mBlePeripheralManager, times(BleMessageStreamV1.BLE_MESSAGE_RETRY_LIMIT))
                .notifyCharacteristicChanged(mBluetoothDevice, mWriteCharacteristicMock, false);

        // But the callback should only be notified once.
        verify(mCallbackMock).onWriteMessageError();
    }

    @Test
    public void processClientMessage_noChunking_notifiesCallbackForCompleteMessage() {
        byte[] payload = "message".getBytes();

        // This client message is a complete message, meaning it is part 1 of 1.
        BLEMessage clientMessage = BLEMessageV1Factory.makeBLEMessage(
                payload, OperationType.CLIENT_MESSAGE, /* isPayloadEncrypted= */ true);

        mBleMessageStream.onCharacteristicWrite(
                mBluetoothDevice,
                mReadCharacteristicMock,
                clientMessage.toByteArray());

        // The callback should be notified with only the payload.
        verify(mCallbackMock).onMessageReceived(payload, READ_UUID);

        // And the callback should only be notified once.
        verify(mCallbackMock).onMessageReceived(any(byte[].class), any(UUID.class));
    }

    @Test
    public void processClientMessage_chunkingRequired_notifiesCallbackForCompleteMessage() {
        // The length here is arbitrary.
        int payloadLength = 1024;
        byte[] payload = makeMessage(payloadLength);

        // Ensure the max size is smaller than the payload length.
        int maxSize = 50;
        List<BLEMessage> clientMessages = BLEMessageV1Factory.makeBLEMessages(
                payload, OperationType.CLIENT_MESSAGE, maxSize, /* isPayloadEncrypted= */ true);

        for (BLEMessage message : clientMessages) {
            mBleMessageStream.onCharacteristicWrite(
                    mBluetoothDevice,
                    mReadCharacteristicMock,
                    message.toByteArray());
        }

        // The callback should be notified with only the payload.
        verify(mCallbackMock).onMessageReceived(payload, READ_UUID);

        // And the callback should only be notified once.
        verify(mCallbackMock).onMessageReceived(any(byte[].class), any(UUID.class));
    }

    @Test
    public void processClientMessage_chunkingRequired_sendsACKForEachMessagePart() {
        // The length here is arbitrary.
        int payloadLength = 1024;
        byte[] payload = makeMessage(payloadLength);

        // Ensure the max size is smaller than the payload length.
        int maxSize = 50;
        List<BLEMessage> clientMessages = BLEMessageV1Factory.makeBLEMessages(
                payload, OperationType.CLIENT_MESSAGE, maxSize, /* isPayloadEncrypted= */ true);

        for (BLEMessage message : clientMessages) {
            mBleMessageStream.onCharacteristicWrite(
                    mBluetoothDevice,
                    mReadCharacteristicMock,
                    message.toByteArray());
        }

        ArgumentCaptor<byte[]> messageCaptor = ArgumentCaptor.forClass(byte[].class);

        // An ACK should be sent for each message received, except the last one.
        verify(mWriteCharacteristicMock, times(clientMessages.size() - 1))
                .setValue(messageCaptor.capture());
        verify(mBlePeripheralManager, times(clientMessages.size() - 1))
                .notifyCharacteristicChanged(mBluetoothDevice, mWriteCharacteristicMock, false);

        List<byte[]> writtenValues = messageCaptor.getAllValues();
        byte[] ackMessageBytes = BLEMessageV1Factory.makeAcknowledgementMessage().toByteArray();

        // Now verify that each byte was an ACK message.
        for (byte[] writtenValue : writtenValues) {
            assertThat(writtenValue).isEqualTo(ackMessageBytes);
        }
    }

    private void mockHandlerToExecuteDelayedRunnablesImmediately() {
        when(mHandlerMock.postDelayed(any(Runnable.class), anyLong())).thenAnswer(invocation -> {
            invocation.<Runnable>getArgument(0).run();
            return null;
        });
    }

    /** Returns a random message of the specified length. */
    private byte[] makeMessage(int length) {
        byte[] message = new byte[length];
        new Random().nextBytes(message);

        return message;
    }
}
