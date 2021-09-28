/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification.tests;

import static android.view.View.SYSTEM_UI_FLAG_FULLSCREEN;

import android.content.Context;
import android.os.Bundle;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.util.ArrayDeque;
import java.util.Queue;

/**
 * Test activity to retrieve frame ready time and buffer latch time.
 */
public class SurfaceFlingerTestActivity extends TestActivity {
    private final String LOG_TAG = "SurfaceFlingerTestActivity";

    private TestView mView;

    private Queue<Long> mReadyTimes = new ArrayDeque<>();
    private Queue<Long> mLatchTimes = new ArrayDeque<>();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mView = new TestView(this);
        mView.setSystemUiVisibility(SYSTEM_UI_FLAG_FULLSCREEN);
        setContentView(mView);
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    public Queue<Long> getReadyTimes() {
        return mReadyTimes;
    }

    public Queue<Long> getLatchTimes() {
        return mLatchTimes;
    }

    private class TestView extends SurfaceView implements Runnable {
        private volatile boolean mRunning = true;
        private Thread mThread;
        public TestView(Context context) {
            super(context);

            getHolder().addCallback(new SurfaceHolder.Callback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    mRunning = true;
                    mThread = new Thread(TestView.this);
                    mThread.start();
                }

                @Override
                public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                }

                @Override
                public void surfaceDestroyed(SurfaceHolder holder) {
                    try {
                        mRunning = false;
                        mThread.join();
                    } catch (InterruptedException e) {
                        throw new RuntimeException(e);
                    }
                }
            });
        }

        public TestView(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        public TestView(Context context, AttributeSet attrs, int defStyleAttr) {
            super(context, attrs, defStyleAttr);
        }

        public TestView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
            super(context, attrs, defStyleAttr, defStyleRes);
        }

        @Override
        public void run() {
            if (getHolder().getSurface().isValid()) {
                initDisplay(getHolder().getSurface());

                while (mRunning) {
                    drawFrame();
                    long[] frameData = getFrameData();
                    while (frameData != null) {
                        // Limit the number of frames to 240 to avoid infinitely growing buffer.
                        int MAX_FRAMES = 240;
                        if (getReadyTimes().size() == MAX_FRAMES) {
                            getReadyTimes().poll();
                            getLatchTimes().poll();
                        }
                        getReadyTimes().offer(frameData[0]);
                        getLatchTimes().offer(frameData[1]);
                        frameData = getFrameData();
                    }
                }
            }
        }
    }

    public native void initDisplay(Object surface);
    public native void drawFrame();
    public native long[] getFrameData();
}
