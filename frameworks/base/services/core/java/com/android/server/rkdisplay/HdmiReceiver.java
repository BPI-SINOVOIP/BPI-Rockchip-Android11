/*
 * Copyright (C) 2008 The Android Open Source Project
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
package com.android.server.rkdisplay;

import android.content.Context;
import android.content.Intent;
import android.content.BroadcastReceiver;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;

import java.io.*;
import android.os.SystemProperties;
import android.content.ContentResolver;
import android.os.Handler;
import android.view.View;
import android.view.WindowManager;
import java.util.Timer;
import java.util.TimerTask;
import java.util.Vector;

public class HdmiReceiver extends BroadcastReceiver{
    private static final String TAG = "HdmiReceiver";
    private final String HDMI_ACTION = "android.intent.action.HDMI_PLUGGED";
    private final String DP_ACTION = "android.intent.action.DP_PLUGGED";
    private final String ACTION_CHANGE = "android.display.action.change";
    private static Context mcontext;
    private static Timer mHdmiUpdateTimer=null;
    private RkDisplayModes mDisplayModes=null;
    private Vector<MyTask> mTaskVector = null;
    public boolean needWaitForTaskFinish = false;

    public HdmiReceiver(RkDisplayModes displayModes){
        mDisplayModes = displayModes;
        mHdmiUpdateTimer = new Timer();
        mTaskVector = new Vector<MyTask>();
    }

    @Override
    public void onReceive(Context context, Intent intent){
        mcontext = context;

        Log.d(TAG,"action ="+intent.getAction());
        boolean plugged = false;
        if (intent.getAction().equals(HDMI_ACTION) || intent.getAction().equals(ACTION_CHANGE) || intent.getAction().equals(DP_ACTION)){
            if (needWaitForTaskFinish == false) {
                needWaitForTaskFinish = true;
                mHdmiUpdateTimer.schedule(new MyTask(), 100);
            } else
                mTaskVector.add(new MyTask());
                Log.d(TAG,"onReceive mTaskVector.size() = "+mTaskVector.size());
        }
    }

    public void updateDisplayInfos(){
        mDisplayModes.updateDisplayInfos();
    }

    private class MyTask extends TimerTask{
        public void run(){
            synchronized (this) {
                updateDisplayInfos();
                if (mTaskVector.size() > 0){
                    Log.d(TAG,"run mTaskVector.size() = "+mTaskVector.size());
                    updateDisplayInfos();
                    mTaskVector.clear();
                }
                needWaitForTaskFinish = false;
           }
        }
	}
}

