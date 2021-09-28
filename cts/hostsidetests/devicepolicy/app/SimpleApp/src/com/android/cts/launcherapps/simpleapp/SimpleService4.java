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

package com.android.cts.launcherapps.simpleapp;

import android.app.ActivityManager;
import android.app.Service;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.Process;
import android.os.RemoteException;
import android.system.OsConstants;
import android.util.Log;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;

/**
 * A simple service which accepts various test command.
 */
public class SimpleService4 extends Service {
    private static final String TAG = SimpleService4.class.getSimpleName();

    private static final String EXIT_ACTION =
            "com.android.cts.launchertests.simpleapp.EXIT_ACTION";
    private static final String EXTRA_ACTION = "action";
    private static final String EXTRA_MESSENGER = "messenger";
    private static final String EXTRA_PROCESS_NAME = "process";
    private static final String EXTRA_COOKIE = "cookie";
    private static final String STUB_PROVIDER_AUTHORITY =
            "com.android.cts.launcherapps.simpleapp.provider";

    private static final int ACTION_NONE = 0;
    private static final int ACTION_FINISH = 1;
    private static final int ACTION_EXIT = 2;
    private static final int ACTION_ANR = 3;
    private static final int ACTION_NATIVE_CRASH = 4;
    private static final int ACTION_KILL = 5;
    private static final int ACTION_ACQUIRE_STABLE_PROVIDER = 6;
    private static final int ACTION_KILL_PROVIDER = 7;
    private static final int CRASH_SIGNAL = OsConstants.SIGSEGV;

    static final String METHOD_EXIT = "exit";
    static final int EXIT_CODE = 123;

    private static final int WAITFOR_SETTLE_DOWN = 2000;

    private static final int CMD_PID = 1;
    private Handler mHandler;
    private ContentProviderClient mProviderClient;

    @Override
    public void onCreate() {
        mHandler = new Handler(Looper.getMainLooper());
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        sendPidBack(intent);
        setCookieIfNeeded(intent);
        // perform the action after return from here,
        // make a delay, otherwise the system might try to restart the service
        // if the process dies before the system realize it's asking for START_NOT_STICKY.
        mHandler.postDelayed(() -> doAction(intent), WAITFOR_SETTLE_DOWN);
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    protected String getProcessName() {
        try (BufferedReader reader = new BufferedReader(new FileReader("/proc/self/cmdline"))) {
            return reader.readLine().trim();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private void sendPidBack(Intent intent) {
        Messenger messenger = intent.getParcelableExtra(EXTRA_MESSENGER);
        Message msg = Message.obtain();
        msg.what = CMD_PID;
        msg.arg1 = Process.myPid();
        msg.arg2 = Process.myUid();
        Bundle b = new Bundle();
        b.putString(EXTRA_PROCESS_NAME, getProcessName());
        msg.obj = b;
        try {
            messenger.send(msg);
        } catch (RemoteException e) {
        }
    }

    private void doAction(Intent intent) {
        if (EXIT_ACTION.equals(intent.getAction())) {
            int action = intent.getIntExtra(EXTRA_ACTION, ACTION_NONE);
            switch (action) {
                case ACTION_FINISH:
                    stopSelf();
                    break;
                case ACTION_EXIT:
                    System.exit(EXIT_CODE);
                    break; // Shoudln't reachable
                case ACTION_ANR:
                    try {
                        Thread.sleep(3600*1000);
                    } catch (InterruptedException e) {
                    }
                    break;
                case ACTION_NATIVE_CRASH:
                    Process.sendSignal(Process.myPid(), CRASH_SIGNAL);
                    break; // Shoudln't reachable
                case ACTION_KILL:
                    Process.sendSignal(Process.myPid(), OsConstants.SIGKILL);
                    break; // Shoudln't reachable
                case ACTION_ACQUIRE_STABLE_PROVIDER:
                    ContentResolver cr = getContentResolver();
                    mProviderClient = cr.acquireContentProviderClient(
                            STUB_PROVIDER_AUTHORITY);
                    break;
                case ACTION_KILL_PROVIDER:
                    try {
                        mProviderClient.call(METHOD_EXIT, null, null);
                    } catch (RemoteException e) {
                    }
                    break;
                case ACTION_NONE:
                default:
                    break;
            }
        }
    }

    private void setCookieIfNeeded(final Intent intent) {
        if (intent.hasExtra(EXTRA_COOKIE)) {
            byte[] cookie = intent.getByteArrayExtra(EXTRA_COOKIE);
            ActivityManager am = getSystemService(ActivityManager.class);
            am.setProcessStateSummary(cookie);
        }
    }
}
