/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.car.pm.blurredbackground;

import android.content.Context;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.opengl.GLES11Ext;
import android.opengl.GLES30;
import android.opengl.GLSurfaceView;
import android.os.IBinder;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceControl;

import com.android.car.R;

import java.nio.IntBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * The renderer class for the {@link GLSurfaceView} of the {@link ActivityBlockingActivity}
 */
public class BlurredSurfaceRenderer implements GLSurfaceView.Renderer {

    private static final String TAG = "BlurredSurfaceRenderer";
    private static final int NUM_INDICES_TO_RENDER = 4;

    private final String mVertexShader;
    private final String mHorizontalBlurShader;
    private final String mVerticalBlurShader;
    private final Rect mWindowRect;

    private BlurTextureProgram mProgram;
    private SurfaceTexture mSurfaceTexture;
    private Surface mSurface;

    private int mScreenshotTextureId;
    private final IntBuffer mScreenshotTextureBuffer = IntBuffer.allocate(1);
    private final float[] mTexMatrix = new float[16];

    private final boolean mShadersLoadedSuccessfully;
    private final boolean mShouldRenderBlurred;
    private boolean mIsScreenShotCaptured = false;

    /**
     * Constructs a new {@link BlurredSurfaceRenderer} and loads the shaders
     * needed for rendering a blurred texture
     *
     * @param windowRect Rect that represents the application window
     */
    public BlurredSurfaceRenderer(Context context, Rect windowRect, boolean shouldRenderBlurred) {
        mShouldRenderBlurred = shouldRenderBlurred;

        mVertexShader = GLHelper.getShaderFromRaw(context, R.raw.vertex_shader);
        mHorizontalBlurShader = GLHelper.getShaderFromRaw(context,
                R.raw.horizontal_blur_fragment_shader);
        mVerticalBlurShader = GLHelper.getShaderFromRaw(context,
                R.raw.vertical_blur_fragment_shader);

        mShadersLoadedSuccessfully = mVertexShader != null
                && mHorizontalBlurShader != null
                && mVerticalBlurShader != null;

        mWindowRect = windowRect;
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        mScreenshotTextureId = GLHelper.createAndBindTextureObject(mScreenshotTextureBuffer,
                GLES11Ext.GL_TEXTURE_EXTERNAL_OES);

        mSurfaceTexture = new SurfaceTexture(mScreenshotTextureId);
        mSurface = new Surface(mSurfaceTexture);

        if (mShouldRenderBlurred) {
            mIsScreenShotCaptured = captureScreenshot();
        }
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        if (shouldDrawFrame()) {
            mProgram = new BlurTextureProgram(
                    mScreenshotTextureBuffer,
                    mTexMatrix,
                    mVertexShader,
                    mHorizontalBlurShader,
                    mVerticalBlurShader,
                    mWindowRect
            );
            mProgram.render();
        } else {
            logWillNotRenderBlurredMsg();

            // If we determine we shouldn't render a blurred texture, we
            // will default to rendering a transparent GLSurfaceView so that
            // the ActivityBlockingActivity appears translucent
            renderTransparent();
        }
    }

    private void renderTransparent() {
        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT);
        GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP, /*first index to render */ 0,
                NUM_INDICES_TO_RENDER);
    }

    /**
     * Called when the ActivityBlockingActivity pauses
     * cleans up the OpenGL program
     */
    public void onPause() {
        if (mProgram != null) {
            mProgram.cleanupResources();
        }
        deleteScreenshotTexture();
    }

    private boolean captureScreenshot() {
        boolean isScreenshotCaptured = false;

        try {
            final IBinder token = SurfaceControl.getInternalDisplayToken();
            if (token == null) {
                Log.e(TAG,
                        "Could not find display token for screenshot. Will not capture screenshot");
            } else {
                SurfaceControl.screenshot(
                        token,
                        mSurface,
                        mWindowRect,
                        mWindowRect.width(),
                        mWindowRect.height(),
                        /* useIdentityTransform= */ false,
                        Surface.ROTATION_0
                );

                mSurfaceTexture.updateTexImage();
                mSurfaceTexture.getTransformMatrix(mTexMatrix);
                isScreenshotCaptured = true;
            }

        } finally {
            mSurface.release();
            mSurfaceTexture.release();
        }

        return isScreenshotCaptured;
    }

    private void deleteScreenshotTexture() {
        GLES30.glDeleteTextures(mScreenshotTextureBuffer.capacity(), mScreenshotTextureBuffer);
        GLHelper.checkGlErrors("glDeleteTextures");

        mIsScreenShotCaptured = false;
    }

    private void logWillNotRenderBlurredMsg() {
        if (!mIsScreenShotCaptured) {
            Log.e(TAG, "Screenshot was not captured. Will not render blurred surface");
        }
        if (!mShadersLoadedSuccessfully) {
            Log.e(TAG, "Shaders were not loaded successfully. Will not render blurred surface");
        }
    }

    private boolean shouldDrawFrame() {
        return mIsScreenShotCaptured
                && mShadersLoadedSuccessfully
                && mShouldRenderBlurred;
    }
}

