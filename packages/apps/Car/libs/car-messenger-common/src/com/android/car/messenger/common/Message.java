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

package com.android.car.messenger.common;

import static com.android.car.apps.common.util.SafeLog.logw;
import static com.android.car.messenger.common.Utils.BMC_EXTRA_MESSAGE_HANDLE;
import static com.android.car.messenger.common.Utils.BMC_EXTRA_MESSAGE_READ_STATUS;
import static com.android.car.messenger.common.Utils.BMC_EXTRA_MESSAGE_TIMESTAMP;

import android.bluetooth.BluetoothDevice;
import android.content.Intent;
import android.util.Log;

import androidx.annotation.Nullable;

import com.android.car.messenger.NotificationMsgProto.NotificationMsg;
import com.android.car.messenger.NotificationMsgProto.NotificationMsg.MessagingStyleMessage;


/**
 * Represents a SMS, MMS, and {@link NotificationMsg}. This object is based
 * on {@link NotificationMsg}.
 */
public class Message {
    private static final String TAG = "CMC.Message";

    private final String mSenderName;
    private final String mDeviceId;
    private final String mMessageText;
    private final long mReceivedTime;
    private final boolean mIsReadOnPhone;
    private boolean mShouldExclude;
    private final String mHandle;
    private final MessageType mMessageType;
    private final SenderKey mSenderKey;


    /**
     * Note: MAP messages from iOS version 12 and earlier, as well as {@link MessagingStyleMessage},
     * don't provide these.
     */
    @Nullable
    final String mSenderContactUri;

    /**
     * Describes if the message was received through Bluetooth MAP or is a {@link NotificationMsg}.
     */
    public enum MessageType {
        BLUETOOTH_MAP_MESSAGE, NOTIFICATION_MESSAGE
    }

