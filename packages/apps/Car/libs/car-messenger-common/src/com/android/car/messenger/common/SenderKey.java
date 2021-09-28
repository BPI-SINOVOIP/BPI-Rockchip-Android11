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

import android.bluetooth.BluetoothDevice;
import android.content.Intent;

import com.android.car.messenger.NotificationMsgProto.NotificationMsg;

/**
 * {@link CompositeKey} subclass used to give each contact on all the connected devices a
 * unique Key.
 */
public class SenderKey extends CompositeKey {
    /** Creates a senderkey for SMS, MMS, and {@link NotificationMsg}. **/
    private SenderKey(String deviceId, String senderName, String keyMetadata) {
        super(deviceId, senderName + "/" + keyMetadata);
    }

    /**
     * Returns the SenderKey for the BluetoothMapClient intent. This should be unique
     * for each contact per device.
     */
    public static SenderKey createSenderKey(Intent intent) {
        BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
        return new SenderKey(device.getAddress(), Utils.getSenderName(intent),
                Utils.getSenderUri(intent));
    }

    /**
     * Returns the SenderKey based on a {@link NotificationMsg} DAO. This key is only
     * guaranteed to be unique for a 1-1 conversation. If the ConversationKey is for a
     * group conversation, the senderKey will not be unique if more than one participant in the
     * conversation share the same name.
     */
    public static SenderKey createSenderKey(ConversationKey convoKey,
            NotificationMsg.Person person) {
        return new SenderKey(convoKey.getDeviceId(), person.getName(), convoKey.getSubKey());
    }
}
