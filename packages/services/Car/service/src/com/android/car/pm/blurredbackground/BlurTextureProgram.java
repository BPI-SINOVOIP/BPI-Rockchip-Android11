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

import android.graphics.Rect;
import android.opengl.GLES11Ext;
import android.opengl.GLES30;

import java.nio.FloatBuffer;
import java.nio.IntBuffer;

/**
 * A class containing the OpenGL programs used to render a blurred texture
 */
public class BlurTextureProgram {

    private static final float[] FRAME_COORDS = {
            -1.0f, -1.0f,   // 0 bottom left
            1.0f, -1.0f,    // 1 bottom right
            -1.0f, 1.0f,    // 2 top left
            1.0f, 1.0f,     // 3 top right
    };
    private static final float[] TEXTURE_COORDS = {
            0.0f, 0.0f,     // 0 bottom left
            1.0f, 0.0f,     // 1 bottom right
            0.0f, 1.0f,     // 2 top left
            1.0f, 1.0f      // 3 top right
    };

    private static final float[] INVERTED_TEXTURE_COORDS = {
            0.0f, 1.0f,     // 0 bottom left
            1.0f, 1.0f,     // 1 bottom right
            0.0f, 0.0f,     // 2 top left
            1.0f, 0.0f      // 3 top right
    };

    private static final int SIZEOF_FLOAT = 4;
    private static final int NUM_COORDS_PER_VERTEX = 2;
    private static final float BLUR_RADIUS = 40.0f;

    private final String mVertexShader;
    private final String mHorizontalBlurShader;
    private final String mVerticalBlurShader;

    private final int mHorizontalBlurProgram;
    private final int mVerticalBlurProgram;

    private final int mWidth;
    private final int mHeight;

    private final int mScreenshotTextureId;
    private final IntBuffer mScreenshotTextureBuffer;
    private final float[] mTexMatrix;
    private final FloatBuffer mResolutionBuffer;

    private final FloatBuffer mVertexBuffer = GLHelper.createFloatBuffer(FRAME_COORDS);
    private final FloatBuffer mTexBuffer = GLHelper.createFloatBuffer(TEXTURE_COORDS);
    private final FloatBuffer mInvertedTexBuffer = GLHelper.createFloatBuffer(
            INVERTED_TEXTURE_COORDS);

    // Locations of the uniforms and attributes for the horizontal program
    private final int mUHorizontalMVPMatrixLoc;
    private final int mUHorizontalTexMatrixLoc;
    private final int mUHorizontalResolutionLoc;
    private final int mUHorizontalRadiusLoc;
    private final int mAHorizontalPositionLoc;
    private final int mAHorizontalTextureCoordLoc;

    // Locations of the uniforms and attributes for the vertical program
    private final int mUVerticalMVPMatrixLoc;
    private final int mUVerticalTexMatrixLoc;
    private final int mUVerticalResolutionLoc;
    private final int mUVerticalRadiusLoc;
    private final int mAVerticalPositionLoc;
    private final int mAVerticalTextureCoordLoc;

    private final IntBuffer mFrameBuffer = IntBuffer.allocate(1);
    private final IntBuffer mFirstPassTextureBuffer = IntBuffer.allocate(1);

    private int mFrameBufferId;
    private int mFirstPassTextureId;

