/*
 * Copyright 2020 The Android Open Source Project
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

#include "shader.h"

#include <android-base/logging.h>
#include <memory>
#include <stdio.h>

// Given shader source, load and compile it
static GLuint loadShader(GLenum type, const char *shaderSrc, const char *name) {
    // Create the shader object
    GLuint shader = glCreateShader (type);
    if (shader == 0) {
        return 0;
    }

    // Load and compile the shader
    glShaderSource(shader, 1, &shaderSrc, nullptr);
    glCompileShader(shader);

    // Verify the compilation worked as expected
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        LOG(ERROR) << "Error compiling "
                   << (type==GL_VERTEX_SHADER ? "vtx":"pxl")
                   << " shader for "
                   << name;

        GLint size = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &size);
        if (size > 0) {
            // Get and report the error message
            std::unique_ptr<char> infoLog(new char[size]);
            glGetShaderInfoLog(shader, size, NULL, infoLog.get());
            LOG(ERROR) << "  msg:\n"
                       << infoLog.get();
        }

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}


// Create a program object given vertex and pixels shader source
GLuint buildShaderProgram(const char* vtxSrc, const char* pxlSrc, const char* name) {
    GLuint program = glCreateProgram();
    if (program == 0) {
        LOG(ERROR) << "Failed to allocate program object";
        return 0;
    }

    // Compile the shaders and bind them to this program
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vtxSrc, name);
    if (vertexShader == 0) {
        LOG(ERROR) << "Failed to load vertex shader";
        glDeleteProgram(program);
        return 0;
    }
    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pxlSrc, name);
    if (pixelShader == 0) {
        LOG(ERROR) << "Failed to load pixel shader";
        glDeleteProgram(program);
        glDeleteShader(vertexShader);
        return 0;
    }
    glAttachShader(program, vertexShader);
    glAttachShader(program, pixelShader);

    // Link the program
    glLinkProgram(program);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        LOG(ERROR) << "Error linking program.";
        GLint size = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &size);
        if (size > 0) {
            // Get and report the error message
            std::unique_ptr<char> infoLog(new char[size]);
            glGetProgramInfoLog(program, size, NULL, infoLog.get());
            LOG(ERROR) << "  msg:  "
                       << infoLog.get();
        }

        glDeleteProgram(program);
        glDeleteShader(vertexShader);
        glDeleteShader(pixelShader);
        return 0;
    }

    return program;
}

