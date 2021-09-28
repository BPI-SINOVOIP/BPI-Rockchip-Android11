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

package com.android.car.ui.paintbooth.currentactivity;

import android.app.ActivityManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.TextView;
import android.widget.Toast;

import androidx.core.app.NotificationCompat;
import androidx.core.content.ContextCompat;

import com.android.car.ui.paintbooth.MainActivity;
import com.android.car.ui.paintbooth.R;
import com.android.car.ui.paintbooth.currentactivity.ActivityTaskManager.TaskStackListener;

import java.util.List;

/**
 * To start the service:
 * adb shell am start-foreground-service -n com.android.car.ui.paintbooth/.CurrentActivityService
 *
 * To stop the service:
 * adb shell am start-foreground-service -n com.android.car.ui.paintbooth/.CurrentActivityService -a
 * com.android.car.ui.paintbooth.StopService
 */
public class CurrentActivityService extends Service {
    private static final int FOREGROUND_SERVICE_ID = 111;

    private WindowManager mWindowManager;
    private TextView mTextView;
    private Handler mHandler;

    @Override
    public void onCreate() {
        mHandler = new Handler(Looper.getMainLooper());

        if (ContextCompat.checkSelfPermission(this, "android.permission.REAL_GET_TASKS")
                != PackageManager.PERMISSION_GRANTED) {
            Toast.makeText(this, "android.permission.REAL_GET_TASKS is not granted!",
                    Toast.LENGTH_LONG).show();
        }

        Intent notificationIntent = new Intent(this, CurrentActivityService.class);

        PendingIntent pendingIntent = PendingIntent.getActivity(this, 0,
                notificationIntent, 0);

        NotificationChannel channel = new NotificationChannel("CurrentActivityService",
                "Show current activity",
                NotificationManager.IMPORTANCE_DEFAULT);
        NotificationManager notificationManager =
                (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.createNotificationChannel(channel);

        Notification notification =
                new NotificationCompat.Builder(this, "CurrentActivityService")
                        .setSmallIcon(R.drawable.ic_launcher)
                        .setContentTitle("CurrentActivityService")
                        .setContentText("Show current activity")
                        .setContentIntent(pendingIntent).build();

        startForeground(FOREGROUND_SERVICE_ID, notification);

        try {
            ActivityTaskManager.getService().registerTaskStackListener(mTaskStackListener);
        } catch (RemoteException e) {
            Toast.makeText(this, e.getLocalizedMessage(), Toast.LENGTH_LONG).show();
        }

        mWindowManager = (WindowManager) getSystemService(WINDOW_SERVICE);
        final WindowManager.LayoutParams layoutParams = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.TYPE_SYSTEM_ALERT,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL,
                PixelFormat.TRANSLUCENT);

        mTextView = new TextView(this);
        layoutParams.gravity = Gravity.TOP | Gravity.LEFT;
        layoutParams.x = 0;
        layoutParams.y = 100;
        mTextView.setLayoutParams(layoutParams);
        mTextView.setBackgroundColor(Color.argb(50, 0, 255, 0));

        mTextView.setOnTouchListener(new View.OnTouchListener() {

            private int mInitialX = 0;
            private int mInitialY = 0;
            private float mInitialTouchX;
            private float mInitialTouchY;

            @Override
            public boolean onTouch(View view, MotionEvent event) {
                switch (event.getAction() & MotionEvent.ACTION_MASK) {
                    case MotionEvent.ACTION_DOWN:
                        mInitialX = layoutParams.x;
                        mInitialY = layoutParams.y;
                        mInitialTouchX = event.getRawX();
                        mInitialTouchY = event.getRawY();
                        break;
                    case MotionEvent.ACTION_MOVE:
                        WindowManager.LayoutParams layoutParams =
                                (WindowManager.LayoutParams) view.getLayoutParams();
                        layoutParams.x = mInitialX + (int) (event.getRawX() - mInitialTouchX);
                        layoutParams.y = mInitialY + (int) (event.getRawY() - mInitialTouchY);
                        mWindowManager.updateViewLayout(view, layoutParams);
                        return true;
                    default:
                        break;
                }

                return false;
            }
        });

        try {
            mWindowManager.addView(mTextView, layoutParams);
        } catch (RuntimeException e) {
            Toast.makeText(this, "Couldn't display overlay", Toast.LENGTH_SHORT)
                .show();
        }

        showCurrentTask();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (MainActivity.STOP_SERVICE.equals(intent.getAction())) {
            stopSelf();
        }

        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onDestroy() {
        mHandler.removeCallbacksAndMessages(null);
        mWindowManager.removeView(mTextView);
        try {
            ActivityTaskManager.getService().unregisterTaskStackListener(mTaskStackListener);
        } catch (RemoteException e) {
            Toast.makeText(this, e.getLocalizedMessage(), Toast.LENGTH_LONG).show();
        }
    }

    /**
     * This requires system permissions or else it will only fetch the current app and the launcher
     * app
     */
    private void showCurrentTask() {
        try {
            List<ActivityManager.RunningTaskInfo> tasks =
                    ActivityTaskManager.getService().getTasks(1);
            if (!tasks.isEmpty()) {
                updateComponentName(tasks.get(0).topActivity);
            }
        } catch (RemoteException e) {
            Toast.makeText(this, e.getLocalizedMessage(), Toast.LENGTH_LONG).show();
        }
    }

    private void updateComponentName(ComponentName componentName) {
        mHandler.post(() -> {
            if (mTextView != null && componentName != null) {
                mTextView.setText(componentName.flattenToShortString().replace('/', '\n'));
            }
        });
    }

    private final TaskStackListener mTaskStackListener = new TaskStackListener() {
        @Override
        public void onTaskCreated(int taskId, ComponentName componentName) {
            updateComponentName(componentName);
        }

        @Override
        public void onTaskRemoved(int taskId) {
            showCurrentTask();
        }

        @Override
        public void onTaskDescriptionChanged(ActivityManager.RunningTaskInfo taskInfo) {
            updateComponentName(taskInfo.topActivity);
        }

        @Override
        public void onTaskMovedToFront(ActivityManager.RunningTaskInfo taskInfo) {
            updateComponentName(taskInfo.topActivity);
        }
    };
}
