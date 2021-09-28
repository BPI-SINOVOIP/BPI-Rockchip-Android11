/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.server.cts.device.statsd;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.app.NotificationManager;
import android.app.usage.NetworkStatsManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Point;
import android.net.ConnectivityManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.RemoteException;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;

/** An activity (to be run as a foreground process) which performs one of a number of actions. */
public class StatsdCtsForegroundActivity extends Activity {
    private static final String TAG = StatsdCtsForegroundActivity.class.getSimpleName();

    public static final String KEY_ACTION = "action";
    public static final String ACTION_END_IMMEDIATELY = "action.end_immediately";
    public static final String ACTION_SLEEP_WHILE_TOP = "action.sleep_top";
    public static final String ACTION_LONG_SLEEP_WHILE_TOP = "action.long_sleep_top";
    public static final String ACTION_SHOW_APPLICATION_OVERLAY = "action.show_application_overlay";
    public static final String ACTION_SHOW_NOTIFICATION = "action.show_notification";
    public static final String ACTION_CRASH = "action.crash";
    public static final String ACTION_CREATE_CHANNEL_GROUP = "action.create_channel_group";
    public static final String ACTION_POLL_NETWORK_STATS = "action.poll_network_stats";

    public static final int SLEEP_OF_ACTION_SLEEP_WHILE_TOP = 2_000;
    public static final int SLEEP_OF_ACTION_SHOW_APPLICATION_OVERLAY = 2_000;
    public static final int LONG_SLEEP_WHILE_TOP = 60_000;

    @Override
    public void onCreate(Bundle bundle) {
        super.onCreate(bundle);

        Intent intent = this.getIntent();
        if (intent == null) {
            Log.e(TAG, "Intent was null.");
            finish();
        }

        String action = intent.getStringExtra(KEY_ACTION);
        Log.i(TAG, "Starting " + action + " from foreground activity.");

        switch (action) {
            case ACTION_END_IMMEDIATELY:
                finish();
                break;
            case ACTION_SLEEP_WHILE_TOP:
                doSleepWhileTop(SLEEP_OF_ACTION_SLEEP_WHILE_TOP);
                break;
            case ACTION_LONG_SLEEP_WHILE_TOP:
                doSleepWhileTop(LONG_SLEEP_WHILE_TOP);
                break;
            case ACTION_SHOW_APPLICATION_OVERLAY:
                doShowApplicationOverlay();
                break;
            case ACTION_SHOW_NOTIFICATION:
                doShowNotification();
                break;
            case ACTION_CRASH:
                doCrash();
                break;
            case ACTION_CREATE_CHANNEL_GROUP:
                doCreateChannelGroup();
                break;
            case ACTION_POLL_NETWORK_STATS:
                doPollNetworkStats();
                break;
            default:
                Log.e(TAG, "Intent had invalid action " + action);
                finish();
        }
    }

    /** Does nothing, but asynchronously. */
    private void doSleepWhileTop(int sleepTime) {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                AtomTests.sleep(sleepTime);
                return null;
            }

            @Override
            protected void onPostExecute(Void nothing) {
                finish();
            }
        }.execute();
    }

    private void doShowApplicationOverlay() {
        // Adapted from BatteryStatsBgVsFgActions.java.
        final WindowManager wm = getSystemService(WindowManager.class);
        Point size = new Point();
        wm.getDefaultDisplay().getSize(size);

        WindowManager.LayoutParams wmlp = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE);
        wmlp.width = size.x / 4;
        wmlp.height = size.y / 4;
        wmlp.gravity = Gravity.CENTER | Gravity.LEFT;
        wmlp.setTitle(getPackageName());

        ViewGroup.LayoutParams vglp = new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT);

        View v = new View(this);
        v.setBackgroundColor(Color.GREEN);
        v.setLayoutParams(vglp);
        wm.addView(v, wmlp);

        // The overlay continues long after the finish. The following is just to end the activity.
        AtomTests.sleep(SLEEP_OF_ACTION_SHOW_APPLICATION_OVERLAY);
        finish();
    }

    private void doShowNotification() {
        final int notificationId = R.layout.activity_main;
        final String notificationChannelId = "StatsdCtsChannel";

        NotificationManager nm = getSystemService(NotificationManager.class);
        NotificationChannel channel = new NotificationChannel(notificationChannelId, "Statsd Cts",
                NotificationManager.IMPORTANCE_DEFAULT);
        channel.setDescription("Statsd Cts Channel");
        nm.createNotificationChannel(channel);

        nm.notify(
                notificationId,
                new Notification.Builder(this, notificationChannelId)
                        .setSmallIcon(android.R.drawable.stat_notify_chat)
                        .setContentTitle("StatsdCts")
                        .setContentText("StatsdCts")
                        .build());
        nm.cancel(notificationId);
        finish();
    }

    private void doCreateChannelGroup() {
        NotificationManager nm = getSystemService(NotificationManager.class);
        NotificationChannelGroup channelGroup = new NotificationChannelGroup("StatsdCtsGroup",
                "Statsd Cts Group");
        channelGroup.setDescription("StatsdCtsGroup Description");
        nm.createNotificationChannelGroup(channelGroup);
        finish();
    }

    // Trigger force poll on NetworkStatsService to make sure the service get most updated network
    // stats from lower layer on subsequent verifications.
    private void doPollNetworkStats() {
        final NetworkStatsManager nsm =
                (NetworkStatsManager) getSystemService(Context.NETWORK_STATS_SERVICE);

        // While the flag of force polling is the only important thing needed when making binder
        // call to service, the type, parameters and returned result of the query here do not
        // matter.
        try {
            nsm.setPollForce(true);
            nsm.querySummaryForUser(ConnectivityManager.TYPE_WIFI, null, Long.MIN_VALUE,
                    Long.MAX_VALUE);
        } catch (RemoteException e) {
            Log.e(TAG, "doPollNetworkStats failed with " + e);
        } finally {
            finish();
        }
    }

    @SuppressWarnings("ConstantOverflow")
    private void doCrash() {
        Log.e(TAG, "About to crash the app with 1/0 " + (long) 1 / 0);
    }
}
