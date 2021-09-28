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
 * limitations under the License
 */

package android.systemui.cts.audiorecorder.base;


import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;

public abstract class BaseAudioRecorderService extends Service {
    private static final String ACTION_START =
            "android.systemui.cts.audiorecorder.ACTION_START";
    private static final String ACTION_STOP =
            "android.systemui.cts.audiorecorder.ACTION_STOP";
    private static final String ACTION_THROW =
            "android.systemui.cts.audiorecorder.ACTION_THROW";

    private static final String NOTIFICATION_CHANNEL_ID = "all";
    private static final String NOTIFICATION_CHANNEL_NAME = "All Notifications";
    private static final int NOTIFICATION_ID = 1;
    private static final String NOTIFICATION_TITLE = "Audio Record Service";
    private static final String NOTIFICATION_TEXT = "Recording...";

    private Handler mHandler;
    private final Runnable mCrashRunnable = () -> {
        throw new RuntimeException("Commanded to throw!");
    };

    @Override
    public void onCreate() {
        super.onCreate();
        mHandler = new Handler(Looper.getMainLooper());
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        startForeground(NOTIFICATION_ID, buildNotification());

        final String action = intent.getAction();
        if (ACTION_START.equals(action) && !isRecording()) {
            startRecording();
        } else if (ACTION_STOP.equals(action) && isRecording()) {
            stopRecording();
        } else if (ACTION_THROW.equals(action)) {
            // If the service crashes in onStartCommand() the system will try to re-start it. We do
            // not want that. So we post "crash" commands.
            mHandler.post(mCrashRunnable);
        }

        if (!isRecording()) {
            stopForeground(true);
            stopSelf();
        }

        return START_NOT_STICKY;
    }

    protected abstract void startRecording();

    protected abstract void stopRecording();

    protected abstract boolean isRecording();

    @Override
    public void onDestroy() {
        if (isRecording()) {
            stopRecording();
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private Notification buildNotification() {
        // Make sure the channel exists
        createChannel();

        return new Notification.Builder(this, NOTIFICATION_CHANNEL_ID)
                .setContentTitle(NOTIFICATION_TITLE)
                .setContentText(NOTIFICATION_TEXT)
                .setSmallIcon(R.drawable.ic_fg)
                .build();
    }

    private void createChannel() {
        final NotificationChannel channel =
                new NotificationChannel(
                        NOTIFICATION_CHANNEL_ID,
                        NOTIFICATION_CHANNEL_NAME,
                        NotificationManager.IMPORTANCE_NONE);
        final NotificationManager manager =
                (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        manager.createNotificationChannel(channel);
    }
}
