/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.car.notification;

import android.annotation.Nullable;
import android.app.ActivityManager;
import android.app.NotificationManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Binder;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.RemoteException;
import android.os.UserHandle;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;
import android.util.Log;

import com.android.car.notification.headsup.CarHeadsUpNotificationAppContainer;

import java.util.HashMap;
import java.util.Map;
import java.util.Objects;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * NotificationListenerService that fetches all notifications from system.
 */
public class CarNotificationListener extends NotificationListenerService implements
        CarHeadsUpNotificationManager.OnHeadsUpNotificationStateChange {
    private static final String TAG = "CarNotificationListener";
    static final String ACTION_LOCAL_BINDING = "local_binding";
    static final int NOTIFY_NOTIFICATION_POSTED = 1;
    static final int NOTIFY_NOTIFICATION_REMOVED = 2;
    /** Temporary {@link Ranking} object that serves as a reused value holder */
    final private Ranking mTemporaryRanking = new Ranking();

    private Handler mHandler;
    private RankingMap mRankingMap;
    private CarHeadsUpNotificationManager mHeadsUpManager;
    private NotificationDataManager mNotificationDataManager;

    /**
     * Map that contains all the active notifications that are not currently HUN. These
     * notifications may or may not be visible to the user if they get filtered out. The only time
     * these will be removed from the map is when the {@llink NotificationListenerService} calls the
     * onNotificationRemoved method. New notifications will be added to this map if the notification
     * is posted as a non-HUN or when a HUN's state is changed to non-HUN.
     */
    private Map<String, AlertEntry> mActiveNotifications = new HashMap<>();

    /**
     * Call this if to register this service as a system service and connect to HUN. This is useful
     * if the notification service is being used as a lib instead of a standalone app. The
     * standalone app version has a manifest entry that will have the same effect.
     *
     * @param context Context required for registering the service.
     * @param carUxRestrictionManagerWrapper will have the heads up manager registered with it.
     * @param carHeadsUpNotificationManager HUN controller.
     * @param notificationDataManager used for keeping track of additional notification states.
     */
    public void registerAsSystemService(Context context,
            CarUxRestrictionManagerWrapper carUxRestrictionManagerWrapper,
            CarHeadsUpNotificationManager carHeadsUpNotificationManager,
            NotificationDataManager notificationDataManager) {
        try {
            mNotificationDataManager = notificationDataManager;
            registerAsSystemService(context,
                    new ComponentName(context.getPackageName(), getClass().getCanonicalName()),
                    ActivityManager.getCurrentUser());
            mHeadsUpManager = carHeadsUpNotificationManager;
            mHeadsUpManager.registerHeadsUpNotificationStateChangeListener(this);
            carUxRestrictionManagerWrapper.setCarHeadsUpNotificationManager(
                    carHeadsUpNotificationManager);
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to register notification listener", e);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mNotificationDataManager = new NotificationDataManager();
        NotificationApplication app = (NotificationApplication) getApplication();

        app.getClickHandlerFactory().setNotificationDataManager(mNotificationDataManager);
        mHeadsUpManager = new CarHeadsUpNotificationManager(/* context= */ this,
                app.getClickHandlerFactory(), mNotificationDataManager, new CarHeadsUpNotificationAppContainer(this));
        mHeadsUpManager.registerHeadsUpNotificationStateChangeListener(this);
        app.getCarUxRestrictionWrapper().setCarHeadsUpNotificationManager(mHeadsUpManager);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return ACTION_LOCAL_BINDING.equals(intent.getAction())
                ? new LocalBinder() : super.onBind(intent);
    }

    @Override
    public void onNotificationPosted(StatusBarNotification sbn, RankingMap rankingMap) {
        if (sbn == null) {
            Log.e(TAG, "onNotificationPosted: StatusBarNotification is null");
            return;
        }

        Log.d(TAG, "onNotificationPosted: " + sbn);
        if (!isNotificationForCurrentUser(sbn)) {
            return;
        }
        AlertEntry alertEntry = new AlertEntry(sbn);
        onNotificationRankingUpdate(rankingMap);
        notifyNotificationPosted(alertEntry);
    }

    @Override
    public void onNotificationRemoved(StatusBarNotification sbn) {
        if (sbn == null) {
            Log.e(TAG, "onNotificationRemoved: StatusBarNotification is null");
            return;
        }

        Log.d(TAG, "onNotificationRemoved: " + sbn);
        AlertEntry alertEntry = mActiveNotifications.get(sbn.getKey());

        if (alertEntry != null) {
            mActiveNotifications.remove(alertEntry.getKey());
        } else {
            // HUN notifications are not tracked in mActiveNotifications but still need to be
            // removed
            alertEntry = new AlertEntry(sbn);
        }

        removeNotification(alertEntry);
    }

    @Override
    public void onNotificationRankingUpdate(RankingMap rankingMap) {
        mRankingMap = rankingMap;
        for (AlertEntry alertEntry : mActiveNotifications.values()) {
            if (!mRankingMap.getRanking(alertEntry.getKey(), mTemporaryRanking)) {
                continue;
            }
            String oldOverrideGroupKey =
                    alertEntry.getStatusBarNotification().getOverrideGroupKey();
            String newOverrideGroupKey = getOverrideGroupKey(alertEntry.getKey());
            if (!Objects.equals(oldOverrideGroupKey, newOverrideGroupKey)) {
                alertEntry.getStatusBarNotification().setOverrideGroupKey(newOverrideGroupKey);
            }
        }
    }

    /**
     * Get the override group key of a {@link AlertEntry} given its key.
     */
    @Nullable
    private String getOverrideGroupKey(String key) {
        if (mRankingMap != null) {
            mRankingMap.getRanking(key, mTemporaryRanking);
            return mTemporaryRanking.getOverrideGroupKey();
        }
        return null;
    }

    /**
     * Get all active notifications that are not heads-up notifications.
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
    public RankingMap getCurrentRanking() {
        return mRankingMap;
    }

    @Override
    public void onListenerConnected() {
        mActiveNotifications = Stream.of(getActiveNotifications()).collect(
                Collectors.toMap(StatusBarNotification::getKey, sbn -> new AlertEntry(sbn)));
        mRankingMap = super.getCurrentRanking();
    }

    @Override
    public void onListenerDisconnected() {
    }

    public void setHandler(Handler handler) {
        mHandler = handler;
    }

    private void notifyNotificationPosted(AlertEntry alertEntry) {
        if (shouldTrackUnseen(alertEntry)) {
            mNotificationDataManager.addNewMessageNotification(alertEntry);
        } else {
            mNotificationDataManager.untrackUnseenNotification(alertEntry);
        }

        boolean isShowingHeadsUp = mHeadsUpManager.maybeShowHeadsUp(alertEntry, getCurrentRanking(),
                mActiveNotifications);

        if (!isShowingHeadsUp) {
            postNewNotification(alertEntry);
        }
    }

    private boolean isNotificationForCurrentUser(StatusBarNotification sbn) {
        // Notifications should only be shown for the current user and the the notifications from
        // the system when CarNotification is running as SystemUI component.
        return (sbn.getUser().getIdentifier() == ActivityManager.getCurrentUser()
                || sbn.getUser().getIdentifier() == UserHandle.USER_ALL);
    }


    @Override
    public void onStateChange(AlertEntry alertEntry, boolean isHeadsUp) {
        // No more a HUN
        if (!isHeadsUp) {
            postNewNotification(alertEntry);
        }
    }

    class LocalBinder extends Binder {
        public CarNotificationListener getService() {
            return CarNotificationListener.this;
        }
    }

    private void postNewNotification(AlertEntry alertEntry) {
        mActiveNotifications.put(alertEntry.getKey(), alertEntry);
        sendNotificationEventToHandler(alertEntry, NOTIFY_NOTIFICATION_POSTED);
    }

    private void removeNotification(AlertEntry alertEntry) {
        mHeadsUpManager.maybeRemoveHeadsUp(alertEntry);
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

    // Don't show unseen markers for <= LOW importance notifications to be consistent
    // with how these notifications are handled on phones
    boolean shouldTrackUnseen(AlertEntry alertEntry) {
        Ranking ranking = new NotificationListenerService.Ranking();
        mRankingMap.getRanking(alertEntry.getKey(), ranking);
        return ranking.getImportance() > NotificationManager.IMPORTANCE_LOW;
    }
}
