/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.android.cts.net.hostside.app2;

import static android.net.ConnectivityManager.ACTION_RESTRICT_BACKGROUND_CHANGED;

import static com.android.cts.net.hostside.app2.Common.ACTION_RECEIVER_READY;
import static com.android.cts.net.hostside.app2.Common.DYNAMIC_RECEIVER;
import static com.android.cts.net.hostside.app2.Common.TAG;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

import com.android.cts.net.hostside.IMyService;
import com.android.cts.net.hostside.INetworkCallback;

/**
 * Service used to dynamically register a broadcast receiver.
 */
public class MyService extends Service {
    private static final String NOTIFICATION_CHANNEL_ID = "MyService";

    ConnectivityManager mCm;

    private MyBroadcastReceiver mReceiver;
    private ConnectivityManager.NetworkCallback mNetworkCallback;

    // TODO: move MyBroadcast static functions here - they were kept there to make git diff easier.

    private IMyService.Stub mBinder =
        new IMyService.Stub() {

        @Override
        public void registerBroadcastReceiver() {
            if (mReceiver != null) {
                Log.d(TAG, "receiver already registered: " + mReceiver);
                return;
            }
            final Context context = getApplicationContext();
            mReceiver = new MyBroadcastReceiver(DYNAMIC_RECEIVER);
            context.registerReceiver(mReceiver, new IntentFilter(ACTION_RECEIVER_READY));
            context.registerReceiver(mReceiver,
                    new IntentFilter(ACTION_RESTRICT_BACKGROUND_CHANGED));
            Log.d(TAG, "receiver registered");
        }

        @Override
        public int getCounters(String receiverName, String action) {
            return MyBroadcastReceiver.getCounter(getApplicationContext(), action, receiverName);
        }

        @Override
        public String checkNetworkStatus() {
            return MyBroadcastReceiver.checkNetworkStatus(getApplicationContext());
        }

        @Override
        public String getRestrictBackgroundStatus() {
            return MyBroadcastReceiver.getRestrictBackgroundStatus(getApplicationContext());
        }

        @Override
        public void sendNotification(int notificationId, String notificationType) {
            MyBroadcastReceiver .sendNotification(getApplicationContext(), NOTIFICATION_CHANNEL_ID,
                    notificationId, notificationType);
        }

        @Override
        public void registerNetworkCallback(INetworkCallback cb) {
            if (mNetworkCallback != null) {
                Log.d(TAG, "unregister previous network callback: " + mNetworkCallback);
                unregisterNetworkCallback();
            }
            Log.d(TAG, "registering network callback");

            mNetworkCallback = new ConnectivityManager.NetworkCallback() {
                @Override
                public void onBlockedStatusChanged(Network network, boolean blocked) {
                    try {
                        cb.onBlockedStatusChanged(network, blocked);
                    } catch (RemoteException e) {
                        Log.d(TAG, "Cannot send onBlockedStatusChanged: " + e);
                        unregisterNetworkCallback();
                    }
                }

                @Override
                public void onAvailable(Network network) {
                    try {
                        cb.onAvailable(network);
                    } catch (RemoteException e) {
                        Log.d(TAG, "Cannot send onAvailable: " + e);
                        unregisterNetworkCallback();
                    }
                }

                @Override
                public void onLost(Network network) {
                    try {
                        cb.onLost(network);
                    } catch (RemoteException e) {
                        Log.d(TAG, "Cannot send onLost: " + e);
                        unregisterNetworkCallback();
                    }
                }

                @Override
                public void onCapabilitiesChanged(Network network, NetworkCapabilities cap) {
                    try {
                        cb.onCapabilitiesChanged(network, cap);
                    } catch (RemoteException e) {
                        Log.d(TAG, "Cannot send onCapabilitiesChanged: " + e);
                        unregisterNetworkCallback();
                    }
                }
            };
            mCm.registerNetworkCallback(makeWifiNetworkRequest(), mNetworkCallback);
            try {
                cb.asBinder().linkToDeath(() -> unregisterNetworkCallback(), 0);
            } catch (RemoteException e) {
                unregisterNetworkCallback();
            }
        }

        @Override
        public void unregisterNetworkCallback() {
            Log.d(TAG, "unregistering network callback");
            if (mNetworkCallback != null) {
                mCm.unregisterNetworkCallback(mNetworkCallback);
                mNetworkCallback = null;
            }
        }
      };

    private NetworkRequest makeWifiNetworkRequest() {
        return new NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                .build();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    @Override
    public void onCreate() {
        final Context context = getApplicationContext();
        ((NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE))
                .createNotificationChannel(new NotificationChannel(NOTIFICATION_CHANNEL_ID,
                        NOTIFICATION_CHANNEL_ID, NotificationManager.IMPORTANCE_DEFAULT));
        mCm = (ConnectivityManager) getApplicationContext()
                .getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    @Override
    public void onDestroy() {
        final Context context = getApplicationContext();
        ((NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE))
                .deleteNotificationChannel(NOTIFICATION_CHANNEL_ID);
        if (mReceiver != null) {
            Log.d(TAG, "onDestroy(): unregistering " + mReceiver);
            getApplicationContext().unregisterReceiver(mReceiver);
        }

        super.onDestroy();
    }
}
