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
package com.google.android.car.userswitchmonitor;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.car.Car;
import android.car.user.CarUserManager;
import android.car.user.CarUserManager.UserLifecycleEvent;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

/**
 * Service that users {@link CarUserManager.UserLifecycleEvent UserLifecycleEvents} to monitor
 * user switches.
 *
 */
public final class UserSwitchMonitorService extends Service {

    static final String TAG = "UserSwitchMonitor";

    private final Object mLock = new Object();

    private final int mUserId = android.os.Process.myUserHandle().getIdentifier();

    private final List<UserLifecycleEvent> mEvents = new ArrayList<>();

    private final CarUserManager.UserLifecycleListener mListener = (e) -> {
        Log.d(TAG, "onEvent(" + mUserId + "): " + e);
        synchronized (mLock) {
            mEvents.add(e);
        }
    };

    private Context mContext;
    private Car mCar;
    private CarUserManager mCarUserManager;
    private NotificationManager mNotificationManager;

    @Override
    public void onCreate() {
        mContext = getApplicationContext();
        mCar = Car.createCar(mContext);
        mCarUserManager = (CarUserManager) mCar.getCarManager(Car.CAR_USER_SERVICE);
        mCarUserManager.addListener((r)-> r.run(), mListener);

        mNotificationManager = mContext.getSystemService(NotificationManager.class);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(TAG, "onStartCommand(" + mUserId + "): " + intent);

        String channelId = "4815162342";
        String name = "UserSwitchMonitor";
        NotificationChannel channel = new NotificationChannel(channelId, name,
                NotificationManager.IMPORTANCE_MIN);
        mNotificationManager.createNotificationChannel(channel);

        startForeground(startId,
                new Notification.Builder(mContext, channelId)
                        .setContentText(name)
                        .setContentTitle(name)
                        .setSmallIcon(R.drawable.ic_launcher)
                        .build());

        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "onDestroy(" + mUserId + ")");

        if (mCarUserManager != null) {
            mCarUserManager.removeListener(mListener);
        } else {
            Log.w(TAG, "Cannot remove listener because manager is null");
        }
        if (mCar != null && mCar.isConnected()) {
            mCar.disconnect();
        }
        super.onDestroy();
    }

    @Override
    protected void dump(FileDescriptor fd, PrintWriter pw, String[] args) {
        pw.printf("User id: %d\n", mUserId);
        synchronized (mLock) {
            if (mEvents.isEmpty()) {
                pw.println("Did not receive any event yet");
                return;
            }
            int size = mEvents.size();
            String indent = "  ";
            pw.printf("Received %d events:\n", size);
            for (int i = 0; i < size; i++) {
                pw.printf("%s%d: %s\n", indent, (i + 1), mEvents.get(i));
            }
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "onBind(): " + intent);
        return null;
    }

}
