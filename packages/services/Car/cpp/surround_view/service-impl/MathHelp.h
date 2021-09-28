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

#ifndef SURROUND_VIEW_SERVICE_IMPL_MATH_HELP_H_
#define SURROUND_VIEW_SERVICE_IMPL_MATH_HELP_H_

#include "Matrix4x4.h"
#include "core_lib.h"

#include <android-base/logging.h>
namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

using android_auto::surround_view::Mat4x4;

const int gMat4Size = 4 * 4 * sizeof(float);

const Mat4x4 gMat4Identity = {1, 0, 0, /*tx=*/0.0, 0, 1, 0, /*ty=*/0,
                              0, 0, 1, /*tz=*/0.0, 0, 0, 0, 1};

inline float degToRad(float angleInDegrees) {
    return 1.0f * angleInDegrees / 180 * M_PI;
}

typedef std::array<float, 3> VectorT;
typedef std::array<float, 4> HomVectorT;
typedef Matrix4x4<float> HomMatrixT;

// Create a Translation matrix.
inline HomMatrixT translationMatrix(const VectorT& v) {
    HomMatrixT m = HomMatrixT::identity();
    m.setRow(3, HomVectorT{v[0], v[1], v[2], 1});
    return m;
}

// Create a Rotation matrix.
inline HomMatrixT rotationMatrix(const VectorT& v, float angle, int orientation) {
    const float c = cos(angle);
    const float s = orientation * sin(angle);
    const float t = 1 - c;
    const float tx = t * v[0];
    const float ty = t * v[1];
    const float tz = t * v[2];
    return HomMatrixT(tx * v[0] + c, tx * v[1] + s * v[2], tx * v[2] - s * v[1], 0,
                      tx * v[1] - s * v[2], ty * v[1] + c, ty * v[2] + s * v[0], 0,
                      tx * v[2] + s * v[1], ty * v[2] - s * v[0], tz * v[2] + c, 0, 0, 0, 0, 1);
}

inline Mat4x4 toMat4x4(const Matrix4x4F& matrix4x4F) {
    Mat4x4 mat4x4;
    memcpy(&mat4x4[0], matrix4x4F.transpose().data(), gMat4Size);
    return mat4x4;
}

inline Matrix4x4F toMatrix4x4F(const Mat4x4& mat4x4) {
    Matrix4x4F matrix4x4F;
    memcpy(matrix4x4F.data(), &mat4x4[0], gMat4Size);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (matrix4x4F(i, j) != mat4x4[i * 4 + j]) {
                LOG(ERROR) << "Matrix error";
            }
        }
    }
    return matrix4x4F.transpose();
}

// Create a Rotation Matrix, around a unit vector by a ccw angle.
inline Mat4x4 rotationMatrix(float angleInDegrees, const VectorT& axis) {
    return toMat4x4(rotationMatrix(axis, degToRad(angleInDegrees), 1));
}

inline Mat4x4 appendRotation(float angleInDegrees, const VectorT& axis, const Mat4x4& mat4) {
    return toMat4x4(toMatrix4x4F(mat4) * rotationMatrix(axis, degToRad(angleInDegrees), 1));
}

// Append mat_l * mat_r;
inline Mat4x4 appendMat(const Mat4x4& matL, const Mat4x4& matR) {
    return toMat4x4(toMatrix4x4F(matL) * toMatrix4x4F(matR));
}

// Rotate about a point about a unit vector.
inline Mat4x4 rotationAboutPoint(float angleInDegrees, const VectorT& point, const VectorT& axis) {
    VectorT pointInv = point;
    pointInv[0] *= -1;
    pointInv[1] *= -1;
    pointInv[2] *= -1;
    return toMat4x4(translationMatrix(pointInv) *
                    rotationMatrix(axis, degToRad(angleInDegrees), 1) * translationMatrix(point));
}

inline Mat4x4 translationMatrixToMat4x4(const VectorT& translation) {
    return toMat4x4(translationMatrix(translation));
}

inline Mat4x4 appendTranslation(const VectorT& translation, const Mat4x4& mat4) {
    return toMat4x4(toMatrix4x4F(mat4) * translationMatrix(translation));
}

inline Mat4x4 appendMatrix(const Mat4x4& deltaMatrix, const Mat4x4& currentMatrix) {
    return toMat4x4(toMatrix4x4F(deltaMatrix) * toMatrix4x4F(currentMatrix));
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif  // SURROUND_VIEW_SERVICE_IMPL_MATH_HELP_H_