    /**
     * Constructor for the BlurTextureProgram
     *
     * @param screenshotTextureBuffer IntBuffer
     * @param texMatrix Float array used to scale the screenshot texture
     * @param vertexShader String containing the horizontal blur shader
     * @param horizontalBlurShader String containing the fragment shader for horizontal blur
     * @param verticalBlurShader String containing the fragment shader for vertical blur
     * @param windowRect Rect representing the location of the window being covered
     */
    BlurTextureProgram(
            IntBuffer screenshotTextureBuffer,
            float[] texMatrix,
            String vertexShader,
            String horizontalBlurShader,
            String verticalBlurShader,
            Rect windowRect
    ) {
        mVertexShader = vertexShader;
        mHorizontalBlurShader = horizontalBlurShader;
        mVerticalBlurShader = verticalBlurShader;

        mScreenshotTextureBuffer = screenshotTextureBuffer;
        mScreenshotTextureId = screenshotTextureBuffer.get(0);
        mTexMatrix = texMatrix;

        mHorizontalBlurProgram = GLHelper.createProgram(mVertexShader, mHorizontalBlurShader);
        mVerticalBlurProgram = GLHelper.createProgram(mVertexShader, mVerticalBlurShader);

        mWidth = windowRect.width();
        mHeight = windowRect.height();

        mResolutionBuffer = FloatBuffer.wrap(new float[]{(float) mWidth, (float) mHeight, 1.0f});

        // Initialize the uniform and attribute locations for the horizontal blur program
        mUHorizontalMVPMatrixLoc = GLES30.glGetUniformLocation(mHorizontalBlurProgram,
                "uMVPMatrix");
        mUHorizontalTexMatrixLoc = GLES30.glGetUniformLocation(mHorizontalBlurProgram,
                "uTexMatrix");
        mUHorizontalResolutionLoc = GLES30.glGetUniformLocation(mHorizontalBlurProgram,
                "uResolution");
        mUHorizontalRadiusLoc = GLES30.glGetUniformLocation(mHorizontalBlurProgram, "uRadius");

        mAHorizontalPositionLoc = GLES30.glGetAttribLocation(mHorizontalBlurProgram, "aPosition");
        mAHorizontalTextureCoordLoc = GLES30.glGetAttribLocation(mHorizontalBlurProgram,
                "aTextureCoord");

        // Initialize the uniform and attribute locations for the vertical blur program
        mUVerticalMVPMatrixLoc = GLES30.glGetUniformLocation(mVerticalBlurProgram, "uMVPMatrix");
        mUVerticalTexMatrixLoc = GLES30.glGetUniformLocation(mVerticalBlurProgram, "uTexMatrix");
        mUVerticalResolutionLoc = GLES30.glGetUniformLocation(mVerticalBlurProgram, "uResolution");
        mUVerticalRadiusLoc = GLES30.glGetUniformLocation(mVerticalBlurProgram, "uRadius");

        mAVerticalPositionLoc = GLES30.glGetAttribLocation(mVerticalBlurProgram, "aPosition");
        mAVerticalTextureCoordLoc = GLES30.glGetAttribLocation(mVerticalBlurProgram,
                "aTextureCoord");
    }

    /**
     * Executes all of the rendering logic.  Sets up FrameBuffers and programs to complete
     * two rendering passes on the captured screenshot to produce a blur.
     */
    public void render() {
        setupProgram(mHorizontalBlurProgram, mScreenshotTextureId,
                GLES11Ext.GL_TEXTURE_EXTERNAL_OES);
        setHorizontalUniformsAndAttributes();

        // Create the framebuffer that will hold the texture we render to
        // for the first shader pass
        mFrameBufferId = GLHelper.createAndBindFramebuffer(mFrameBuffer);

        // Create the empty texture that will store the output of the first shader pass (this'll
        // be held in the Framebuffer Object)
        mFirstPassTextureId = GLHelper.createAndBindTextureObject(mFirstPassTextureBuffer,
                GLES30.GL_TEXTURE_2D);

        setupTextureForFramebuffer(mFirstPassTextureId);
        assertValidFramebufferStatus();
        renderToFramebuffer(mFrameBufferId);

        setupProgram(mVerticalBlurProgram, mFirstPassTextureId, GLES30.GL_TEXTURE_2D);
        setVerticalUniformsAndAttributes();

        renderToSurface();
        cleanupResources();
    }

    /**
     * Cleans up all OpenGL resources used by programs in this class
     */
    public void cleanupResources() {
        deleteFramebufferTexture();
        deleteFrameBuffer();
        deletePrograms();

        GLES30.glFlush();
    }