    /**
     * Creates a Message based on {@link MessagingStyleMessage}. Returns {@code null} if the {@link
     * MessagingStyleMessage} is missing required fields.
     *
     * @param deviceId of the phone that received this message.
     * @param updatedMessage containing the information to base this message object off of.
     * @param senderKey of the sender of the message. Not guaranteed to be unique for all senders
     *                  if this message is part of a group conversation.
     **/
    @Nullable
    public static Message parseFromMessage(String deviceId,
            MessagingStyleMessage updatedMessage, SenderKey senderKey) {

        if (!Utils.isValidMessagingStyleMessage(updatedMessage)) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                throw new IllegalArgumentException(
                        "MessagingStyleMessage is missing required fields");
            } else {
                logw(TAG, "MessagingStyleMessage is missing required fields");
                return null;
            }
        }

        return new Message(updatedMessage.getSender().getName(),
                deviceId,
                updatedMessage.getTextMessage(),
                updatedMessage.getTimestamp(),
                updatedMessage.getIsRead(),
                Utils.createMessageHandle(updatedMessage),
                MessageType.NOTIFICATION_MESSAGE,
                /* senderContactUri */ null,
                senderKey);
    }

    /**
     * Creates a Message based on BluetoothMapClient intent. Returns {@code null} if the
     * intent is missing required fields.
     **/
    public static Message parseFromIntent(Intent intent) {
        if (!Utils.isValidMapClientIntent(intent)) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                throw new IllegalArgumentException(
                        "BluetoothMapClient intent is missing required fields");
            } else {
                logw(TAG, "BluetoothMapClient intent is missing required fields");
                return null;
            }
        }
        BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
        String senderUri = Utils.getSenderUri(intent);

        return new Message(
                Utils.getSenderName(intent),
                device.getAddress(),
                intent.getStringExtra(android.content.Intent.EXTRA_TEXT),
                intent.getLongExtra(BMC_EXTRA_MESSAGE_TIMESTAMP,
                        System.currentTimeMillis()),
                intent.getBooleanExtra(BMC_EXTRA_MESSAGE_READ_STATUS,
                        false),
                intent.getStringExtra(BMC_EXTRA_MESSAGE_HANDLE),
                MessageType.BLUETOOTH_MAP_MESSAGE,
                senderUri,
                SenderKey.createSenderKey(intent)
        );
    }

    private Message(String senderName, String deviceId, String messageText, long receivedTime,
            boolean isReadOnPhone, String handle, MessageType messageType,
            @Nullable String senderContactUri, SenderKey senderKey) {
        boolean missingSenderName = (senderName == null);
        boolean missingDeviceId = (deviceId == null);
        boolean missingText = (messageText == null);
        boolean missingHandle = (handle == null);
        boolean missingType = (messageType == null);
        if (missingSenderName || missingDeviceId || missingText || missingHandle || missingType) {
            StringBuilder builder = new StringBuilder("Missing required fields:");
            if (missingSenderName) {
                builder.append(" senderName");
            }
            if (missingDeviceId) {
                builder.append(" deviceId");
            }
            if (missingText) {
                builder.append(" messageText");
            }
            if (missingHandle) {
                builder.append(" handle");
            }
            if (missingType) {
                builder.append(" type");
            }
            throw new IllegalArgumentException(builder.toString());
        }
        this.mSenderName = senderName;
        this.mDeviceId = deviceId;
        this.mMessageText = messageText;
        this.mReceivedTime = receivedTime;
        this.mIsReadOnPhone = isReadOnPhone;
        this.mShouldExclude = false;
        this.mHandle = handle;
        this.mMessageType = messageType;
        this.mSenderContactUri = senderContactUri;
        this.mSenderKey = senderKey;
    }

    /**
     * Returns the contact name as obtained from the device.
     * If contact is in the device's address-book, this is typically the contact name.
     * Otherwise it will be the phone number.
     */
    public String getSenderName() {
        return mSenderName;
    }

    /**
     * Returns the id of the device from which this message was received.
     */
    public String getDeviceId() {
        return mDeviceId;
    }

    /**
     * Returns the actual content of the message.
     */
    public String getMessageText() {
        return mMessageText;
    }

    /**
     * Returns the milliseconds since epoch at which this message notification was received on the
     * head-unit.
     */
    public long getReceivedTime() {
        return mReceivedTime;
    }

    /**
     * Whether message should be included in the notification. Messages that have been read aloud on
     * the car, or that have been dismissed by the user should be excluded from the notification if/
     * when the notification gets updated. Note: this state will not be propagated to the phone.
     */
    public void excludeFromNotification() {
        mShouldExclude = true;
    }

    /**
     * Returns {@code true} if message was read on the phone before it was received on the car.
     */
    public boolean isReadOnPhone() {
        return mIsReadOnPhone;
    }

    /**
     * Returns {@code true} if message should not be included in the notification. Messages that
     * have been read aloud on the car, or that have been dismissed by the user should be excluded
     * from the notification if/when the notification gets updated.
     */
    public boolean shouldExcludeFromNotification() {
        return mShouldExclude;
    }

    /**
     * Returns a unique handle/key for this message. This is used as this Message's
     * {@link MessageKey#getSubKey()} Note: this handle might only be unique for the lifetime of a
     * device connection session.
     */
    public String getHandle() {
        return mHandle;
    }

    /**
     * If the message came from BluetoothMapClient, this retrieves a key that is unique
     * for each contact per device.
     * If the message came from {@link NotificationMsg}, this retrieves a key that is only
     * guaranteed to be unique per sender in a 1-1 conversation. If this message is part of a
     * group conversation, the senderKey will not be unique if more than one participant in the
     * conversation share the same name.
     */
    public SenderKey getSenderKey() {
        return mSenderKey;
    }

    /** Returns whether the message is a SMS/MMS or a {@link NotificationMsg} **/
    public MessageType getMessageType() {
        return mMessageType;
    }

    /**
     * Returns the sender's phone number available as a URI string.
     * Note: MAP messages from iOS version 12 and earlier, as well as {@link MessagingStyleMessage},
     * don't provide these.
     */
    @Nullable
    public String getSenderContactUri() {
        return mSenderContactUri;
    }

    @Override
    public String toString() {
        return "Message{"
                + " mSenderName='" + mSenderName + '\''
                + ", mMessageText='" + mMessageText + '\''
                + ", mSenderContactUri='" + mSenderContactUri + '\''
                + ", mReceiveTime=" + mReceivedTime + '\''
                + ", mIsReadOnPhone= " + mIsReadOnPhone + '\''
                + ", mShouldExclude= " + mShouldExclude + '\''
                + ", mHandle='" + mHandle + '\''
                + ", mSenderKey='" + mSenderKey.toString()
                + "}";
    }
}
