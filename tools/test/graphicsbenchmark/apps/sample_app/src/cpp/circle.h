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

#ifndef ANDROID_GAMECORE_CIRCLE_H
#define ANDROID_GAMECORE_CIRCLE_H

#include "vecmath.h"
#include <GLES2/gl2.h>

namespace android {
namespace gamecore {

class Circle {
private:
    float mRadius;
    GLuint mProgram;

    GLuint mVPositionHandle;
    GLuint mMvpMatrixHandle;
    GLuint mRadiusHandle;
    GLuint mColorHandle;
    GLfloat mColor[3];
    Vec3 mPosition;
    Mat4 mViewProjectionMatrix;

public:
    Circle(float radius);

    void setColor(float r, float g, float b);

    const Vec3& getPosition() const;
    void setPosition(const Vec3& position);

    void updateViewProjection(const Mat4& vpMatrix);
    void draw() const;

};

} // end of namespace gamecore
} // end of namespace android

#endif // ANDROID_GAMECORE_CIRCLE_H
