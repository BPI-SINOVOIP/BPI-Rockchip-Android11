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

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.telecom.Call;
import android.text.TextUtils;

import com.android.car.dialer.Constants;
import com.android.car.dialer.telecom.UiCallManager;
import com.android.car.dialer.ui.activecall.InCallActivity;
import com.android.car.telephony.common.TelecomUtils;

import java.util.List;

/**
 * A {@link Service} that is used to handle actions from notifications to:
 * <ul><li>answer or inject an incoming call.
 * <li>call back or message to a missed call.
 */
public class NotificationService extends Service {
    static final String ACTION_ANSWER_CALL = "CD.ACTION_ANSWER_CALL";
    static final String ACTION_DECLINE_CALL = "CD.ACTION_DECLINE_CALL";
    static final String ACTION_SHOW_FULLSCREEN_UI = "CD.ACTION_SHOW_FULLSCREEN_UI";
    static final String ACTION_CALL_BACK_MISSED = "CD.ACTION_CALL_BACK_MISSED";
    static final String ACTION_MESSAGE_MISSED = "CD.ACTION_MESSAGE_MISSED";
    static final String ACTION_READ_MISSED = "CD.ACTION_READ_MISSED";
    static final String EXTRA_PHONE_NUMBER = "CD.EXTRA_PHONE_NUMBER";
    static final String EXTRA_CALL_LOG_ID = "CD.EXTRA_CALL_LOG_ID";
    static final String EXTRA_NOTIFICATION_TAG = "CD.EXTRA_NOTIFICATION_TAG";

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    /** Create an intent to handle reading all missed call action and schedule for executing. */
    public static void readAllMissedCall(Context context) {
        Intent readAllMissedCallIntent = new Intent(context, NotificationService.class);
        readAllMissedCallIntent.setAction(ACTION_READ_MISSED);
        context.startService(readAllMissedCallIntent);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String action = intent.getAction();
        String phoneNumber = intent.getStringExtra(EXTRA_PHONE_NUMBER);
        String notificationTag = intent.getStringExtra(EXTRA_NOTIFICATION_TAG);
        Context context = getApplicationContext();
        switch (action) {
            case ACTION_ANSWER_CALL:
                answerCall(phoneNumber);
                InCallNotificationController.get().cancelInCallNotification(phoneNumber);
                break;
            case ACTION_DECLINE_CALL:
                declineCall(phoneNumber);
                InCallNotificationController.get().cancelInCallNotification(phoneNumber);
                break;
            case ACTION_SHOW_FULLSCREEN_UI:
                Intent inCallActivityIntent = new Intent(context, InCallActivity.class);
                inCallActivityIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                inCallActivityIntent.putExtra(Constants.Intents.EXTRA_SHOW_INCOMING_CALL, true);
                startActivity(inCallActivityIntent);
                InCallNotificationController.get().cancelInCallNotification(phoneNumber);
                break;
            case ACTION_CALL_BACK_MISSED:
                UiCallManager.get().placeCall(phoneNumber);
                TelecomUtils.markCallLogAsRead(context, phoneNumber);
                MissedCallNotificationController.get().cancelMissedCallNotification(
                        notificationTag);
                break;
            case ACTION_MESSAGE_MISSED:
                // TODO: call assistant to send message
                TelecomUtils.markCallLogAsRead(context, phoneNumber);
                MissedCallNotificationController.get().cancelMissedCallNotification(
                        notificationTag);
                break;
            case ACTION_READ_MISSED:
                if (!TextUtils.isEmpty(phoneNumber)) {
                    TelecomUtils.markCallLogAsRead(context, phoneNumber);
                } else {
                    long callLogId = intent.getLongExtra(EXTRA_CALL_LOG_ID, -1);
                    TelecomUtils.markCallLogAsRead(context, callLogId);
                }
                MissedCallNotificationController.get().cancelMissedCallNotification(
                        notificationTag);
                break;
            default:
                break;
        }

        return START_NOT_STICKY;
    }

    private void answerCall(String callId) {
        List<Call> callList = UiCallManager.get().getCallList();
        for (Call call : callList) {
            if (call.getDetails() != null
                    && TextUtils.equals(call.getDetails().getTelecomCallId(), callId)) {
                call.answer(/* videoState= */0);
                return;
            }
        }
    }

    private void declineCall(String callId) {
        List<Call> callList = UiCallManager.get().getCallList();
        for (Call call : callList) {
            if (call.getDetails() != null
                    && TextUtils.equals(call.getDetails().getTelecomCallId(), callId)) {
                call.reject(false, /* textMessage= */"");
                return;
            }
        }
    }
}
