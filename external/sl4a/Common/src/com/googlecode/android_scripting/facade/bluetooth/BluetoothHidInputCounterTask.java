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

package com.googlecode.android_scripting.facade.bluetooth;

import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import com.googlecode.android_scripting.Log;
import com.googlecode.android_scripting.facade.ui.ViewInflater;
import com.googlecode.android_scripting.future.FutureActivityTask;

import org.xmlpull.v1.XmlPullParser;

import java.io.StringReader;
import java.util.concurrent.CountDownLatch;

public class BluetoothHidInputCounterTask extends FutureActivityTask<Object> {
    private static final String BLANKLAYOUT = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
            + "<LinearLayout xmlns:android=\"http://schemas.android.com/apk/res/android\""
            + "android:id=\"@+id/background\" android:orientation=\"vertical\""
            + "android:layout_width=\"match_parent\" android:layout_height=\"match_parent\""
            + "android:background=\"#ff000000\">"
            + "</LinearLayout>";
    private static final String TITLE = "Blank";

    protected final CountDownLatch mShowLatch = new CountDownLatch(1);

    private long mFirstTime = 0;
    private long mLastTime = 0;
    private int mInputCount = 0;

    public BluetoothHidInputCounterTask() {
        super();
    }

    @Override
    public void onCreate() {
        ViewInflater inflater = new ViewInflater();
        inflater.getErrors().clear();
        try {
            StringReader sr = new StringReader(BLANKLAYOUT);
            XmlPullParser xml = ViewInflater.getXml(sr);
            View view = inflater.inflate(getActivity(), xml);
            getActivity().setContentView(view);
        } catch (Exception e) {
            Log.e("fail with exception " + e);
        }
        getActivity().setTitle(TITLE);
        mShowLatch.countDown();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
    }

    public CountDownLatch getShowLatch() {
        return mShowLatch;
    }

    public int getCount() {
        return mInputCount;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        mInputCount++;
        return false;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        mInputCount++;
        return false;
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        mInputCount++;
        return false;
    }
}
