/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.app.stubs;

import android.app.ActivityManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.util.ArrayMap;
import android.util.Log;

public class CommandReceiver extends BroadcastReceiver {

    private static final String TAG = "CommandReceiver";

    // Requires flags and targetPackage
    public static final int COMMAND_BIND_SERVICE = 1;
    // Requires targetPackage
    public static final int COMMAND_UNBIND_SERVICE = 2;
    public static final int COMMAND_START_FOREGROUND_SERVICE = 3;
    public static final int COMMAND_STOP_FOREGROUND_SERVICE = 4;
    public static final int COMMAND_START_FOREGROUND_SERVICE_LOCATION = 5;
    public static final int COMMAND_STOP_FOREGROUND_SERVICE_LOCATION = 6;
    public static final int COMMAND_START_ALERT_SERVICE = 7;
    public static final int COMMAND_STOP_ALERT_SERVICE = 8;
    public static final int COMMAND_SELF_INDUCED_ANR = 9;
    public static final int COMMAND_START_ACTIVITY = 10;
    public static final int COMMAND_STOP_ACTIVITY = 11;
    public static final int COMMAND_CREATE_FGSL_PENDING_INTENT = 12;
    public static final int COMMAND_SEND_FGSL_PENDING_INTENT = 13;
    public static final int COMMAND_BIND_FOREGROUND_SERVICE = 14;

    public static final String EXTRA_COMMAND = "android.app.stubs.extra.COMMAND";
    public static final String EXTRA_TARGET_PACKAGE = "android.app.stubs.extra.TARGET_PACKAGE";
    public static final String EXTRA_FLAGS = "android.app.stubs.extra.FLAGS";

    public static final String SERVICE_NAME = "android.app.stubs.LocalService";
    public static final String FG_SERVICE_NAME = "android.app.stubs.LocalForegroundService";
    public static final String FG_LOCATION_SERVICE_NAME =
            "android.app.stubs.LocalForegroundServiceLocation";

    public static final String ACTIVITY_NAME = "android.app.stubs.SimpleActivity";

    private static ArrayMap<String,ServiceConnection> sServiceMap = new ArrayMap<>();

    // Map a packageName to a Intent that starts an Activity.
    private static ArrayMap<String, Intent> sActivityIntent = new ArrayMap<>();

    // Map a packageName to a PendingIntent.
    private static ArrayMap<String, PendingIntent> sPendingIntent = new ArrayMap<>();

    /**
     * Handle the different types of binding/unbinding requests.
     * @param context The Context in which the receiver is running.
     * @param intent The Intent being received.
     */
    @Override
    public void onReceive(Context context, Intent intent) {
        // Use the application context as the receiver context could be restricted.
        context = context.getApplicationContext();
        int command = intent.getIntExtra(EXTRA_COMMAND, -1);
        Log.d(TAG + "_" + context.getPackageName(), "Got command " + command + ", intent="
                + intent);
        switch (command) {
            case COMMAND_BIND_SERVICE:
                doBindService(context, intent, SERVICE_NAME);
                break;
            case COMMAND_UNBIND_SERVICE:
                doUnbindService(context, intent);
                break;
            case COMMAND_START_FOREGROUND_SERVICE:
                doStartForegroundService(context, intent);
                break;
            case COMMAND_STOP_FOREGROUND_SERVICE:
                doStopForegroundService(context, intent, FG_SERVICE_NAME);
                break;
            case COMMAND_START_FOREGROUND_SERVICE_LOCATION:
                doStartForegroundServiceWithType(context, intent);
                break;
            case COMMAND_STOP_FOREGROUND_SERVICE_LOCATION:
                doStopForegroundService(context, intent, FG_LOCATION_SERVICE_NAME);
                break;
            case COMMAND_START_ALERT_SERVICE:
                doStartAlertService(context);
                break;
            case COMMAND_STOP_ALERT_SERVICE:
                doStopAlertService(context);
                break;
            case COMMAND_SELF_INDUCED_ANR:
                doSelfInducedAnr(context);
                break;
            case COMMAND_START_ACTIVITY:
                doStartActivity(context, intent);
                break;
            case COMMAND_STOP_ACTIVITY:
                doStopActivity(context, intent);
                break;
            case COMMAND_CREATE_FGSL_PENDING_INTENT:
                doCreateFgslPendingIntent(context, intent);
                break;
            case COMMAND_SEND_FGSL_PENDING_INTENT:
                doSendFgslPendingIntent(context, intent);
                break;
            case COMMAND_BIND_FOREGROUND_SERVICE:
                doBindService(context, intent, FG_LOCATION_SERVICE_NAME);
                break;
        }
    }

    private void doBindService(Context context, Intent commandIntent, String serviceName) {
        String targetPackage = getTargetPackage(commandIntent);
        int flags = getFlags(commandIntent);

        Intent bindIntent = new Intent();
        bindIntent.setComponent(new ComponentName(targetPackage, serviceName));

        ServiceConnection connection = addServiceConnection(targetPackage);

        context.bindService(bindIntent, connection, flags | Context.BIND_AUTO_CREATE);
    }

