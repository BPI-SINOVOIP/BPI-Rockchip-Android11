/*
 * Copyright 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.server.telecom.ui;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.telecom.Log;

import com.android.server.telecom.Call;
import com.android.server.telecom.CallState;
import com.android.server.telecom.CallsManagerListenerBase;
import com.android.server.telecom.R;

/**
 * Displays a persistent notification whenever there's a call in the AUDIO_PROCESSING state so that
 * the user is aware that there's some app
 */
public class AudioProcessingNotification extends CallsManagerListenerBase {

    private static final int AUDIO_PROCESSING_NOTIFICATION_ID = 2;
    private static final String NOTIFICATION_TAG =
            AudioProcessingNotification.class.getSimpleName();

    private final Context mContext;
    private final NotificationManager mNotificationManager;
    private Call mCallInAudioProcessing;

    public AudioProcessingNotification(Context context) {
        mContext = context;
        mNotificationManager =
                (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);
    }

    @Override
    public void onCallStateChanged(Call call, int oldState, int newState) {
        if (newState == CallState.AUDIO_PROCESSING && oldState != CallState.AUDIO_PROCESSING) {
            showAudioProcessingNotification(call);
        } else if (oldState == CallState.AUDIO_PROCESSING
                && newState != CallState.AUDIO_PROCESSING) {
            cancelAudioProcessingNotification();
        }
    }

    @Override
    public void onCallAdded(Call call) {
        if (call.getState() == CallState.AUDIO_PROCESSING) {
            showAudioProcessingNotification(call);
        }
    }

    @Override
    public void onCallRemoved(Call call) {
        if (call == mCallInAudioProcessing) {
            cancelAudioProcessingNotification();
        }
    }

    /**
     * Create a system notification for the audio processing call.
     *
     * @param call The missed call.
     */
    private void showAudioProcessingNotification(Call call) {
        Log.i(this, "showAudioProcessingNotification");
        mCallInAudioProcessing = call;

        Notification.Builder builder = new Notification.Builder(mContext,
                NotificationChannelManager.CHANNEL_ID_AUDIO_PROCESSING);
        builder.setSmallIcon(R.drawable.ic_phone)
                .setColor(mContext.getResources().getColor(R.color.theme_color))
                .setContentTitle(mContext.getText(R.string.notification_audioProcessing_title))
                .setStyle(new Notification.BigTextStyle()
                        .bigText(mContext.getString(
                                R.string.notification_audioProcessing_body,
                                call.getAudioProcessingRequestingApp())))
                .setOngoing(true);

        Notification notification = builder.build();

        mNotificationManager.notify(
                NOTIFICATION_TAG, AUDIO_PROCESSING_NOTIFICATION_ID, notification);
    }

    /** Cancels the audio processing notification. */
    private void cancelAudioProcessingNotification() {
        mNotificationManager.cancel(NOTIFICATION_TAG, AUDIO_PROCESSING_NOTIFICATION_ID);
    }
}
