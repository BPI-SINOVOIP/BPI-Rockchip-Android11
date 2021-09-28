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

package com.android.angleIntegrationTest.common;

import static javax.microedition.khronos.egl.EGL10.EGL_STENCIL_SIZE;

import android.annotation.TargetApi;
import android.opengl.EGL14;
import android.opengl.GLES20;
import android.os.Build.VERSION_CODES;
import android.util.Log;
import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;

/**
 * GLES View Class to get access to GLES20.glGetString()
 */

@TargetApi(VERSION_CODES.GINGERBREAD)
public class GlesView {

    private static final EGL10 EGL = (EGL10) EGLContext.getEGL();
    private String mRenderer = "";

    private final String TAG = this.getClass().getSimpleName();

    public GlesView() {
        createEGL();
    }

    @TargetApi(VERSION_CODES.JELLY_BEAN_MR1)
    private void createEGL() {
        int[] version = new int[2];
        EGLDisplay display = EGL.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
        EGL.eglInitialize(display, version);

        int[] numConfigs = new int[1];
        EGL.eglGetConfigs(display, null, 0, numConfigs);
        EGLConfig[] allConfigs = new EGLConfig[numConfigs[0]];
        EGL.eglGetConfigs(display, allConfigs, numConfigs[0], numConfigs);

        int[] configAttrib =
            new int[]{
                EGL10.EGL_RENDERABLE_TYPE,
                EGL14.EGL_OPENGL_ES2_BIT,
                EGL10.EGL_SURFACE_TYPE,
                EGL10.EGL_WINDOW_BIT,
                EGL10.EGL_RED_SIZE,
                8,
                EGL10.EGL_GREEN_SIZE,
                8,
                EGL10.EGL_BLUE_SIZE,
                8,
                EGL10.EGL_ALPHA_SIZE,
                8,
                EGL10.EGL_DEPTH_SIZE,
                16,
                EGL_STENCIL_SIZE,
                0,
                EGL10.EGL_NONE
            };

        EGLConfig[] selectedConfig = new EGLConfig[1];
        EGL.eglChooseConfig(display, configAttrib, selectedConfig, 1, numConfigs);
        EGLConfig eglConfig;
        if (selectedConfig[0] != null) {
            eglConfig = selectedConfig[0];
            Log.i(TAG, "Found matching EGL config");
        } else {
            Log.e(TAG, "Could not find matching EGL config");
            throw new RuntimeException("No Matching EGL Config Found");
        }

        EGLSurface surface = EGL.eglCreatePbufferSurface(display, eglConfig, null);
        int[] contextAttribs = new int[]{EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL10.EGL_NONE};
        EGLContext context = EGL
            .eglCreateContext(display, eglConfig, EGL10.EGL_NO_CONTEXT, contextAttribs);
        EGL.eglMakeCurrent(display, surface, surface, context);

        Log.i(TAG, "CTS ANGLE Test :: GLES GL_VENDOR   : " + GLES20.glGetString(GLES20.GL_VENDOR));
        Log.i(TAG, "CTS ANGLE Test :: GLES GL_VERSION  : " + GLES20.glGetString(GLES20.GL_VERSION));
        Log.i(TAG,
            "CTS ANGLE Test :: GLES GL_RENDERER : " + GLES20.glGetString(GLES20.GL_RENDERER));
        mRenderer = GLES20.glGetString(GLES20.GL_RENDERER);
    }

    public String getRenderer() {
        return mRenderer;
    }

    public boolean validateDeveloperOption(boolean angleEnabled)  {
        if (angleEnabled) {
            if (!mRenderer.toLowerCase().contains("angle")) {
                return false;
            }
        } else {
            if (mRenderer.toLowerCase().contains("angle")) {
                return false;
            }
        }

        return true;
    }
}
