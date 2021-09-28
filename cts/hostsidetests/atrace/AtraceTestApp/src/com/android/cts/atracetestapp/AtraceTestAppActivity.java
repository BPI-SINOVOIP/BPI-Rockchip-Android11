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
package com.android.cts.atracetestapp;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Trace;
import android.view.View;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class AtraceTestAppActivity extends Activity {

    private CountDownLatch mHasDrawnFence;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Trace.beginAsyncSection("AtraceActivity::created", 1);
        super.onCreate(savedInstanceState);
        mHasDrawnFence = new CountDownLatch(1);
        MyView view = new MyView(this);
        setContentView(view);
        view.getViewTreeObserver().registerFrameCommitCallback(mHasDrawnFence::countDown);
    }

    @Override
    protected void onStart() {
        Trace.beginAsyncSection("AtraceActivity::started", 1);
        super.onStart();
    }

    @Override
    protected void onResume() {
        Trace.beginAsyncSection("AtraceActivity::resumed", 1);
        super.onResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
        Trace.endAsyncSection("AtraceActivity::resumed", 1);
    }

    @Override
    protected void onStop() {
        super.onStop();
        Trace.endAsyncSection("AtraceActivity::started", 1);
    }

    @Override
    protected void onDestroy() {
        mHasDrawnFence = null;
        super.onDestroy();
        Trace.endAsyncSection("AtraceActivity::created", 1);
    }

    public void waitForDraw() {
        try {
            assertTrue(mHasDrawnFence.await(10, TimeUnit.SECONDS));
        } catch (InterruptedException e) {
            fail("Timed out: " + e.getMessage());
        }
    }

    private static class MyView extends View {
        private static int sDrawCount = 0;

        public MyView(Context context) {
            super(context);
            setWillNotDraw(false);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            Trace.beginSection("MyView::onDraw");
            Trace.setCounter("MyView::drawCount", ++sDrawCount);
            canvas.drawColor(Color.BLUE);
            Trace.endSection();
        }
    }
}
