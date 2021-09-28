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
import android.graphics.drawable.Icon;
import android.telecom.Call;
import android.text.TextUtils;

import androidx.annotation.StringRes;

import com.android.car.dialer.R;
import com.android.car.dialer.log.L;
import com.android.car.telephony.common.CallDetail;
import com.android.car.telephony.common.TelecomUtils;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.CompletableFuture;

/** Controller that manages the heads up notification for incoming calls. */
public final class InCallNotificationController {
    private static final String TAG = "CD.InCallNotificationController";
    private static final String CHANNEL_ID = "com.android.car.dialer.incoming";
    // A random number that is used for notification id.
    private static final int NOTIFICATION_ID = 20181105;

    private static InCallNotificationController sInCallNotificationController;

    private boolean mShowFullscreenIncallUi;

    /**
     * Initialized a globally accessible {@link InCallNotificationController} which can be retrieved
     * by {@link #get}. If this function is called a second time before calling {@link #tearDown()},
     * an {@link IllegalStateException} will be thrown.
     *
     * @param applicationContext Application context.
     */
    public static void init(Context applicationContext) {
        if (sInCallNotificationController == null) {
            sInCallNotificationController = new InCallNotificationController(applicationContext);
        } else {
            throw new IllegalStateException("InCallNotificationController has been initialized.");
        }
    }

    /**
     * Gets the global {@link InCallNotificationController} instance. Make sure
     * {@link #init(Context)} is called before calling this method.
     */
    public static InCallNotificationController get() {
        if (sInCallNotificationController == null) {
            throw new IllegalStateException(
                    "Call InCallNotificationController.init(Context) before calling this function");
        }
        return sInCallNotificationController;
    }

    public static void tearDown() {
        sInCallNotificationController = null;
    }

    private final Context mContext;
    private final NotificationManager mNotificationManager;
    private final Notification.Builder mNotificationBuilder;
    private final Set<String> mActiveInCallNotifications;
    private CompletableFuture<Void> mNotificationFuture;

    private InCallNotificationController(Context context) {
        mContext = context;

        mShowFullscreenIncallUi = mContext.getResources().getBoolean(
                R.bool.config_show_hun_fullscreen_incall_ui);
        mNotificationManager =
                (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);

        CharSequence name = mContext.getString(R.string.in_call_notification_channel_name);
        NotificationChannel notificationChannel = new NotificationChannel(CHANNEL_ID, name,
                NotificationManager.IMPORTANCE_HIGH);
        mNotificationManager.createNotificationChannel(notificationChannel);

        mNotificationBuilder = new Notification.Builder(mContext, CHANNEL_ID)
                .setSmallIcon(R.drawable.ic_phone)
                .setColor(mContext.getColor(R.color.notification_app_icon_color))
                .setCategory(Notification.CATEGORY_CALL)
                .setOngoing(true)
                .setAutoCancel(false);

        mActiveInCallNotifications = new HashSet<>();
    }


    /** Show a new incoming call notification or update the existing incoming call notification. */
    public void showInCallNotification(Call call) {
        L.d(TAG, "showInCallNotification");

        if (mNotificationFuture != null) {
            mNotificationFuture.cancel(true);
        }

        CallDetail callDetail = CallDetail.fromTelecomCallDetail(call.getDetails());
        String number = callDetail.getNumber();
        String callId = call.getDetails().getTelecomCallId();
        mActiveInCallNotifications.add(callId);

        if (mShowFullscreenIncallUi) {
            mNotificationBuilder.setFullScreenIntent(
                    getFullscreenIntent(call), /* highPriority= */true);
        }
        mNotificationBuilder
                .setLargeIcon((Icon) null)
                .setContentTitle(TelecomUtils.getBidiWrappedNumber(number))
                .setContentText(mContext.getString(R.string.notification_incoming_call))
                .setActions(
                        getAction(call, R.string.answer_call,
                                NotificationService.ACTION_ANSWER_CALL),
                        getAction(call, R.string.decline_call,
                                NotificationService.ACTION_DECLINE_CALL));
        mNotificationManager.notify(
                callId,
                NOTIFICATION_ID,
                mNotificationBuilder.build());

        mNotificationFuture = NotificationUtils.getDisplayNameAndRoundedAvatar(mContext, number)
                .thenAcceptAsync((pair) -> {
                    // Check that the notification hasn't already been dismissed
                    if (mActiveInCallNotifications.contains(callId)) {
                        mNotificationBuilder
                                .setLargeIcon(pair.second)
                                .setContentTitle(TelecomUtils.getBidiWrappedNumber(pair.first));

                        mNotificationManager.notify(
                                callId,
                                NOTIFICATION_ID,
                                mNotificationBuilder.build());
                    }
                }, mContext.getMainExecutor());
    }

    /** Cancel the incoming call notification for the given call. */
    public void cancelInCallNotification(Call call) {
        L.d(TAG, "cancelInCallNotification");
        if (call.getDetails() != null) {
            String callId = call.getDetails().getTelecomCallId();
            cancelInCallNotification(callId);
        }
    }

    /**
     * Cancel the incoming call notification for the given call id. Any action that dismisses the
     * notification needs to call this explicitly.
     */
    void cancelInCallNotification(String callId) {
        if (TextUtils.isEmpty(callId)) {
            return;
        }
        mActiveInCallNotifications.remove(callId);
        mNotificationManager.cancel(callId, NOTIFICATION_ID);
    }

    private PendingIntent getFullscreenIntent(Call call) {
        Intent intent = getIntent(NotificationService.ACTION_SHOW_FULLSCREEN_UI, call);
        return PendingIntent.getService(mContext, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private Notification.Action getAction(Call call, @StringRes int actionText,
            String intentAction) {
        CharSequence text = mContext.getString(actionText);
        PendingIntent intent = PendingIntent.getService(
                mContext,
                0,
                getIntent(intentAction, call),
                PendingIntent.FLAG_UPDATE_CURRENT);
        return new Notification.Action.Builder(null, text, intent).build();
    }

    private Intent getIntent(String action, Call call) {
        Intent intent = new Intent(action, null, mContext, NotificationService.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(NotificationService.EXTRA_PHONE_NUMBER,
                call.getDetails().getTelecomCallId());
        return intent;
    }
}
