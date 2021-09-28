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

package com.android.car.dummylauncher;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Trace;
import android.os.UserHandle;
import android.util.Log;
import android.view.View;
import android.widget.TextView;

/**
 * A dummy launcher for CTS.
 *
 * Current CarLauncher invokes Maps Activity in its ActivityView and it creates unexpected events
 * for CTS and they make CTS fail.
 */
public class LauncherActivity extends Activity {
    private static final String TAG = "DummyLauncher";

    private int mUserId = UserHandle.USER_NULL;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        mUserId = UserHandle.myUserId();
        Log.i(TAG, "pre.onCreate(): userId=" + mUserId);
        super.onCreate(savedInstanceState);
        Log.i(TAG, "post.onCreate(): userId=" + mUserId);
        View view = getLayoutInflater().inflate(R.layout.launcher_activity, null);
        setContentView(view);

        TextView message = findViewById(R.id.message);
        message.setText(message.getText() + "\n\nI am user " + mUserId);
        reportFullyDrawn();
        Log.i(TAG, "done.onCreate(): userId=" + mUserId);
    }

    @Override
    protected void onResume() {
        Trace.traceBegin(Trace.TRACE_TAG_APP, "onResume-" + mUserId);
        Log.i(TAG, "pre.onResume(): userId=" + mUserId);
        super.onResume();
        Log.i(TAG, "post.onResume(): userId=" + mUserId);
        Trace.traceEnd(Trace.TRACE_TAG_APP);
    }

    @Override
    protected void onPostResume() {
        Trace.traceBegin(Trace.TRACE_TAG_APP, "onPostResume-" + mUserId);
        Log.i(TAG, "pre.onPostResume(): userId=" + mUserId);
        super.onPostResume();
        Log.i(TAG, "post.onPostResume(): userId=" + mUserId);
        Trace.traceEnd(Trace.TRACE_TAG_APP);
    }

    @Override
    protected void onRestart() {
        Log.i(TAG, "pre.onRestart(): userId=" + mUserId);
        super.onRestart();
        Log.i(TAG, "post.onRestart(): userId=" + mUserId);
    }

    @Override
    public void onActivityReenter(int resultCode, Intent data) {
        Log.i(TAG, "pre.onActivityReenter(): userId=" + mUserId);
        super.onActivityReenter(resultCode, data);
        Log.i(TAG, "post.onActivityReenter(): userId=" + mUserId);
    }

    @Override
    public void onEnterAnimationComplete() {
        Log.i(TAG, "pre.onEnterAnimationComplete(): userId=" + mUserId);
        super.onEnterAnimationComplete();
        Log.i(TAG, "post.onEnterAnimationComplete(): userId=" + mUserId);
    }

    @Override
    protected void onStop() {
        Log.i(TAG, "pre.onStop(): userId=" + mUserId);
        super.onStop();
        Log.i(TAG, "post.onStop(): userId=" + mUserId);
    }

    @Override
    protected void onDestroy() {
        Log.i(TAG, "pre.onDestroy(): userId=" + mUserId);
        super.onDestroy();
        Log.i(TAG, "post.onDestroy(): userId=" + mUserId);
    }
}

