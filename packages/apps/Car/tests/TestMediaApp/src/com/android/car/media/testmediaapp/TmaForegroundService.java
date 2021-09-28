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

package com.android.car.media.testmediaapp;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.os.IBinder;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

import com.android.car.media.testmediaapp.prefs.TmaPrefsActivity;

/**
 * Service used to test and demonstrate the access to "foreground" permissions. In particular, this
 * implementation deals with location access from a headless service. This service is initiated
 * using {@link Service#startService(Intent)} from the browse service as a respond to a custom
 * playback command. Subsequent start commands make the service toggle between running and stopping.
 *
 * In real applications, this service would be handling background playback, maybe using location
 * and other sensors to automatically select songs.
 */
public class TmaForegroundService extends Service {
    public static final String CHANNEL_ID = "ForegroundServiceChannel";
    private LocationManager mLocationManager;

    @Override
    public void onCreate() {
        super.onCreate();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (doWork()) {
            createNotificationChannel();
            Intent notificationIntent = new Intent(this, TmaPrefsActivity.class);
            PendingIntent pendingIntent = PendingIntent.getActivity(this,
                    0, notificationIntent, 0);
            Notification notification = new NotificationCompat.Builder(this, CHANNEL_ID)
                    .setContentTitle("Foreground Service")
                    .setSmallIcon(R.drawable.ic_app_icon)
                    .setContentIntent(pendingIntent)
                    .build();
            startForeground(1, notification);
        } else {
            getMainExecutor().execute(this::stopSelf);
        }

        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        if (mLocationManager != null) {
            mLocationManager.removeUpdates(mLocationListener);
            toast("Location is off");
        }
        super.onDestroy();
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void createNotificationChannel() {
        NotificationChannel serviceChannel = new NotificationChannel(
                CHANNEL_ID,
                "Foreground Service Channel",
                NotificationManager.IMPORTANCE_DEFAULT
        );
        NotificationManager manager = getSystemService(NotificationManager.class);
        manager.createNotificationChannel(serviceChannel);
    }

    private boolean doWork() {
        if (mLocationManager != null) {
            return false;
        }
        mLocationManager = (LocationManager) getSystemService(LOCATION_SERVICE);
        try {
            mLocationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 2000,
                    0, mLocationListener);
            toast("Location is on");
        } catch (Throwable e) {
            toast("Unable to get location: " + e.getMessage());
        }
        return true;
    }

    /**
     * We use toasts here as it is the only way for a headless service to show something on the
     * screen. Real application shouldn't be using toasts from service.
     */
    private void toast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
    }

    private final LocationListener mLocationListener = new LocationListener() {
        @Override
        public void onLocationChanged(Location location) {
            toast("Location provider: " + location.getLatitude() + ":" + location.getLongitude());
        }

        @Override
        public void onStatusChanged(String provider, int status, Bundle extras) {
            toast("Location provider: " + provider + " status changed to: " + status);
        }

        @Override
        public void onProviderEnabled(String provider) {
            toast("Location provider enabled: " + provider);
        }

        @Override
        public void onProviderDisabled(String provider) {
            toast("Location provider disabled: " + provider);
        }
    };
}
