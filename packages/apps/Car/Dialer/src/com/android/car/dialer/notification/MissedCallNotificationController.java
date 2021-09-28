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

package com.android.car.dialer.notification;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.provider.CallLog;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.Observer;

import com.android.car.arch.common.LiveDataFunctions;
import com.android.car.dialer.R;
import com.android.car.dialer.bluetooth.UiBluetoothMonitor;
import com.android.car.dialer.livedata.UnreadMissedCallLiveData;
import com.android.car.dialer.log.L;
import com.android.car.dialer.ui.TelecomActivity;
import com.android.car.telephony.common.PhoneCallLog;
import com.android.car.telephony.common.TelecomUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;

/** Controller that manages the missed call notifications. */
public final class MissedCallNotificationController {
    private static final String TAG = "CD.MissedCallNotification";
    private static final String CHANNEL_ID = "com.android.car.dialer.missedcall";
    // A random number that is used for notification id.
    private static final int NOTIFICATION_ID = 20190520;

    private static MissedCallNotificationController sMissedCallNotificationController;

    /**
     * Initialized a globally accessible {@link MissedCallNotificationController} which can be
     * retrieved by {@link #get}. If this function is called a second time before calling {@link
     * #tearDown()}, an {@link IllegalStateException} will be thrown.
     *
     * @param applicationContext Application context.
     */
    public static void init(Context applicationContext) {
        if (sMissedCallNotificationController == null) {
            sMissedCallNotificationController = new MissedCallNotificationController(
                    applicationContext);
        } else {
            throw new IllegalStateException(
                    "MissedCallNotificationController has been initialized.");
        }
    }

    /**
     * Gets the global {@link MissedCallNotificationController} instance. Make sure {@link
     * #init(Context)} is called before calling this method.
     */
    public static MissedCallNotificationController get() {
        if (sMissedCallNotificationController == null) {
            throw new IllegalStateException(
                    "Call MissedCallNotificationController.init(Context) before calling this "
                            + "function");
        }
        return sMissedCallNotificationController;
    }

    /** Tear down the global missed call notification controller. */
    public void tearDown() {
        mUnreadMissedCallLiveData.removeObserver(mUnreadMissedCallObserver);
        sMissedCallNotificationController = null;
    }

    private final Context mContext;
    private final NotificationManager mNotificationManager;
    private final LiveData<List<PhoneCallLog>> mUnreadMissedCallLiveData;
    private final Observer<List<PhoneCallLog>> mUnreadMissedCallObserver;
    private final List<PhoneCallLog> mCurrentPhoneCallLogList;
    private final Map<String, CompletableFuture<Void>> mUpdateFutures = new HashMap<>();

    private MissedCallNotificationController(Context context) {
        mContext = context;
        mNotificationManager =
                (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);
        CharSequence name = mContext.getString(R.string.missed_call_notification_channel_name);
        NotificationChannel notificationChannel = new NotificationChannel(CHANNEL_ID, name,
                NotificationManager.IMPORTANCE_DEFAULT);
        mNotificationManager.createNotificationChannel(notificationChannel);

        mCurrentPhoneCallLogList = new ArrayList<>();
        mUnreadMissedCallLiveData = LiveDataFunctions.switchMapNonNull(
                UiBluetoothMonitor.get().getFirstHfpConnectedDevice(),
                device-> UnreadMissedCallLiveData.newInstance(context, device.getAddress()));
        mUnreadMissedCallObserver = this::updateNotifications;
        mUnreadMissedCallLiveData.observeForever(mUnreadMissedCallObserver);
    }

    /**
     * The phone call log list might be null when switching users if permission gets denied and
     * throws exception.
     */
    private void updateNotifications(@Nullable List<PhoneCallLog> phoneCallLogs) {
        List<PhoneCallLog> updatedPhoneCallLogs =
                phoneCallLogs == null ? Collections.emptyList() : phoneCallLogs;
        for (PhoneCallLog phoneCallLog : updatedPhoneCallLogs) {
            showMissedCallNotification(phoneCallLog);
            mCurrentPhoneCallLogList.remove(phoneCallLog);
        }

        for (PhoneCallLog phoneCallLog : mCurrentPhoneCallLogList) {
            cancelMissedCallNotification(phoneCallLog);
        }
        mCurrentPhoneCallLogList.clear();
        mCurrentPhoneCallLogList.addAll(updatedPhoneCallLogs);
    }

