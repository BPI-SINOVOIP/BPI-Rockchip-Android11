/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.car.media.service;

import static android.car.media.CarMediaManager.MEDIA_SOURCE_MODE_PLAYBACK;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.support.v4.media.session.PlaybackStateCompat;
import android.util.Log;

import androidx.core.app.NotificationCompat;
import androidx.lifecycle.LifecycleService;

import com.android.car.media.R;
import com.android.car.media.common.playback.PlaybackViewModel;

/**
 * This service is started by CarMediaService when a new user is unlocked. It connects to the
 * media source provided by CarMediaService and calls prepare() on the active MediaSession.
 * Additionally, CarMediaService can instruct this service to autoplay, in which case this service
 * will attempt to play the source before stopping.
 *
 * TODO(b/139497602): merge this class into CarMediaService, so it doesn't depend on Media Center
 */
public class MediaConnectorService extends LifecycleService {

    private static int FOREGROUND_NOTIFICATION_ID = 1;
    private static final String NOTIFICATION_CHANNEL_ID = "com.android.car.media.service";
    private static final String NOTIFICATION_CHANNEL_NAME = "MediaConnectorService";
    private static final String EXTRA_AUTOPLAY = "com.android.car.media.autoplay";

    @Override
    public void onCreate() {
        super.onCreate();
        NotificationChannel channel = new NotificationChannel(NOTIFICATION_CHANNEL_ID,
                NOTIFICATION_CHANNEL_NAME, NotificationManager.IMPORTANCE_NONE);
        NotificationManager manager =
                (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        manager.createNotificationChannel(channel);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return super.onBind(intent);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        super.onStartCommand(intent, flags, startId);

        final boolean autoplay = intent.getBooleanExtra(EXTRA_AUTOPLAY, false);
        PlaybackViewModel playbackViewModel = PlaybackViewModel.get(getApplication(),
                MEDIA_SOURCE_MODE_PLAYBACK);
        // Listen to playback state updates to determine which actions are supported;
        // relevant actions here are prepare() and play()
        // If we should autoplay the source, we wait until play() is available before we
        // stop the service, otherwise just calling prepare() is sufficient.
        playbackViewModel.getPlaybackStateWrapper().observe(this,
                playbackStateWrapper -> {
                    if (playbackStateWrapper != null) {
                        if (playbackStateWrapper.isPlaying()) {
                            stopSelf(startId);
                            return;
                        }
                        if ((playbackStateWrapper.getSupportedActions()
                                & PlaybackStateCompat.ACTION_PREPARE) != 0) {
                            playbackViewModel.getPlaybackController().getValue().prepare();
                            if (!autoplay) {
                                stopSelf(startId);
                            }
                        }
                        if (autoplay && (playbackStateWrapper.getSupportedActions()
                                & PlaybackStateCompat.ACTION_PLAY) != 0) {
                            playbackViewModel.getPlaybackController().getValue().play();
                            stopSelf(startId);
                        }
                    }
                });

        // Since this service is started from CarMediaService (which runs in background), we need
        // to call startForeground to prevent the system from stopping this service and ANRing.
        Notification notification = new NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID)
                .setSmallIcon(R.drawable.ic_music)
                .setContentTitle(getResources().getString(R.string.service_notification_title))
                .build();
        startForeground(FOREGROUND_NOTIFICATION_ID, notification);
        return START_NOT_STICKY;
    }
}
