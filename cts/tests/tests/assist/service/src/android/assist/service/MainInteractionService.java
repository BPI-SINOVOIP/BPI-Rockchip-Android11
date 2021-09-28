/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.assist.service;

import static android.service.voice.VoiceInteractionSession.SHOW_WITH_ASSIST;
import static android.service.voice.VoiceInteractionSession.SHOW_WITH_SCREENSHOT;

import android.assist.common.AutoResetLatch;
import android.assist.common.Utils;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.RemoteCallback;
import android.service.voice.VoiceInteractionService;
import android.service.voice.VoiceInteractionSession;
import android.util.Log;

public class MainInteractionService extends VoiceInteractionService {
    static final String TAG = "MainInteractionService";
    private Intent mIntent;
    private boolean mReady = false;
    private BroadcastReceiver mBroadcastReceiver;
    private AutoResetLatch mResumeLatch = new AutoResetLatch();

    private RemoteCallback mRemoteCallback;

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "onCreate received");
    }

    @Override
    public void onReady() {
        super.onReady();
        Log.i(TAG, "onReady received");
        mReady = true;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i(TAG, "onStartCommand received - intent: " + intent);
        mIntent = intent;
        mRemoteCallback = mIntent.getParcelableExtra(Utils.EXTRA_REMOTE_CALLBACK);
        maybeStart();
        return START_NOT_STICKY;
    }

    protected void notify(String action) {
        if (mRemoteCallback == null) {
            throw new IllegalStateException("Test ran expecting " + action + " but did not register"
                    + "callback");
        } else {
            Bundle bundle = new Bundle();
            bundle.putString(Utils.EXTRA_REMOTE_CALLBACK_ACTION, action);
            mRemoteCallback.sendResult(bundle);
        }
    }

    private void maybeStart() {
        if (mIntent == null || !mReady) {
            Log.wtf(TAG, "Can't start session because either intent is null or onReady() "
                    + "has not been called yet. mIntent = " + mIntent + ", mReady = " + mReady);
        } else {
            if (isActiveService(this, new ComponentName(this, getClass()))) {
                if (mIntent.getBooleanExtra(Utils.EXTRA_REGISTER_RECEIVER, false)) {
                    if (mBroadcastReceiver == null) {
                        mBroadcastReceiver = new MainInteractionServiceBroadcastReceiver();
                        IntentFilter filter = new IntentFilter();
                        filter.addAction(Utils.BROADCAST_INTENT_START_ASSIST);
                        registerReceiver(mBroadcastReceiver, filter,
                                Context.RECEIVER_VISIBLE_TO_INSTANT_APPS);
                        Log.i(TAG, "Registered receiver to start session later");
                    }
                    notify(Utils.ASSIST_RECEIVER_REGISTERED);
                } else {
                    Log.i(TAG, "Yay! about to start session");
                    Bundle bundle = new Bundle();
                    bundle.putString(Utils.TESTCASE_TYPE,
                            mIntent.getStringExtra(Utils.TESTCASE_TYPE));
                    bundle.putParcelable(Utils.EXTRA_REMOTE_CALLBACK,
                            mIntent.getParcelableExtra(Utils.EXTRA_REMOTE_CALLBACK));
                    showSession(bundle, VoiceInteractionSession.SHOW_WITH_ASSIST |
                            VoiceInteractionSession.SHOW_WITH_SCREENSHOT);
                }
            } else {
                Log.wtf(TAG, "**** Not starting MainInteractionService because" +
                        " it is not set as the current voice interaction service");
            }
        }
    }

    private class MainInteractionServiceBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            Log.i(TAG, "Received broadcast to start session now: " + intent);
            String action = intent.getAction();
            if (action.equals(Utils.BROADCAST_INTENT_START_ASSIST)) {
                String testCaseName = intent.getStringExtra(Utils.TESTCASE_TYPE);
                Bundle extras = intent.getExtras();
                if (extras == null) {
                    extras = new Bundle();
                }

                extras.putString(Utils.TESTCASE_TYPE, mIntent.getStringExtra(Utils.TESTCASE_TYPE));
                extras.putParcelable(Utils.EXTRA_REMOTE_CALLBACK, mRemoteCallback);
                MainInteractionService.this.showSession(
                        extras, SHOW_WITH_ASSIST | SHOW_WITH_SCREENSHOT);
            } else {
                Log.e(TAG, "MainInteractionServiceBroadcastReceiver: invalid action " + action);
            }
        }
    }

    @Override
    public void onDestroy() {
        if (mBroadcastReceiver != null) {
            unregisterReceiver(mBroadcastReceiver);
        }
    }
}
