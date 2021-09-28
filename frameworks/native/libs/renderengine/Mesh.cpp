/*
 * Copyright 2013 The Android Open Source Project
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

#include <renderengine/Mesh.h>

#include <utils/Log.h>

namespace android {
namespace renderengine {

Mesh::Mesh(Primitive primitive, size_t vertexCount, size_t vertexSize, size_t texCoordSize,
           size_t cropCoordsSize, size_t shadowColorSize, size_t shadowParamsSize,
           size_t indexCount)
      : mVertexCount(vertexCount),
        mVertexSize(vertexSize),
        mTexCoordsSize(texCoordSize),
        mCropCoordsSize(cropCoordsSize),
        mShadowColorSize(shadowColorSize),
        mShadowParamsSize(shadowParamsSize),
        mPrimitive(primitive),
        mIndexCount(indexCount) {
    if (vertexCount == 0) {
        mVertices.resize(1);
        mVertices[0] = 0.0f;
        mStride = 0;
        return;
    }
    size_t stride = vertexSize + texCoordSize + cropCoordsSize + shadowColorSize + shadowParamsSize;
    size_t remainder = (stride * vertexCount) / vertexCount;
    // Since all of the input parameters are unsigned, if stride is less than
    // either vertexSize or texCoordSize, it must have overflowed. remainder
    // will be equal to stride as long as stride * vertexCount doesn't overflow.
    if ((stride < vertexSize) || (remainder != stride)) {
        ALOGE("Overflow in Mesh(..., %zu, %zu, %zu, %zu, %zu, %zu)", vertexCount, vertexSize,
              texCoordSize, cropCoordsSize, shadowColorSize, shadowParamsSize);
        mVertices.resize(1);
        mVertices[0] = 0.0f;
        mVertexCount = 0;
        mVertexSize = 0;
        mTexCoordsSize = 0;
        mCropCoordsSize = 0;
        mShadowColorSize = 0;
        mShadowParamsSize = 0;
        mStride = 0;
        return;
    }

    mVertices.resize(stride * vertexCount);
    mStride = stride;
    mIndices.resize(indexCount);
}

Mesh::Primitive Mesh::getPrimitive() const {
    return mPrimitive;
}

float const* Mesh::getPositions() const {
    return mVertices.data();
}
float* Mesh::getPositions() {
    return mVertices.data();
}

float const* Mesh::getTexCoords() const {
    return mVertices.data() + mVertexSize;
}
float* Mesh::getTexCoords() {
    return mVertices.data() + mVertexSize;
}

float const* Mesh::getCropCoords() const {
    return mVertices.data() + mVertexSize + mTexCoordsSize;
}
float* Mesh::getCropCoords() {
    return mVertices.data() + mVertexSize + mTexCoordsSize;
}

float const* Mesh::getShadowColor() const {
    return mVertices.data() + mVertexSize + mTexCoordsSize + mCropCoordsSize;
}
float* Mesh::getShadowColor() {
    return mVertices.data() + mVertexSize + mTexCoordsSize + mCropCoordsSize;
}

float const* Mesh::getShadowParams() const {
    return mVertices.data() + mVertexSize + mTexCoordsSize + mCropCoordsSize + mShadowColorSize;
}
float* Mesh::getShadowParams() {
    return mVertices.data() + mVertexSize + mTexCoordsSize + mCropCoordsSize + mShadowColorSize;
}

uint16_t const* Mesh::getIndices() const {
    return mIndices.data();
}

uint16_t* Mesh::getIndices() {
    return mIndices.data();
}

size_t Mesh::getVertexCount() const {
    return mVertexCount;
}

size_t Mesh::getVertexSize() const {
    return mVertexSize;
}

size_t Mesh::getTexCoordsSize() const {
    return mTexCoordsSize;
}

size_t Mesh::getShadowColorSize() const {
    return mShadowColorSize;
}

size_t Mesh::getShadowParamsSize() const {
    return mShadowParamsSize;
}

size_t Mesh::getByteStride() const {
    return mStride * sizeof(float);
}

size_t Mesh::getStride() const {
    return mStride;
}

size_t Mesh::getIndexCount() const {
    return mIndexCount;
}

} // namespace renderengine
} // namespace android