    private void doUnbindService(Context context, Intent commandIntent) {
        String targetPackage = getTargetPackage(commandIntent);
        context.unbindService(sServiceMap.remove(targetPackage));
    }

    private void doStartForegroundService(Context context, Intent commandIntent) {
        String targetPackage = getTargetPackage(commandIntent);
        Intent fgsIntent = new Intent();
        fgsIntent.setComponent(new ComponentName(targetPackage, FG_SERVICE_NAME));
        int command = LocalForegroundService.COMMAND_START_FOREGROUND;
        fgsIntent.putExtras(LocalForegroundService.newCommand(new Binder(), command));
        context.startForegroundService(fgsIntent);
    }

    private void doStartForegroundServiceWithType(Context context, Intent commandIntent) {
        String targetPackage = getTargetPackage(commandIntent);
        Intent fgsIntent = new Intent();
        fgsIntent.putExtras(commandIntent); // include the fg service type if any.
        fgsIntent.setComponent(new ComponentName(targetPackage, FG_LOCATION_SERVICE_NAME));
        int command = LocalForegroundServiceLocation.COMMAND_START_FOREGROUND_WITH_TYPE;
        fgsIntent.putExtras(LocalForegroundService.newCommand(new Binder(), command));
        context.startForegroundService(fgsIntent);
    }

    private void doStopForegroundService(Context context, Intent commandIntent,
            String serviceName) {
        String targetPackage = getTargetPackage(commandIntent);
        Intent fgsIntent = new Intent();
        fgsIntent.setComponent(new ComponentName(targetPackage, serviceName));
        context.stopService(fgsIntent);
    }

    private void doStartAlertService(Context context) {
        Intent intent = new Intent(context, LocalAlertService.class);
        intent.setAction(LocalAlertService.COMMAND_SHOW_ALERT);
        context.startService(intent);
    }

    private void doStopAlertService(Context context) {
        Intent intent = new Intent(context, LocalAlertService.class);
        intent.setAction(LocalAlertService.COMMAND_HIDE_ALERT);
        context.startService(intent);
    }

    private void doSelfInducedAnr(Context context) {
        ActivityManager am = context.getSystemService(ActivityManager.class);
        am.appNotResponding("CTS - self induced");
    }
    private void doStartActivity(Context context, Intent commandIntent) {
        String targetPackage = getTargetPackage(commandIntent);
        Intent activityIntent = new Intent(Intent.ACTION_MAIN);
        sActivityIntent.put(targetPackage, activityIntent);
        activityIntent.setComponent(new ComponentName(targetPackage, ACTIVITY_NAME));
        activityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(activityIntent);
    }

    private void doStopActivity(Context context, Intent commandIntent) {
        String targetPackage = getTargetPackage(commandIntent);
        Intent activityIntent = sActivityIntent.remove(targetPackage);
        activityIntent.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        activityIntent.putExtra("finish", true);
        context.startActivity(activityIntent);
    }

    private void doCreateFgslPendingIntent(Context context, Intent commandIntent) {
        final String targetPackage = getTargetPackage(commandIntent);
        final Intent intent = new Intent().setComponent(
                new ComponentName(targetPackage, FG_LOCATION_SERVICE_NAME));
        int command = LocalForegroundServiceLocation.COMMAND_START_FOREGROUND_WITH_TYPE;
        intent.putExtras(LocalForegroundService.newCommand(new Binder(), command));
        final PendingIntent pendingIntent = PendingIntent.getForegroundService(context, 0,
                intent, 0);
        sPendingIntent.put(targetPackage, pendingIntent);
    }

    private void doSendFgslPendingIntent(Context context, Intent commandIntent) {
        final String targetPackage = getTargetPackage(commandIntent);
        try {
            ((PendingIntent) sPendingIntent.remove(targetPackage)).send();
        } catch (PendingIntent.CanceledException e) {
            Log.e(TAG, "Caugtht exception:", e);
        }
    }

    private String getTargetPackage(Intent intent) {
        return intent.getStringExtra(EXTRA_TARGET_PACKAGE);
    }

    private int getFlags(Intent intent) {
        return intent.getIntExtra(EXTRA_FLAGS, 0);
    }

    public static void sendCommand(Context context, int command, String sourcePackage,
            String targetPackage, int flags, Bundle extras) {
        Intent intent = new Intent();
        if (command == COMMAND_BIND_SERVICE || command == COMMAND_START_FOREGROUND_SERVICE) {
            intent.setFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        }
        intent.setComponent(new ComponentName(sourcePackage, "android.app.stubs.CommandReceiver"));
        intent.putExtra(EXTRA_COMMAND, command);
        intent.putExtra(EXTRA_FLAGS, flags);
        intent.putExtra(EXTRA_TARGET_PACKAGE, targetPackage);
        if (extras != null) {
            intent.putExtras(extras);
        }
        sendCommand(context, intent);
    }

    private static void sendCommand(Context context, Intent intent) {
        Log.d(TAG, "Sending broadcast " + intent);
        context.sendOrderedBroadcast(intent, null);
    }

    private ServiceConnection addServiceConnection(final String packageName) {
        ServiceConnection connection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {
            }
        };
        sServiceMap.put(packageName, connection);
        return connection;
    }
}