    /**
     * Attaches a 2D texture image to the active framebuffer object
     *
     * @param textureId The ID of the texture to be attached
     */
    private void setupTextureForFramebuffer(int textureId) {
        GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D, 0, GLES30.GL_RGB, mWidth, mHeight, 0,
                GLES30.GL_RGB, GLES30.GL_UNSIGNED_BYTE, null);
        GLHelper.checkGlErrors("glTexImage2D");
        GLES30.glFramebufferTexture2D(GLES30.GL_FRAMEBUFFER, GLES30.GL_COLOR_ATTACHMENT0,
                GLES30.GL_TEXTURE_2D, textureId, 0);
        GLHelper.checkGlErrors("glFramebufferTexture2D");
    }

    /**
     * Deletes the texture stored in the framebuffer
     */
    private void deleteFramebufferTexture() {
        GLES30.glDeleteTextures(mFirstPassTextureBuffer.capacity(), mFirstPassTextureBuffer);
        GLHelper.checkGlErrors("glDeleteTextures");
    }

    /**
     * Deletes the frame buffers.
     */
    private void deleteFrameBuffer() {
        GLES30.glDeleteBuffers(1, mFrameBuffer);
        GLHelper.checkGlErrors("glDeleteBuffers");
    }

    /**
     * Deletes the GL programs.
     */
    private void deletePrograms() {
        GLES30.glDeleteProgram(mHorizontalBlurProgram);
        GLHelper.checkGlErrors("glDeleteProgram");
        GLES30.glDeleteProgram(mVerticalBlurProgram);
        GLHelper.checkGlErrors("glDeleteProgram");
    }

    /**
     * Set all of the Uniform and Attribute variable values for the horizontal blur program
     */
    private void setHorizontalUniformsAndAttributes() {
        GLES30.glUniformMatrix4fv(mUHorizontalMVPMatrixLoc, 1, false, GLHelper.getIdentityMatrix(),
                0);
        GLES30.glUniformMatrix4fv(mUHorizontalTexMatrixLoc, 1, false, mTexMatrix, 0);
        GLES30.glUniform3fv(mUHorizontalResolutionLoc, 1, mResolutionBuffer);
        GLES30.glUniform1f(mUHorizontalRadiusLoc, BLUR_RADIUS);

        GLES30.glEnableVertexAttribArray(mAHorizontalPositionLoc);
        GLES30.glVertexAttribPointer(mAHorizontalPositionLoc, NUM_COORDS_PER_VERTEX,
                GLES30.GL_FLOAT, false, NUM_COORDS_PER_VERTEX * SIZEOF_FLOAT, mVertexBuffer);

        GLES30.glEnableVertexAttribArray(mAHorizontalTextureCoordLoc);
        GLES30.glVertexAttribPointer(mAHorizontalTextureCoordLoc, 2,
                GLES30.GL_FLOAT, false, 2 * SIZEOF_FLOAT, mTexBuffer);
    }

    /**
     * Set all of the Uniform and Attribute variable values for the vertical blur program
     */
    private void setVerticalUniformsAndAttributes() {
        GLES30.glUniformMatrix4fv(mUVerticalMVPMatrixLoc, 1, false, GLHelper.getIdentityMatrix(),
                0);
        GLES30.glUniformMatrix4fv(mUVerticalTexMatrixLoc, 1, false, mTexMatrix, 0);
        GLES30.glUniform3fv(mUVerticalResolutionLoc, 1, mResolutionBuffer);
        GLES30.glUniform1f(mUVerticalRadiusLoc, BLUR_RADIUS);

        GLES30.glEnableVertexAttribArray(mAVerticalPositionLoc);
        GLES30.glVertexAttribPointer(mAVerticalPositionLoc, NUM_COORDS_PER_VERTEX,
                GLES30.GL_FLOAT, false, NUM_COORDS_PER_VERTEX * SIZEOF_FLOAT, mVertexBuffer);

        GLES30.glEnableVertexAttribArray(mAVerticalTextureCoordLoc);
        GLES30.glVertexAttribPointer(mAVerticalTextureCoordLoc, 2,
                GLES30.GL_FLOAT, false, 2 * SIZEOF_FLOAT, mInvertedTexBuffer);
    }

    /**
     * Sets the program to be used in the next rendering, and binds
     * a texture to it
     *
     * @param programId     The Id of the program
     * @param textureId     The Id of the texture to be bound
     * @param textureTarget The type of texture that is being bound
     */
    private void setupProgram(int programId, int textureId, int textureTarget) {
        GLES30.glUseProgram(programId);
        GLHelper.checkGlErrors("glUseProgram");

        GLES30.glActiveTexture(GLES30.GL_TEXTURE0);
        GLHelper.checkGlErrors("glActiveTexture");

        GLES30.glBindTexture(textureTarget, textureId);
        GLHelper.checkGlErrors("glBindTexture");
    }

    /**
     * Renders to a framebuffer using the current active program
     *
     * @param framebufferId The Id of the framebuffer being rendered to
     */
    private void renderToFramebuffer(int framebufferId) {
        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT);
        GLHelper.checkGlErrors("glClear");

        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, framebufferId);
        GLHelper.checkGlErrors("glBindFramebuffer");

        GLES30.glViewport(0, 0, mWidth, mHeight);
        GLHelper.checkGlErrors("glViewport");

        GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP, 0,
                FRAME_COORDS.length / NUM_COORDS_PER_VERTEX);
        GLHelper.checkGlErrors("glDrawArrays");

        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, 0);
    }

    /**
     * Renders to a the GLSurface using the current active program
     */
    private void renderToSurface() {
        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, 0);
        GLHelper.checkGlErrors("glDrawArrays");

        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT);
        GLHelper.checkGlErrors("glDrawArrays");

        GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP, 0,
                FRAME_COORDS.length / NUM_COORDS_PER_VERTEX);
        GLHelper.checkGlErrors("glDrawArrays");
    }

    private void assertValidFramebufferStatus() {
        if (GLES30.glCheckFramebufferStatus(GLES30.GL_FRAMEBUFFER)
                != GLES30.GL_FRAMEBUFFER_COMPLETE) {
            throw new RuntimeException(
                    "Failed to attach Framebuffer. Framebuffer status code is: "
                            + GLES30.glCheckFramebufferStatus(GLES30.GL_FRAMEBUFFER));
        }
    }
}