    private void showMissedCallNotification(PhoneCallLog callLog) {
        L.d(TAG, "show missed call notification %s", callLog);
        String phoneNumber = callLog.getPhoneNumberString();
        String tag = getTag(callLog);
        cancelLoadingRunnable(tag);
        CompletableFuture<Void> updateFuture = NotificationUtils.getDisplayNameAndRoundedAvatar(
                mContext, phoneNumber)
                .thenAcceptAsync((pair) -> {
                    int callLogSize = callLog.getAllCallRecords().size();
                    Notification.Builder builder = new Notification.Builder(mContext, CHANNEL_ID)
                            .setSmallIcon(R.drawable.ic_phone)
                            .setColor(mContext.getColor(R.color.notification_app_icon_color))
                            .setLargeIcon(pair.second)
                            .setContentTitle(mContext.getResources().getQuantityString(
                                    R.plurals.notification_missed_call, callLogSize, callLogSize))
                            .setContentText(TelecomUtils.getBidiWrappedNumber(pair.first))
                            .setContentIntent(getContentPendingIntent())
                            .setDeleteIntent(getDeleteIntent(callLog))
                            .setOnlyAlertOnce(true)
                            .setShowWhen(true)
                            .setWhen(callLog.getLastCallEndTimestamp())
                            .setAutoCancel(false);

                    if (!TextUtils.isEmpty(phoneNumber)) {
                        builder.addAction(getAction(phoneNumber, tag, R.string.call_back,
                                NotificationService.ACTION_CALL_BACK_MISSED));
                        // TODO: add action button to send message
                    }

                    mNotificationManager.notify(
                            tag,
                            NOTIFICATION_ID,
                            builder.build());
                }, mContext.getMainExecutor());
        mUpdateFutures.put(tag, updateFuture);
    }

    private void cancelMissedCallNotification(PhoneCallLog phoneCallLog) {
        L.d(TAG, "cancel missed call notification %s", phoneCallLog);
        String tag = getTag(phoneCallLog);
        cancelMissedCallNotification(tag);
    }

    /**
     * Explicitly cancels the notification that in some circumstances the database update operation
     * has a delay to notify the cursor to reload.
     */
    void cancelMissedCallNotification(String tag) {
        if (TextUtils.isEmpty(tag)) {
            L.w(TAG, "Invalid notification tag, ignore canceling request.");
            return;
        }
        cancelLoadingRunnable(tag);
        mNotificationManager.cancel(tag, NOTIFICATION_ID);
    }

    private void cancelLoadingRunnable(String tag) {
        CompletableFuture<Void> completableFuture = mUpdateFutures.get(tag);
        if (completableFuture != null) {
            completableFuture.cancel(true);
        }
        mUpdateFutures.remove(tag);
    }

    private PendingIntent getContentPendingIntent() {
        Intent intent = new Intent(mContext, TelecomActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setAction(Intent.ACTION_VIEW);
        intent.setType(CallLog.Calls.CONTENT_TYPE);
        PendingIntent pendingIntent = PendingIntent.getActivity(mContext, 0, intent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        return pendingIntent;
    }

    private PendingIntent getDeleteIntent(PhoneCallLog phoneCallLog) {
        Intent intent = new Intent(NotificationService.ACTION_READ_MISSED, null, mContext,
                NotificationService.class);
        String phoneNumberString = phoneCallLog.getPhoneNumberString();
        if (TextUtils.isEmpty(phoneNumberString)) {
            // For unknown call, pass the call log id to mark as read
            intent.putExtra(NotificationService.EXTRA_CALL_LOG_ID, phoneCallLog.getPhoneLogId());
        } else {
            intent.putExtra(NotificationService.EXTRA_PHONE_NUMBER, phoneNumberString);
        }
        intent.putExtra(NotificationService.EXTRA_NOTIFICATION_TAG, getTag(phoneCallLog));
        PendingIntent pendingIntent = PendingIntent.getService(
                mContext,
                // Unique id for PendingIntents with different extras
                /* requestCode= */(int) System.currentTimeMillis(),
                intent,
                PendingIntent.FLAG_IMMUTABLE);
        return pendingIntent;
    }

    private Notification.Action getAction(String phoneNumberString, String tag,
            @StringRes int actionText, String intentAction) {
        CharSequence text = mContext.getString(actionText);
        PendingIntent intent = getIntent(intentAction, phoneNumberString, tag);
        return new Notification.Action.Builder(null, text, intent).build();
    }

    private PendingIntent getIntent(String action, String phoneNumberString, String tag) {
        Intent intent = new Intent(action, null, mContext, NotificationService.class);
        intent.putExtra(NotificationService.EXTRA_PHONE_NUMBER, phoneNumberString);
        intent.putExtra(NotificationService.EXTRA_NOTIFICATION_TAG, tag);
        return PendingIntent.getService(
                mContext,
                // Unique id for PendingIntents with different extras
                /* requestCode= */(int) System.currentTimeMillis(),
                intent,
                PendingIntent.FLAG_IMMUTABLE);
    }

    private String getTag(@NonNull PhoneCallLog phoneCallLog) {
        return String.valueOf(phoneCallLog.hashCode());
    }
}
