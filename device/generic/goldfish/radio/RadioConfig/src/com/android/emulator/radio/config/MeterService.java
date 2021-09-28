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

package com.android.emulator.radio.config;

import android.app.Notification;
import android.app.Notification.Builder;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.os.ServiceManager;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.SubscriptionPlan;
import android.telephony.TelephonyManager;
import android.util.Log;
import java.time.Period;
import java.time.ZonedDateTime;
import java.util.Arrays;
import java.util.List;

public class MeterService extends Service {
  public MeterService() {}
  private final String TAG = "MeterService";

  private String createNotificationChannel(String channelId, String channelName) {
    NotificationChannel chan =
        new NotificationChannel(channelId, channelName, NotificationManager.IMPORTANCE_LOW);
    NotificationManager service =
        getApplicationContext().getSystemService(NotificationManager.class);
    service.createNotificationChannel(chan);
    return channelId;
  }

  @Override
  public int onStartCommand(Intent intent, int flags, int startId) {
    String msg = intent.getStringExtra("meter");
    String meterStatus = "METERED";
    if ("on".equals(msg)) {
      turnOn();
    } else if ("off".equals(msg)) {
      meterStatus = "TEMPORARILY_NOT_METERED";
      turnOff();
    }

    Notification.Builder builder =
        new Notification
            .Builder(this, createNotificationChannel("emulator_radio_config", "Meter Service"))
            .setContentTitle("Mobile Data Meter Status")
            .setContentText(meterStatus)
            .setSmallIcon(android.R.drawable.ic_info)
            .setAutoCancel(true);

    Notification notification = builder.build();

    startForeground(1, notification);

    return START_NOT_STICKY;
  }

  private void turnOn() {
    Log.v(TAG, "setting metered mobile data now");
    turn(false);
    Log.v(TAG, "done setting metered mobile data");
  }

  private void turnOff() {
    Log.v(TAG, "setting unmetered mobile data now");
    turn(true);
    Log.v(TAG, "done setting unmetered mobile data");
  }

  private void turn(boolean status) {
    SubscriptionManager mSm = getApplicationContext().getSystemService(SubscriptionManager.class);
    int mSubId = SubscriptionManager.getDefaultDataSubscriptionId();
    if (mSm.getSubscriptionPlans(mSubId).isEmpty()) {
      mSm.setSubscriptionPlans(mSubId, Arrays.asList(buildValidSubscriptionPlan()));
    }

    mSm.setSubscriptionOverrideUnmetered(mSubId, status, 0);
  }

  private static SubscriptionPlan buildValidSubscriptionPlan() {
    return SubscriptionPlan.Builder
        .createRecurring(ZonedDateTime.parse("2020-05-14T00:00:00.000Z"), Period.ofMonths(1))
        .setTitle("EmulatorDataPlan")
        .setDataLimit(2_000_000_000, SubscriptionPlan.LIMIT_BEHAVIOR_DISABLED)
        .setDataUsage(200_000_000, System.currentTimeMillis())
        .build();
  }

  @Override
  public IBinder onBind(Intent intent) {
    return null;
  }
}
