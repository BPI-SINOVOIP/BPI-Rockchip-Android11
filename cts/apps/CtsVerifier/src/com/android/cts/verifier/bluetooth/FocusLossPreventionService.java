package com.android.cts.verifier.bluetooth;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import com.android.cts.verifier.R;

public class FocusLossPreventionService extends Service {

  public static final String TAG = "FocusLossPreventionService";

  private static final String NOTIFICATION_CHANNEL_ID = "ctsVerifier/" + TAG;

  @Override
  public void onCreate() {
    super.onCreate();
  }

  @Override
  public int onStartCommand(Intent intent, int flags, int startId) {
    Context context = getApplicationContext();
    String title = getResources().getString(R.string.app_name);
    String channelId = "default";

    NotificationManager notificationManager = getSystemService(NotificationManager.class);

    if (notificationManager != null) {
      notificationManager.createNotificationChannel(
          new NotificationChannel(
              NOTIFICATION_CHANNEL_ID,
              NOTIFICATION_CHANNEL_ID,
              NotificationManager.IMPORTANCE_DEFAULT));

      Notification notification =
          new Notification.Builder(context, NOTIFICATION_CHANNEL_ID)
              .setContentTitle(title)
              .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
              .build();

      startForeground(1, notification);
    }

    return START_NOT_STICKY;
  }

  @Override
  public void onDestroy() {
    super.onDestroy();
    stopForeground(true /* removeNotification */);
  }

  @Override
  public IBinder onBind(Intent intent) {
    return null;
  }
}
