/*
 * Copyright (C) 2018 The Android Open Source Project
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

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.SurfaceView;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.Surface;

import java.util.ArrayList;

public class ChoreoTestActivity extends TestActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ChoreoTestView sv = new ChoreoTestView(this);
        setContentView(sv);
    }

    @Override
    protected void onStart() {
        super.onStart();
    }

    private class ChoreoTestView extends SurfaceView implements Runnable {
        private Thread mThread;

        public ChoreoTestView(Context context) {
            super(context);

            getHolder().addCallback(new SurfaceHolder.Callback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    startTheTest();
                    mThread = new Thread(ChoreoTestView.this);
                    mThread.start();
                }

                @Override
                public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}

                @Override
                public void surfaceDestroyed(SurfaceHolder holder) {
                    stopTheTest();
                    try {
                        if(mThread != null) {
                            mThread.join();
                        }
                    } catch (InterruptedException e) {
                        throw new RuntimeException(e);
                    }
                }
            });
        }

        public ChoreoTestView(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        public ChoreoTestView(Context context, AttributeSet attrs, int defStyleAttr) {
            super(context, attrs, defStyleAttr);
        }

        public ChoreoTestView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
            super(context, attrs, defStyleAttr, defStyleRes);
        }

        @Override
        public void run() {
            runTheTest(getHolder().getSurface());
        }
    }

    public native boolean startTheTest();
    public native boolean runTheTest(Object theSurface);
    public native void stopTheTest();
    public native ArrayList<Long> getFrameIntervals();
}
