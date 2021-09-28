/*
 * Copyright (C) 2011 The Android Open Source Project
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

package android.opengl.cts;

import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;

/**
 * {@link Activity} with a {@link GLSurfaceView} that chooses a specific configuration.
 */
public class EglConfigCtsActivity extends Activity {

    private final String TAG = this.getClass().getSimpleName();

    public static final String CONFIG_ID_EXTRA = "eglConfigId";

    public static final String CONTEXT_CLIENT_VERSION_EXTRA = "eglContextClientVersion";

    private static final int EGL_OPENGL_ES_BIT = 0x1;
    private static final int EGL_OPENGL_ES2_BIT = 0x4;

    private EglConfigGLSurfaceView mView;

    private CountDownLatch mFinishedDrawing;
    private CountDownLatch mFinishedTesting;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Dismiss keyguard and keep screen on while this test is on.
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD |
                WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON |
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        int[] configIds = getEglConfigIds(EGL_OPENGL_ES_BIT);
        int[] configIds2 = getEglConfigIds(EGL_OPENGL_ES2_BIT);
        assertTrue(configIds.length + configIds2.length > 0);
        mFinishedTesting = new CountDownLatch(configIds.length + configIds2.length);
        try {
            runConfigTests(configIds, 1);
            runConfigTests(configIds2, 2);
        } catch (InterruptedException e) {
            Log.e(TAG, "Caught exception");
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (mView != null)
        {
            mView.onResume();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (mView != null)
        {
            mView.onPause();
        }
    }

    private void runConfigTests(int[] configIds, int contextClientVersion)
            throws InterruptedException {
        Context context = this;
        Thread thread = new Thread() {
            @Override
            public void run() {
                for (int configId : configIds) {
                    mFinishedDrawing = new CountDownLatch(1);

                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            setTitle("EGL Config Id: " + configId + " Client Version: " + contextClientVersion);
                            mView = new EglConfigGLSurfaceView(context, configId, contextClientVersion, new Runnable() {
                                @Override
                                public void run() {
                                    mFinishedDrawing.countDown();
                                }
                            });
                            setContentView(mView);
                        }
                    });

                    try {
                        waitToFinishDrawing();
                    } catch (Exception e) {
                        Log.e(TAG, "Timed out!");
                    }

                    mFinishedTesting.countDown();
                }
            }
        };
        thread.start();
    }

    private void waitToFinishDrawing() throws InterruptedException {
        if (!mFinishedDrawing.await(5, TimeUnit.SECONDS)) {
            throw new IllegalStateException("Couldn't finish drawing frames!");
        }
    }

    void waitToFinishTesting() throws InterruptedException {
        if (!mFinishedTesting.await(300, TimeUnit.SECONDS)) {
            throw new IllegalStateException("Couldn't finish testing!");
        }
    }

    private static int[] getEglConfigIds(int renderableType) {
        EGL10 egl = (EGL10) EGLContext.getEGL();
        EGLDisplay display = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
        int[] numConfigs = new int[1];

        int[] attributeList = new int[] {
                EGL10.EGL_RENDERABLE_TYPE, renderableType,

                // Avoid configs like RGBA0008 which crash even though they have the window bit set.
                EGL10.EGL_RED_SIZE, 1,
                EGL10.EGL_GREEN_SIZE, 1,
                EGL10.EGL_BLUE_SIZE, 1,

                EGL10.EGL_SURFACE_TYPE, EGL10.EGL_WINDOW_BIT,
                EGL10.EGL_NONE
        };

        if (egl.eglInitialize(display, null)) {
            try {
                if (egl.eglChooseConfig(display, attributeList, null, 0, numConfigs)) {
                    EGLConfig[] configs = new EGLConfig[numConfigs[0]];
                    if (egl.eglChooseConfig(display, attributeList, configs, configs.length,
                            numConfigs)) {
                        int[] configIds = new int[numConfigs[0]];
                        for (int i = 0; i < numConfigs[0]; i++) {
                            int[] value = new int[1];
                            if (egl.eglGetConfigAttrib(display, configs[i], EGL10.EGL_CONFIG_ID,
                                    value)) {
                                configIds[i] = value[0];
                            } else {
                                throw new IllegalStateException("Couldn't call eglGetConfigAttrib");
                            }
                        }
                        return configIds;
                    } else {
                        throw new IllegalStateException("Couldn't call eglChooseConfig (1)");
                    }
                } else {
                    throw new IllegalStateException("Couldn't call eglChooseConfig (2)");
                }
            } finally {
                egl.eglTerminate(display);
            }
        } else {
            throw new IllegalStateException("Couldn't initialize EGL.");
        }
    }
}
