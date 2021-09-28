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

import static org.junit.Assert.assertEquals;

//import static android.opengl.EGL14.*;
//import static android.opengl.EGL15.*;
import android.opengl.EGL14;
import android.opengl.EGL15;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLImage;
import android.opengl.EGLSurface;
import android.opengl.EGLSync;
import android.opengl.GLES20;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

@SmallTest
@RunWith(BlockJUnit4ClassRunner.class)
public class EGL15Test {

    static {
        System.loadLibrary("ctsgraphics_jni");
    }

    private static final String TAG = EGL15Test.class.getSimpleName();
    private static final boolean DEBUG = false;

    private EGLDisplay mEglDisplay = EGL15.EGL_NO_DISPLAY;
    private EGLConfig mEglConfig = null;
    private EGLSurface mEglPbuffer = EGL15.EGL_NO_SURFACE;
    private EGLContext mEglContext = EGL15.EGL_NO_CONTEXT;
    private int mEglVersion = 0;

    @Before
    public void setup() throws Throwable {
        int error;

        mEglDisplay = EGL15.eglGetPlatformDisplay(EGL15.EGL_PLATFORM_ANDROID_KHR,
                EGL14.EGL_DEFAULT_DISPLAY,
                new long[] {
                    EGL14.EGL_NONE },
                0);
        if (mEglDisplay == EGL15.EGL_NO_DISPLAY) {
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
        if (mEglPbuffer == EGL15.EGL_NO_SURFACE) {
            throw new RuntimeException("eglCreatePbufferSurface failed");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglCreatePbufferSurface failed");
        }


        mEglContext = EGL14.eglCreateContext(mEglDisplay, mEglConfig, EGL14.EGL_NO_CONTEXT,
                new int[] {EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE}, 0);
        if (mEglContext == EGL15.EGL_NO_CONTEXT) {
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
        EGL14.eglTerminate(mEglDisplay);
    }

    @Test
    public void testEGL15SyncFence() {
        if (mEglVersion < 15) {
            return;
        }
        EGLSync sync = EGL15.eglCreateSync(mEglDisplay, EGL15.EGL_SYNC_FENCE,
                new long[] {
                    EGL14.EGL_NONE },
                0);
        if (sync == EGL15.EGL_NO_SYNC) {
            throw new RuntimeException("eglCreateSync failed");
        }
        int error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglCreateSync failed");
        }

        GLES20.glFlush();
        error = GLES20.glGetError();
        if (error != GLES20.GL_NO_ERROR) {
            throw new RuntimeException("glFlush failed");
        }

        int status = EGL15.eglClientWaitSync(mEglDisplay, sync, 0, EGL15.EGL_FOREVER);
        if (status != EGL15.EGL_CONDITION_SATISFIED) {
            throw new RuntimeException("eglClientWaitSync failed");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglClientWaitSync failed");
        }

        if (!EGL15.eglDestroySync(mEglDisplay, sync)) {
            throw new RuntimeException("eglDestroySync failed");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglDestroySync failed");
        }
    }

    @Test
    public void testEGL15GetSyncType() {
        if (mEglVersion < 15) {
            return;
        }
        EGLSync sync = EGL15.eglCreateSync(mEglDisplay, EGL15.EGL_SYNC_FENCE,
                new long[] {
                    EGL14.EGL_NONE },
                0);
        if (sync == EGL15.EGL_NO_SYNC) {
            throw new RuntimeException("eglCreateSync failed");
        }
        int error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglCreateSync failed");
        }

        long[] type = new long[1];
        boolean success = EGL15.eglGetSyncAttrib(mEglDisplay, sync, EGL15.EGL_SYNC_TYPE, type, 0);
        if (!success) {
            throw new RuntimeException("eglGetSyncAttrib failed (returned false)");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglGetSyncAttrib failed");
        }
        assertEquals(type[0], EGL15.EGL_SYNC_FENCE);

        if (!EGL15.eglDestroySync(mEglDisplay, sync)) {
            throw new RuntimeException("eglDestroySync failed");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglDestroySync failed");
        }
    }

