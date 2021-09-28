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
package android.view.cts.surfacevalidator;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.Icon;
import android.os.Binder;
import android.os.IBinder;

public class LocalMediaProjectionService extends Service {

    private final IBinder mBinder = new LocalBinder();
    private Bitmap mTestBitmap;

    private static final String NOTIFICATION_CHANNEL_ID = "Surfacevalidator";
    private static final String CHANNEL_NAME = "ProjectionService";

    public class LocalBinder extends Binder {
        LocalMediaProjectionService getService() {
            return LocalMediaProjectionService.this;
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        startForeground();
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    @Override
    public void onDestroy() {
        if (mTestBitmap != null) {
            mTestBitmap.recycle();
            mTestBitmap = null;
        }
        super.onDestroy();
    }

    private Icon createNotificationIcon() {
        mTestBitmap = Bitmap.createBitmap(50, 50, Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(mTestBitmap);
        canvas.drawColor(Color.BLUE);
        return Icon.createWithBitmap(mTestBitmap);
    }

    private void startForeground() {
        final NotificationChannel channel = new NotificationChannel(NOTIFICATION_CHANNEL_ID,
                CHANNEL_NAME, NotificationManager.IMPORTANCE_NONE);
        channel.setLockscreenVisibility(Notification.VISIBILITY_PRIVATE);

        final NotificationManager notificationManager =
                (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.createNotificationChannel(channel);

        final Notification.Builder notificationBuilder =
                new Notification.Builder(this, NOTIFICATION_CHANNEL_ID);

        final Notification notification = notificationBuilder.setOngoing(true)
                .setContentTitle("App is running")
                .setSmallIcon(createNotificationIcon())
                .setCategory(Notification.CATEGORY_SERVICE)
                .setContentText("Context")
                .build();

        startForeground(2, notification);
    }

}
