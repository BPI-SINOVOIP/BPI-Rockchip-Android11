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
 *
 */

#include "circle.h"
#include "common.h"
#include "shader.h"

#include <cstdlib>
#include <math.h>

namespace android {
namespace gamecore {

namespace {

GLfloat* initializeVertices(int numSegments) {
    GLfloat* vertices = new GLfloat[2 * (numSegments + 2)];
    vertices[0] = 0.0f;
    vertices[1] = 0.0f;

    float dTheta = static_cast<float>(2 * M_PI / numSegments);
    for (int i = 0; i < numSegments + 1; i++) {
        vertices[(i + 1) * 2] = cos(dTheta * i);
        vertices[(i + 1) * 2 + 1] = sin(dTheta * i);
    }
    return vertices;
}

int const NUM_SEGMENTS = 36;
GLfloat* gVertices = initializeVertices(NUM_SEGMENTS);

auto const gVertexShader =
        "uniform highp float uRadius;\n"
        "uniform highp mat4 uMvpMatrix;\n"
        "attribute vec4 vPosition;\n"
        "void main() {\n"
        "  gl_Position = uMvpMatrix * (vPosition * vec4(vec3(uRadius), 1.0));"
        "}\n";

auto const gFragmentShader =
        "uniform lowp vec3 uColor;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(uColor, 1.0);\n"
        "}\n";

} // end of anonymous namespace

Circle::Circle(float radius) {
    mRadius = radius;
    mProgram = createProgram(gVertexShader, gFragmentShader);
    mMvpMatrixHandle = glGetUniformLocation(mProgram, "uMvpMatrix");
    mRadiusHandle = glGetUniformLocation(mProgram, "uRadius");
    mVPositionHandle = glGetAttribLocation(mProgram, "vPosition");
    mColorHandle = glGetUniformLocation(mProgram, "uColor");
    checkGlError("glGetAttribLocation");
    LOGI("glGetAttribLocation(\"vPosition\") = %d\n",
         mVPositionHandle);

    setColor(0.0f, 1.0f, 0.0f);
}

void Circle::draw() const {
    Mat4 mvpMatrix = mViewProjectionMatrix * Mat4::Translation(mPosition);
    glUseProgram(mProgram);
    checkGlError("glUseProgram");

    glUniform3fv(mColorHandle, 1, mColor);
    checkGlError("glUniform3fv");
    glUniform1f(mRadiusHandle, mRadius);
    checkGlError("glUniform1f");
    glVertexAttribPointer(mVPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, gVertices);
    checkGlError("glVertexAttribPointer");
    glUniformMatrix4fv(mMvpMatrixHandle, 1, GL_FALSE, mvpMatrix.Ptr());
    checkGlError("glUniformMatrix4fv");
    glEnableVertexAttribArray(mVPositionHandle);
    checkGlError("glEnableVertexAttribArray");
    glDrawArrays(GL_TRIANGLE_FAN, 0, NUM_SEGMENTS + 2);
    checkGlError("glDrawArrays");
}

void Circle::setColor(float r, float g, float b) {
    mColor[0] = r;
    mColor[1] = g;
    mColor[2] = b;
}

const Vec3& Circle::getPosition() const {
    return mPosition;
}

void Circle::setPosition(const Vec3& position) {
    mPosition = position;
}

void Circle::updateViewProjection(const Mat4& vpMatrix) {
    mViewProjectionMatrix = vpMatrix;
}

} // end of namespace gamecore
} // end of namespace android