    @Test
    public void testEGL15WaitSync() {
        if (mEglVersion < 15) {
            return;
        }
        EGLSync sync = EGL15.eglCreateSync(mEglDisplay, EGL15.EGL_SYNC_FENCE,
                new long[] {
                    EGL14.EGL_NONE },
                0);
        if (sync == EGL15.EGL_NO_SYNC) {
            throw new RuntimeException("eglCreateSync failed");
        }
        int error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglCreateSync failed");
        }

        boolean success = EGL15.eglWaitSync(mEglDisplay, sync, 0);
        if (!success) {
            throw new RuntimeException("eglWaitSync failed (returned false)");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglWaitSync failed");
        }

        if (!EGL15.eglDestroySync(mEglDisplay, sync)) {
            throw new RuntimeException("eglDestroySync failed");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglDestroySync failed");
        }
    }

    @Test
    public void testEGL15CreateImage() {
        if (mEglVersion < 15) {
            return;
        }

        int error;
        int srcTex = 1;
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, srcTex);
        error = GLES20.glGetError();
        if (error != GLES20.GL_NO_ERROR) {
            throw new RuntimeException("glBindTexture failed");
        }

        GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA, 64, 64, 0,
                            GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, null);
        error = GLES20.glGetError();
        if (error != GLES20.GL_NO_ERROR) {
            throw new RuntimeException("glTexImage2D failed");
        }

        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER,
                               GLES20.GL_LINEAR);
        error = GLES20.glGetError();
        if (error != GLES20.GL_NO_ERROR) {
            throw new RuntimeException("glTexParameteri failed");
        }

        long[] attribs = new long[] {
                EGL15.EGL_GL_TEXTURE_LEVEL, 0,
                EGL14.EGL_NONE };
        EGLImage image = EGL15.eglCreateImage(mEglDisplay, mEglContext,
                EGL15.EGL_GL_TEXTURE_2D, srcTex, attribs, 0);
        if (image == EGL15.EGL_NO_IMAGE) {
            throw new RuntimeException("eglCreateImage failed, got EGL_NO_IMAGE");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglCreateImage failed");
        }

        boolean success = EGL15.eglDestroyImage(mEglDisplay, image);
        if (!success) {
            throw new RuntimeException("eglDestroyImage failed (returned false)");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglDestroyImage failed");
        }
    }

    @Test
    public void testEGL15CreatePlatformPixmap() {
        if (mEglVersion < 15) {
            return;
        }

        long[] attribs = new long[] { EGL14.EGL_NONE };
        boolean unsupported = false;
        try {
            EGLSurface surface = EGL15.eglCreatePlatformPixmapSurface(mEglDisplay,
                    mEglConfig, null, attribs, 0);
        } catch (UnsupportedOperationException e) {
            unsupported = true;
        }
        if (!unsupported) {
            throw new RuntimeException("eglCreatePlatformPixmapSurface is not supported on Android,"
                    + " why did call not report that!");
        }
    }

    @Test
    public void testEGL15CreateDebugContext() {
        int error;

        if (mEglVersion < 15) {
            return;
        }

        mEglContext = EGL14.eglCreateContext(mEglDisplay, mEglConfig, EGL14.EGL_NO_CONTEXT,
                new int[] { EGL15.EGL_CONTEXT_OPENGL_DEBUG, EGL14.EGL_FALSE, EGL14.EGL_NONE }, 0);
        if (mEglContext == EGL15.EGL_NO_CONTEXT) {
            throw new RuntimeException("eglCreateContext failed");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglCreateContext failed");
        }
        EGL14.eglDestroyContext(mEglDisplay, mEglContext);

        mEglContext = EGL14.eglCreateContext(mEglDisplay, mEglConfig, EGL14.EGL_NO_CONTEXT,
                new int[] { EGL15.EGL_CONTEXT_OPENGL_DEBUG, EGL14.EGL_TRUE, EGL14.EGL_NONE }, 0);
        if (mEglContext == EGL15.EGL_NO_CONTEXT) {
            throw new RuntimeException("eglCreateContext failed");
        }
        error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            throw new RuntimeException("eglCreateContext failed");
        }
        EGL14.eglDestroyContext(mEglDisplay, mEglContext);
    }
}

// Note: Need to add tests for eglCreatePlatformWindowSurface
