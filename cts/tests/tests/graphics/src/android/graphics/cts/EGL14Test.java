/*
 * Copyright 2017 The Android Open Source Project
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

package android.graphics.cts;

import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLSurface;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

@SmallTest
@RunWith(BlockJUnit4ClassRunner.class)
public class EGL14Test {

    static {
        System.loadLibrary("ctsgraphics_jni");
    }

    private static final String TAG = EGL14Test.class.getSimpleName();
    private static final boolean DEBUG = false;

    private EGLDisplay mEglDisplay = EGL14.EGL_NO_DISPLAY;
    private EGLConfig mEglConfig = null;
    private EGLSurface mEglPbuffer = EGL14.EGL_NO_SURFACE;
    private EGLContext mEglContext = EGL14.EGL_NO_CONTEXT;
    private int mEglVersion = 0;

    @Before
    public void setup() throws Throwable {
        int error;

        mEglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
        if (mEglDisplay == EGL14.EGL_NO_DISPLAY) {
            throw new RuntimeException("no EGL display");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglGetPlatformDisplay failed");
        }

        int[] major = new int[1];
        int[] minor = new int[1];
        if (!EGL14.eglInitialize(mEglDisplay, major, 0, minor, 0)) {
            throw new RuntimeException("error in eglInitialize");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglInitialize failed");
        }
        mEglVersion = major[0] * 10 + minor[0];

        // If we could rely on having EGL_KHR_surfaceless_context and EGL_KHR_context_no_config, we
        // wouldn't have to create a config or pbuffer at all.

        int[] numConfigs = new int[1];
        EGLConfig[] configs = new EGLConfig[1];
        if (!EGL14.eglChooseConfig(mEglDisplay,
                new int[] {
                    EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
                    EGL14.EGL_SURFACE_TYPE, EGL14.EGL_PBUFFER_BIT,
                    EGL14.EGL_NONE},
                0, configs, 0, 1, numConfigs, 0)) {
            throw new RuntimeException("eglChooseConfig failed");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglChooseConfig failed");
        }

        mEglConfig = configs[0];

        mEglPbuffer = EGL14.eglCreatePbufferSurface(mEglDisplay, mEglConfig,
                new int[] {EGL14.EGL_WIDTH, 1, EGL14.EGL_HEIGHT, 1, EGL14.EGL_NONE}, 0);
        if (mEglPbuffer == EGL14.EGL_NO_SURFACE) {
            throw new RuntimeException("eglCreatePbufferSurface failed");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglCreatePbufferSurface failed");
        }


        mEglContext = EGL14.eglCreateContext(mEglDisplay, mEglConfig, EGL14.EGL_NO_CONTEXT,
                new int[] {EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE}, 0);
        if (mEglContext == EGL14.EGL_NO_CONTEXT) {
            throw new RuntimeException("eglCreateContext failed");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglCreateContext failed");
        }


        if (!EGL14.eglMakeCurrent(mEglDisplay, mEglPbuffer, mEglPbuffer, mEglContext)) {
            throw new RuntimeException("eglMakeCurrent failed");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglMakeCurrent failed");
        }
    }

    @After
    public void teardown() throws Throwable {
        int error;

        if (!EGL14.eglMakeCurrent(mEglDisplay, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE,
                    EGL14.EGL_NO_CONTEXT)) {
            throw new RuntimeException("eglMakeCurrent failed");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglMakeCurrent failed");
        }
        if (!EGL14.eglDestroySurface(mEglDisplay, mEglPbuffer)) {
            throw new RuntimeException("eglDestroySurface failed");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglDestroySurface failed");
        }
        if (!EGL14.eglDestroyContext(mEglDisplay, mEglContext)) {
            throw new RuntimeException("eglDestroyContext failed");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglDestroyContext failed");
        }
        EGL14.eglTerminate(mEglDisplay);
    }

    @Test
    public void testEGL14CreatePixmap() {

        int[] attribs = new int[] { EGL14.EGL_NONE };
        boolean unsupported = false;
        try {
            EGLSurface surface = EGL14.eglCreatePixmapSurface(mEglDisplay,
                    mEglConfig, 0, attribs, 0);
        } catch (UnsupportedOperationException e) {
            unsupported = true;
        }
        if (!unsupported) {
            throw new RuntimeException("eglCreatePixmapSurface is not supported on Android,"
                    + " why did call not report that!");
        }
    }
}

// Note: Need to add tests for eglCreatePlatformWindowSurface
