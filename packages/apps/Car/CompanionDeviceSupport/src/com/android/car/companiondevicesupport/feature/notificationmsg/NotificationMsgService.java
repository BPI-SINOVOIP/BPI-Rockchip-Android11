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

package com.android.car.companiondevicesupport.feature.notificationmsg;

import static com.android.car.connecteddevice.util.SafeLog.loge;
import static com.android.car.connecteddevice.util.SafeLog.logw;
import static com.android.car.messenger.common.BaseNotificationDelegate.ACTION_DISMISS_NOTIFICATION;
import static com.android.car.messenger.common.BaseNotificationDelegate.ACTION_MARK_AS_READ;
import static com.android.car.messenger.common.BaseNotificationDelegate.ACTION_REPLY;
import static com.android.car.messenger.common.BaseNotificationDelegate.EXTRA_CONVERSATION_KEY;
import static com.android.car.messenger.common.BaseNotificationDelegate.EXTRA_REMOTE_INPUT_KEY;

import android.annotation.Nullable;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;

import androidx.core.app.NotificationCompat;
import androidx.core.app.RemoteInput;

import com.android.car.companiondevicesupport.R;
import com.android.car.companiondevicesupport.api.external.CompanionDevice;
import com.android.car.messenger.NotificationMsgProto.NotificationMsg;
import com.android.car.messenger.common.ConversationKey;

/**
 * Service responsible for handling {@link NotificationMsg} messaging events from the active user's
 * securely paired {@link CompanionDevice}s.
 */
public class NotificationMsgService extends Service {
    private final static String TAG = "NotificationMsgService";

    /* NOTIFICATIONS */
    static final String NOTIFICATION_MSG_CHANNEL_ID = "NOTIFICATION_MSG_CHANNEL_ID";
    private static final String APP_RUNNING_CHANNEL_ID = "APP_RUNNING_CHANNEL_ID";
    private static final int SERVICE_STARTED_NOTIFICATION_ID = Integer.MAX_VALUE;

    private NotificationMsgDelegate mNotificationMsgDelegate;
    private NotificationMsgFeature mNotificationMsgFeature;
    private final IBinder binder = new LocalBinder();
    private NotificationManager mNotificationManager;

    public class LocalBinder extends Binder {
        NotificationMsgService getService() {
            return NotificationMsgService.this;
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return binder;
    }

    @Override
    public void onCreate() {
        super.onCreate();

        mNotificationManager = getSystemService(NotificationManager.class);
        mNotificationMsgDelegate = new NotificationMsgDelegate(this);
        mNotificationMsgFeature = new NotificationMsgFeature(this, mNotificationMsgDelegate);
        mNotificationMsgFeature.start();
        sendServiceRunningNotification();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mNotificationMsgFeature.stop();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null || intent.getAction() == null) return START_STICKY;

        String action = intent.getAction();

        switch (action) {
            case ACTION_REPLY:
                handleReplyIntent(intent);
                break;
            case ACTION_DISMISS_NOTIFICATION:
                handleDismissNotificationIntent(intent);
                break;
            case ACTION_MARK_AS_READ:
                handleMarkAsReadIntent(intent);
                break;
            default:
                logw(TAG, "Unsupported action: " + action);
        }

        return START_STICKY;
    }

    /**
     * Posts a service running (silent/hidden) notification, so we don't throw ANR after service
     * is started.
     */
    private void sendServiceRunningNotification() {
        if (mNotificationManager == null) {
            loge(TAG, "Failed to get NotificationManager instance");
            return;
        }

        // Create notification channel for app running notification
        NotificationChannel appRunningNotificationChannel =
                new NotificationChannel(APP_RUNNING_CHANNEL_ID,
                        getString(R.string.app_running_msg_channel_name),
                        NotificationManager.IMPORTANCE_MIN);
        mNotificationManager.createNotificationChannel(appRunningNotificationChannel);

        final Notification notification =
                new NotificationCompat.Builder(this, APP_RUNNING_CHANNEL_ID)
                        .setSmallIcon(R.drawable.ic_message)
                        .setContentTitle(getString(R.string.app_running_msg_notification_title))
                        .setContentText(getString(R.string.app_running_msg_notification_content))
                        .build();
        startForeground(SERVICE_STARTED_NOTIFICATION_ID, notification);
    }


    private void handleDismissNotificationIntent(Intent intent) {
        ConversationKey key = getConversationKey(intent);
        if (key == null) {
            logw(TAG, "Dropping dismiss intent. Received null conversation key.");
            return;
        }
        mNotificationMsgFeature.sendData(key.getDeviceId(),
                mNotificationMsgDelegate.dismiss(key).toByteArray());
    }

    private void handleMarkAsReadIntent(Intent intent) {
        ConversationKey key = getConversationKey(intent);
        if (key == null) {
            logw(TAG, "Dropping mark as read intent. Received null conversation key.");
            return;
        }
        mNotificationMsgFeature.sendData(key.getDeviceId(),
                mNotificationMsgDelegate.markAsRead(key).toByteArray());
    }

    private void handleReplyIntent(Intent intent) {
        ConversationKey key = getConversationKey(intent);
        Bundle bundle = RemoteInput.getResultsFromIntent(intent);
        if (bundle == null || key == null) {
            logw(TAG, "Dropping voice reply intent. Received null arguments.");
            return;
        }
        CharSequence message = bundle.getCharSequence(EXTRA_REMOTE_INPUT_KEY);
        mNotificationMsgFeature.sendData(key.getDeviceId(),
                mNotificationMsgDelegate.reply(key, message.toString()).toByteArray());
    }

    @Nullable
    private ConversationKey getConversationKey(Intent intent) {
        return intent.getParcelableExtra(EXTRA_CONVERSATION_KEY);
    }
}
