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

package com.android.car.ui.paintbooth;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.os.IBinder;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;

import androidx.core.app.NotificationCompat;

/**
 * To start the service:
 * adb shell am start-foreground-service com.android.car.ui.paintbooth/.DisplayService
 *
 * To stop the service:
 * adb shell am stopservice com.android.car.ui.paintbooth/.DisplayService
 *
 * When the service is started it will draw a overlay view on top of the screen displayed. This
 * overlay comes from a SVG file that can be modified to take different shapes. This service will be
 * used to display different screen styles from OEMs.
 */
public class VisibleBoundsSimulator extends Service {
    private static final int FOREGROUND_SERVICE_ID = 222;
    private View mContainer;

    private WindowManager mWindowManager;

    @Override
    public IBinder onBind(Intent intent) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (MainActivity.STOP_SERVICE.equals(intent.getAction())) {
            stopSelf();
        }

        return START_STICKY;
    }

    @Override
    public void onCreate() {

        Intent notificationIntent = new Intent(this, VisibleBoundsSimulator.class);

        PendingIntent pendingIntent = PendingIntent.getActivity(this, 0,
                notificationIntent, 0);

        NotificationChannel channel = new NotificationChannel("DisplayService",
                "Show overlay screen",
                NotificationManager.IMPORTANCE_DEFAULT);
        NotificationManager notificationManager =
                (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.createNotificationChannel(channel);

        Notification notification =
                new NotificationCompat.Builder(this, "DisplayService")
                        .setSmallIcon(R.drawable.ic_launcher)
                        .setContentTitle("DisplayService")
                        .setContentText("Show overlay screen")
                        .setContentIntent(pendingIntent).build();

        startForeground(FOREGROUND_SERVICE_ID, notification);
        applyDisplayOverlay();
    }

    /**
     * Creates a view overlay on top of a new window. The overlay gravity is set to left and
     * bottom. If the width and height is not provided then the default is to take up the entire
     * screen. Overlay will show bounds around the view and we can still click through the window.
     */
    private void applyDisplayOverlay() {
        mWindowManager = (WindowManager) getSystemService(WINDOW_SERVICE);

        DisplayMetrics displayMetrics = new DisplayMetrics();

        mWindowManager.getDefaultDisplay().getRealMetrics(displayMetrics);
        int screenHeight = displayMetrics.heightPixels;
        int screenWidth = displayMetrics.widthPixels;
        LayoutInflater inflater = (LayoutInflater) getSystemService(LAYOUT_INFLATER_SERVICE);
        final WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.TYPE_SYSTEM_ALERT,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE,
                PixelFormat.TRANSLUCENT);

        params.packageName = this.getPackageName();
        params.gravity = Gravity.BOTTOM | Gravity.LEFT;

        Display display = mWindowManager.getDefaultDisplay();
        Point size = new Point();
        display.getSize(size);
        int height = size.y;

        params.x = 0;
        // If the sysUI is showing and nav bar is taking up some space at the bottom we want to
        // offset the height of the navBar so that the overlay starts from the bottom left.
        params.y = -(screenHeight - height);

        float overlayWidth = getApplicationContext().getResources().getDimension(
                R.dimen.screen_shape_container_width);
        float overlayHeight = getApplicationContext().getResources().getDimension(
                R.dimen.screen_shape_container_height);


        params.width = (int) (overlayWidth == 0 ? screenWidth : overlayHeight);
        params.height = (int) (overlayHeight == 0 ? screenHeight : overlayHeight);
        params.setTitle("Simulated display bound");

        mContainer = inflater.inflate(R.layout.simulated_screen_shape_container, null);
        mContainer.setLayoutParams(params);

        mWindowManager.addView(mContainer, params);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mWindowManager.removeView(mContainer);
        stopSelf();
    }
}
