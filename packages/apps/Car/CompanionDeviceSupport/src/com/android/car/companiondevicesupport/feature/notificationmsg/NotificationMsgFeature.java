/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.car.companiondevicesupport.feature.notificationmsg;

import static com.android.car.connecteddevice.util.SafeLog.logd;
import static com.android.car.connecteddevice.util.SafeLog.loge;
import static com.android.car.connecteddevice.util.SafeLog.logw;

import android.annotation.NonNull;
import android.content.Context;
import android.os.ParcelUuid;
import android.os.RemoteException;

import com.android.car.companiondevicesupport.R;
import com.android.car.companiondevicesupport.api.external.CompanionDevice;
import com.android.car.companiondevicesupport.feature.RemoteFeature;
import com.android.car.messenger.NotificationMsgProto.NotificationMsg;
import com.android.car.protobuf.InvalidProtocolBufferException;

/**
 * An implementation of {@link RemoteFeature} that registers the NotificationMsg feature and handles
 * the import and deletion of text messages sent from connected {@link CompanionDevice}s.
 *
 * <p>The communication data this feature is handling will be cleared every time an error or
 * disconnect happens, or the feature is stopped.
 */
public class NotificationMsgFeature extends RemoteFeature {
    private final static String TAG = "NotificationMsgFeature";

    private final NotificationMsgDelegate mNotificationMsgDelegate;
    private CompanionDevice mSecureDeviceForActiveUser;

    NotificationMsgFeature(@NonNull Context context,
            @NonNull NotificationMsgDelegate notificationMsgDelegate) {
        super(context,
                ParcelUuid.fromString(context.getString(R.string.notification_msg_feature_id)));
        mNotificationMsgDelegate = notificationMsgDelegate;
    }

    @Override
    public void start() {
        // For safety in case something went wrong and the feature couldn't terminate correctly we
        // clear all notifications and local data on start of the feature.
        mNotificationMsgDelegate.cleanupMessagesAndNotifications(key -> true);
        super.start();
    }

    @Override
    public void stop() {
        // Erase all the notifications and local data, so that no user data stays on the device
        // after the feature is stopped.
        mNotificationMsgDelegate.onDestroy();
        super.stop();
    }

    @Override
    protected void onMessageReceived(CompanionDevice device, byte[] message) {
        if (mSecureDeviceForActiveUser == null && device.hasSecureChannel()
                && device.isActiveUser()) {
            logw(TAG, "stored secure device is null, but message was received on a"
                    + " secure device!" + device);
            mSecureDeviceForActiveUser = device;
        }
        if (!isSecureDeviceForActiveUser(device.getDeviceId())) {
            logd(TAG, device + ": skipped message from unsecure device");
            return;
        }
        logd(TAG, device + ": received message over secure channel");
        try {
            NotificationMsg.PhoneToCarMessage
                    phoneToCarMessage = NotificationMsg.PhoneToCarMessage.parseFrom(message);
            mNotificationMsgDelegate.onMessageReceived(device, phoneToCarMessage);
        } catch (InvalidProtocolBufferException e) {
            loge(TAG, device + ": error parsing notification msg protobuf", e);
        }
    }

    @Override
    protected void onSecureChannelEstablished(CompanionDevice device) {
        logd(TAG, "received secure device: " + device);
        mSecureDeviceForActiveUser = device;
    }

    @Override
    protected void onDeviceError(CompanionDevice device, int error) {
        if (!isSecureDeviceForActiveUser(device.getDeviceId())) return;
        loge(TAG, device + ": received device error " + error, null);
    }

    @Override
    protected void onDeviceDisconnected(CompanionDevice device) {
        if (!isSecureDeviceForActiveUser(device.getDeviceId())) return;
        logw(TAG, device + ": disconnected");
        mNotificationMsgDelegate.onDeviceDisconnected(device.getDeviceId());
        mSecureDeviceForActiveUser = null;
    }

    protected void sendData(@NonNull String deviceId, @NonNull byte[] message) {
        if (mSecureDeviceForActiveUser == null || !isSecureDeviceForActiveUser(deviceId)) {
            logw(TAG, "Could not send message to device: " + deviceId);
            return;
        }

        sendMessageSecurely(deviceId, message);
    }

    @Override
    protected void onMessageFailedToSend(String deviceId, byte[] message, boolean isTransient) {
        // TODO (b/144924164): Notify Delegate action request failed.
    }

    private boolean isSecureDeviceForActiveUser(String deviceId) {
        return (mSecureDeviceForActiveUser != null)
                && mSecureDeviceForActiveUser.getDeviceId().equals(deviceId);
    }
}
