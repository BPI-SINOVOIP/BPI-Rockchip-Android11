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

package com.android.car.secondaryhome.launcher;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Binder;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Process;
import android.os.RemoteException;
import android.os.UserHandle;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;
import android.util.Log;

import com.android.car.notification.AlertEntry;

import java.util.HashMap;
import java.util.Map;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * NotificationListenerService that fetches all notifications from system.
 * Copied from {@link CarNotificationListener} since {@link CarNotificationListener}
 * cannot be used outside that package.
*/
public class NotificationListener extends NotificationListenerService {
    private static final String TAG = "SecHome.NotificationListener";
    public static final String ACTION_LOCAL_BINDING = "local_binding";
    public static final int NOTIFY_NOTIFICATION_POSTED = 1;
    public static final int NOTIFY_NOTIFICATION_REMOVED = 2;

    private final int mUserId = Process.myUserHandle().getIdentifier();

    private Handler mHandler;

    /**
     * Map that contains all the active notifications. They will be removed from the map
     * when the {@link NotificationListenerService} calls the onNotificationRemoved method.
     */
    private Map<String, AlertEntry> mActiveNotifications = new HashMap<>();

    /**
     * Call this to register this service as a system service.
     *
     * @param context Context required for registering the service.
     */

    public void registerAsSystemService(Context context) {
        try {
            Log.d(TAG, "register as system service for: " + mUserId);
            registerAsSystemService(context,
                    new ComponentName(context.getPackageName(),
                    getClass().getCanonicalName()),
                    Process.myUid());
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to register notification listener", e);
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        Log.d("TAG", "onBind");
        return ACTION_LOCAL_BINDING.equals(intent.getAction())
                ? new LocalBinder() : super.onBind(intent);
    }

    @Override
    public void onNotificationPosted(StatusBarNotification sbn, RankingMap rankingMap) {
        Log.d(TAG, "onNotificationPosted: " + sbn);
        if (!isNotificationForCurrentUser(sbn)) {
            return;
        }
        AlertEntry alertEntry = new AlertEntry(sbn);
        notifyNotificationPosted(alertEntry);
    }

    @Override
    public void onNotificationRemoved(StatusBarNotification sbn) {
        Log.d(TAG, "onNotificationRemoved: " + sbn);
        AlertEntry alertEntry = mActiveNotifications.get(sbn.getKey());
        if (alertEntry != null) {
            removeNotification(alertEntry);
        }
    }

    /**
     * Get all active notifications
     *
     * @return a map of all active notifications with key being the notification key.
     */
    Map<String, AlertEntry> getNotifications() {
        return mActiveNotifications.entrySet().stream()
                .filter(x -> (isNotificationForCurrentUser(
                        x.getValue().getStatusBarNotification())))
                .collect(Collectors.toMap(Map.Entry::getKey, Map.Entry::getValue));
    }

    @Override
    public void onListenerConnected() {
        mActiveNotifications = Stream.of(getActiveNotifications()).collect(
                Collectors.toMap(StatusBarNotification::getKey, sbn -> new AlertEntry(sbn)));
    }

    @Override
    public void onListenerDisconnected() {
    }

    public void setHandler(Handler handler) {
        mHandler = handler;
    }

    private void notifyNotificationPosted(AlertEntry alertEntry) {
        postNewNotification(alertEntry);
    }

    private boolean isNotificationForCurrentUser(StatusBarNotification sbn) {
        return (sbn.getUser().getIdentifier() == mUserId
                || sbn.getUser().getIdentifier() == UserHandle.USER_ALL);
    }

    class LocalBinder extends Binder {
        public NotificationListener getService() {
            return NotificationListener.this;
        }
    }

    private void postNewNotification(AlertEntry alertEntry) {
        mActiveNotifications.put(alertEntry.getKey(), alertEntry);
        sendNotificationEventToHandler(alertEntry, NOTIFY_NOTIFICATION_POSTED);
    }

    private void removeNotification(AlertEntry alertEntry) {
        mActiveNotifications.remove(alertEntry.getKey());
        sendNotificationEventToHandler(alertEntry, NOTIFY_NOTIFICATION_REMOVED);
    }

    private void sendNotificationEventToHandler(AlertEntry alertEntry, int eventType) {
        if (mHandler == null) {
            return;
        }
        Message msg = Message.obtain(mHandler);
        msg.what = eventType;
        msg.obj = alertEntry;
        mHandler.sendMessage(msg);
    }
}
