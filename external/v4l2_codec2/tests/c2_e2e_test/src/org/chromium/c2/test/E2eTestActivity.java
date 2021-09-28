/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

package org.chromium.c2.test;

import android.app.Activity;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.util.Size;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.LinearLayout;

import java.util.concurrent.CountDownLatch;

/** Activity responsible for running the native Codec2.0 E2E tests. */
public class E2eTestActivity extends Activity implements SurfaceHolder.Callback {

    public final String TAG = "E2eTestActivity";

    private SurfaceView mSurfaceView;
    private Size mSize;

    private boolean mSurfaceCreated = false;
    private boolean mCanStartTest = false;
    private Size mExpectedSize;
    private CountDownLatch mLatch;

    private long mDecoderPtr;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        System.loadLibrary("codectest");

        setContentView(R.layout.main_activity);
        mSurfaceView = (SurfaceView) findViewById(R.id.surface);

        mSurfaceView.getHolder().addCallback(this);

        mCanStartTest = !getIntent().getBooleanExtra("delay-start", false);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // gtest can't reuse a process
        System.exit(0);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        mSurfaceCreated = true;
        maybeStartTest();
    }

    private void maybeStartTest() {
        if (!mSurfaceCreated || !mCanStartTest) {
            return;
        }
        boolean encode = getIntent().getBooleanExtra("do-encode", false);
        String[] testArgs =
                getIntent().getStringArrayExtra("test-args") != null
                        ? getIntent().getStringArrayExtra("test-args")
                        : new String[0];
        String logFile = getIntent().getStringExtra("log-file");

        AsyncTask.execute(
                new Runnable() {
                    @Override
                    public void run() {
                        int res =
                                c2VideoTest(
                                        encode,
                                        testArgs,
                                        testArgs.length,
                                        mSurfaceView.getHolder().getSurface(),
                                        logFile);
                        Log.i(TAG, "Test returned result code " + res);

                        new Handler(Looper.getMainLooper())
                                .post(
                                        new Runnable() {
                                            @Override
                                            public void run() {
                                                finish();
                                            }
                                        });
                    }
                });
    }

    void onDecoderReady(long decoderPtr) {
        synchronized (this) {
            mDecoderPtr = decoderPtr;
        }
    }

    @Override
    public void onNewIntent(Intent intent) {
        if (intent.getAction().equals("org.chromium.c2.test.START_TEST")) {
            mCanStartTest = true;
            maybeStartTest();
            return;
        }

        synchronized (this) {
            if (mDecoderPtr != 0) {
                stopDecoderLoop(mDecoderPtr);
            }
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        synchronized (this) {
            mSize = new Size(width, height);
            if (mLatch != null && mExpectedSize.equals(mSize)) {
                mLatch.countDown();
            }
        }
    }

    // Configures the SurfaceView with the dimensions of the video the test is currently playing.
    void onSizeChanged(int width, int height) {
        synchronized (this) {
            mExpectedSize = new Size(width, height);
            if (mSize != null && mSize.equals(mExpectedSize)) {
                return;
            }
            mLatch = new CountDownLatch(1);
        }

        new Handler(Looper.getMainLooper())
                .post(
                        new Runnable() {
                            @Override
                            public void run() {
                                mSurfaceView.setLayoutParams(
                                        new LinearLayout.LayoutParams(width, height));
                            }
                        });

        try {
            mLatch.await();
        } catch (Exception e) {
        }
        synchronized (this) {
            mLatch = null;
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (!isFinishing()) {
            throw new RuntimeException("Surface destroyed during test");
        }
    }

    public native int c2VideoTest(
            boolean encode, String[] testArgs, int testArgsCount, Surface surface, String tmpFile);

    public native void stopDecoderLoop(long decoderPtr);
}
