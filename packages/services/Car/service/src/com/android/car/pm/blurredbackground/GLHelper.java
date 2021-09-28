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
import android.opengl.GLES30;
import android.opengl.Matrix;
import android.os.Build;
import android.util.Log;
import android.util.Slog;

import androidx.annotation.Nullable;

import libcore.io.Streams;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;

/**
 * A helper class for simple OpenGL operations
 */
public class GLHelper {

    private static final String TAG = "GLHelper";
    private static final int SIZEOF_FLOAT = 4;

    /**
     * Creates an OpenGL program that uses the provided shader sources
     * and returns the id of the created program
     *
     * @param vertexShaderSource   The source for the vertex shader
     * @param fragmentShaderSource The source for the fragment shader
     * @return The id of the created program
     */
    public static int createProgram(String vertexShaderSource, String fragmentShaderSource) {
        int vertexShader = compileShader(GLES30.GL_VERTEX_SHADER, vertexShaderSource);
        int fragmentShader = compileShader(GLES30.GL_FRAGMENT_SHADER, fragmentShaderSource);

        int programId = GLES30.glCreateProgram();
        checkGlErrors("glCreateProgram");

        GLES30.glAttachShader(programId, vertexShader);
        GLES30.glAttachShader(programId, fragmentShader);

        // glDeleteShader flags these shaders to be deleted, the shaders
        // are not actually deleted until the program they are attached to are deleted
        GLES30.glDeleteShader(vertexShader);
        checkGlErrors("glDeleteShader");
        GLES30.glDeleteShader(fragmentShader);
        checkGlErrors("glDeleteShader");

        GLES30.glLinkProgram(programId);
        checkGlErrors("glLinkProgram");

        return programId;
    }

    /**
     * Creates and binds a texture and returns the id of the created texture
     *
     * @param textureIdBuffer The IntBuffer that will contain the created texture id
     * @param textureTarget   The texture target for the created texture
     * @return The id of the created and bound texture
     */
    public static int createAndBindTextureObject(IntBuffer textureIdBuffer, int textureTarget) {
        GLES30.glGenTextures(1, textureIdBuffer);

        int textureId = textureIdBuffer.get(0);

        GLES30.glBindTexture(textureTarget, textureId);
        checkGlErrors("glBindTexture");

        // We define the filters that will be applied to the textures if
        // they get minified or magnified when they are sampled
        GLES30.glTexParameterf(textureTarget, GLES30.GL_TEXTURE_MIN_FILTER,
                GLES30.GL_LINEAR);
        GLES30.glTexParameterf(textureTarget, GLES30.GL_TEXTURE_MAG_FILTER,
                GLES30.GL_LINEAR);

        // Set the wrap parameters for if the edges of the texture do not fill the surface
        GLES30.glTexParameteri(textureTarget, GLES30.GL_TEXTURE_WRAP_S,
                GLES30.GL_CLAMP_TO_EDGE);
        GLES30.glTexParameteri(textureTarget, GLES30.GL_TEXTURE_WRAP_T,
                GLES30.GL_CLAMP_TO_EDGE);

        return textureId;
    }

    /**
     * Creates and binds a Framebuffer object
     *
     * @param frameBuffer the IntBuffer that will contain the created Framebuffer ID
     * @return The id of the created and bound Framebuffer
     */
    public static int createAndBindFramebuffer(IntBuffer frameBuffer) {
        GLES30.glGenFramebuffers(1, frameBuffer);
        checkGlErrors("glGenFramebuffers");

        int frameBufferId = frameBuffer.get(0);

        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, frameBufferId);
        checkGlErrors("glBindFramebuffer");

        return frameBufferId;
    }

    /**
     * Retrieves a string of an OpenGL shader
     *
     * @param id the ID of the raw shader resource
     * @return The shader script, null if the shader failed to load
     */
    public static @Nullable String getShaderFromRaw(Context context, int id) {
        try {
            InputStream stream = context.getResources().openRawResource(id);
            return new String(Streams.readFully(new InputStreamReader(stream)));
        } catch (IOException e) {
            Log.e(TAG, "Failed to load shader");
            return null;
        }
    }

    /**
     * Creates a FloatBuffer to hold texture and vertex coordinates
     *
     * @param coords The coordinates that will be held in the FloatBuffer
     * @return a FloatBuffer containing the provided coordinates
     */
    public static FloatBuffer createFloatBuffer(float[] coords) {
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(coords.length * SIZEOF_FLOAT);
        byteBuffer.order(ByteOrder.nativeOrder());

        FloatBuffer floatBuffer = byteBuffer.asFloatBuffer();
        floatBuffer.put(coords);
        floatBuffer.position(0);
        return floatBuffer;
    }

    /**
     * @return a float[] representing a 4x4 identity matrix
     */
    public static float[] getIdentityMatrix() {
        float[] identityMatrix = new float[16];
        Matrix.setIdentityM(identityMatrix, 0);
        return identityMatrix;
    }

    /**
     * Checks for GL errors, logging any errors found
     *
     * @param func The name of the most recent GL function called
     * @return a boolean representing if there was a GL error or not
     */
    public static boolean checkGlErrors(String func) {
        boolean hadError = false;
        int error;

        while ((error = GLES30.glGetError()) != GLES30.GL_NO_ERROR) {
            if (Build.IS_ENG || Build.IS_USERDEBUG) {
                Slog.e(TAG, func + " failed: error " + error, new Throwable());
            }
            hadError = true;
        }
        return hadError;
    }

    private static int compileShader(int shaderType, String shaderSource) {
        int shader = GLES30.glCreateShader(shaderType);
        GLES30.glShaderSource(shader, shaderSource);

        GLES30.glCompileShader(shader);
        checkGlErrors("glCompileShader");

        return shader;
    }
}
