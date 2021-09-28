/*
 * Copyright 2019 The Android Open Source Project
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

#define LOG_TAG "AImageDecoderTest"

#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/data_space.h>
#include <android/bitmap.h>
#include <android/imagedecoder.h>
#include <android/rect.h>

#include "NativeTestHelpers.h"

#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <stdio.h>
#include <unistd.h>
#include <vector>

#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

using AssetCloser = std::unique_ptr<AAsset, decltype(&AAsset_close)>;
using DecoderDeleter = std::unique_ptr<AImageDecoder, decltype(&AImageDecoder_delete)>;

static void testEmptyCreate(JNIEnv* env, jclass) {
    AImageDecoder* decoderPtr = nullptr;
    for (AImageDecoder** outDecoder : { &decoderPtr, (AImageDecoder**) nullptr }) {
        for (AAsset* asset : { nullptr }) {
            int result = AImageDecoder_createFromAAsset(asset, outDecoder);
            ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
            if (outDecoder) {
                ASSERT_EQ(nullptr, *outDecoder);
            }
        }

        for (int fd : { 0, -1 }) {
            int result = AImageDecoder_createFromFd(fd, outDecoder);
            ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
            if (outDecoder) {
                ASSERT_EQ(nullptr, *outDecoder);
            }
        }

        auto testEmptyBuffer = [env, outDecoder](void* buffer, size_t length) {
            int result = AImageDecoder_createFromBuffer(buffer, length, outDecoder);
            ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
            if (outDecoder) {
                ASSERT_EQ(nullptr, *outDecoder);
            }
        };
        testEmptyBuffer(nullptr, 0);
        char buf[4];
        testEmptyBuffer(buf, 0);
    }
}

static AAsset* openAsset(JNIEnv* env, jobject jAssets, jstring jFile, int mode) {
    AAssetManager* nativeManager = AAssetManager_fromJava(env, jAssets);
    const char* file = env->GetStringUTFChars(jFile, nullptr);
    AAsset* asset = AAssetManager_open(nativeManager, file, mode);
    if (!asset) {
        ALOGE("Could not open %s", file);
    } else {
        ALOGD("Testing %s", file);
    }
    env->ReleaseStringUTFChars(jFile, file);
    return asset;
}

static void testNullDecoder(JNIEnv* env, jclass, jobject jAssets, jstring jFile) {
    AAsset* asset = openAsset(env, jAssets, jFile, AASSET_MODE_BUFFER);
    ASSERT_NE(asset, nullptr);
    AssetCloser assetCloser(asset, AAsset_close);

    {
        int result = AImageDecoder_createFromAAsset(asset, nullptr);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
    }

    {
        const void* buffer = AAsset_getBuffer(asset);
        ASSERT_NE(buffer, nullptr);

        int result = AImageDecoder_createFromBuffer(buffer, AAsset_getLength(asset), nullptr);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
    }

    {
        off_t start, length;
        int fd = AAsset_openFileDescriptor(asset, &start, &length);
        ASSERT_GT(fd, 0);

        off_t offset = ::lseek(fd, start, SEEK_SET);
        ASSERT_EQ(start, offset);

        int result = AImageDecoder_createFromFd(fd, nullptr);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
        close(fd);
    }

    {
        auto stride = AImageDecoder_getMinimumStride(nullptr);
        ASSERT_EQ(0, stride);
    }

    {
        char buf[4];
        int result = AImageDecoder_decodeImage(nullptr, buf, 4, 4);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
    }

    {
        int result = AImageDecoder_setAndroidBitmapFormat(nullptr, ANDROID_BITMAP_FORMAT_RGBA_8888);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);

        auto format = AImageDecoderHeaderInfo_getAndroidBitmapFormat(nullptr);
        ASSERT_EQ(ANDROID_BITMAP_FORMAT_NONE, format);
    }

    {
        int result = AImageDecoder_setUnpremultipliedRequired(nullptr, true);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);

        int alpha = AImageDecoderHeaderInfo_getAlphaFlags(nullptr);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, alpha);
    }

    ASSERT_EQ(0, AImageDecoderHeaderInfo_getWidth(nullptr));
    ASSERT_EQ(0, AImageDecoderHeaderInfo_getHeight(nullptr));
    ASSERT_EQ(nullptr, AImageDecoderHeaderInfo_getMimeType(nullptr));
    ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, AImageDecoderHeaderInfo_getDataSpace(nullptr));

    {
        int result = AImageDecoder_setTargetSize(nullptr, 1, 1);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
    }
    {
        ARect rect {0, 0, 10, 10};
        int result = AImageDecoder_setCrop(nullptr, rect);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
    }

    for (ADataSpace dataSpace : { ADATASPACE_UNKNOWN, ADATASPACE_SCRGB_LINEAR, ADATASPACE_SRGB,
                                  ADATASPACE_SCRGB, ADATASPACE_DISPLAY_P3, ADATASPACE_BT2020_PQ,
                                  ADATASPACE_ADOBE_RGB, ADATASPACE_BT2020, ADATASPACE_BT709,
                                  ADATASPACE_DCI_P3, ADATASPACE_SRGB_LINEAR }) {
        int result = AImageDecoder_setDataSpace(nullptr, dataSpace);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
    }
}

static void testInfo(JNIEnv* env, jclass, jlong imageDecoderPtr, jint width, jint height,
                     jstring jMimeType, jboolean isF16, jint dataSpace) {
    AImageDecoder* decoder = reinterpret_cast<AImageDecoder*>(imageDecoderPtr);
    ASSERT_NE(decoder, nullptr);
    DecoderDeleter decoderDeleter(decoder, AImageDecoder_delete);

    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder);
    ASSERT_NE(info, nullptr);
    int32_t actualWidth = AImageDecoderHeaderInfo_getWidth(info);
    ASSERT_EQ(width, actualWidth);
    int32_t actualHeight = AImageDecoderHeaderInfo_getHeight(info);
    ASSERT_EQ(height, actualHeight);

    const char* mimeType = env->GetStringUTFChars(jMimeType, nullptr);
    ASSERT_NE(mimeType, nullptr);
    ASSERT_EQ(0, strcmp(mimeType, AImageDecoderHeaderInfo_getMimeType(info)));
    env->ReleaseStringUTFChars(jMimeType, mimeType);
    auto format = AImageDecoderHeaderInfo_getAndroidBitmapFormat(info);
    if (isF16) {
        ASSERT_EQ(ANDROID_BITMAP_FORMAT_RGBA_F16, format);
    } else {
        ASSERT_EQ(ANDROID_BITMAP_FORMAT_RGBA_8888, format);
    }

    ASSERT_EQ(dataSpace, AImageDecoderHeaderInfo_getDataSpace(info));
}

static jlong openAssetNative(JNIEnv* env, jclass, jobject jAssets, jstring jFile) {
    // FIXME: Test the other modes? Or more to the point, pass in the mode? It
    // seems that when we want a buffer we should use AASSET_MODE_BUFFER.
    AAsset* asset = openAsset(env, jAssets, jFile, AASSET_MODE_UNKNOWN);
    if (!asset) {
        fail(env, "Failed to open native asset!");
    }
    return reinterpret_cast<jlong>(asset);
}

static void closeAsset(JNIEnv*, jclass, jlong asset) {
    AAsset_close(reinterpret_cast<AAsset*>(asset));
}

static jlong createFromAsset(JNIEnv* env, jclass, jlong asset) {
    AImageDecoder* decoder = nullptr;
    int result = AImageDecoder_createFromAAsset(reinterpret_cast<AAsset*>(asset), &decoder);
    if (ANDROID_IMAGE_DECODER_SUCCESS != result || !decoder) {
        fail(env, "Failed to create AImageDecoder!");
    }
    return reinterpret_cast<jlong>(decoder);
}

static jlong createFromFd(JNIEnv* env, jclass, int fd) {
    AImageDecoder* decoder = nullptr;
    int result = AImageDecoder_createFromFd(fd, &decoder);
    if (ANDROID_IMAGE_DECODER_SUCCESS != result || !decoder) {
        fail(env, "Failed to create AImageDecoder!");
    }
    return reinterpret_cast<jlong>(decoder);
}

static jlong createFromAssetFd(JNIEnv* env, jclass, jlong assetPtr) {
    AAsset* asset = reinterpret_cast<AAsset*>(assetPtr);
    off_t start, length;
    int fd = AAsset_openFileDescriptor(asset, &start, &length);
    if (fd <= 0) {
        fail(env, "Failed to open file descriptor!");
        return -1;
    }

    off_t offset = ::lseek(fd, start, SEEK_SET);
    if (offset != start) {
        fail(env, "Failed to seek file descriptor!");
        return -1;
    }

    return createFromFd(env, nullptr, fd);
}

static jlong createFromAssetBuffer(JNIEnv* env, jclass, jlong assetPtr) {
    AAsset* asset = reinterpret_cast<AAsset*>(assetPtr);
    const void* buffer = AAsset_getBuffer(asset);
    if (!buffer) {
        fail(env, "AAsset_getBuffer failed!");
        return -1;
    }

    AImageDecoder* decoder = nullptr;
    int result = AImageDecoder_createFromBuffer(buffer, AAsset_getLength(asset), &decoder);
    if (ANDROID_IMAGE_DECODER_SUCCESS != result || !decoder) {
        fail(env, "AImageDecoder_createFromBuffer failed!");
        return -1;
    }
    return reinterpret_cast<jlong>(decoder);
}

static void testCreateIncomplete(JNIEnv* env, jclass, jobject jAssets, jstring jFile,
                                 jint truncatedLength) {
    AAsset* asset = openAsset(env, jAssets, jFile, AASSET_MODE_UNKNOWN);
    ASSERT_NE(asset, nullptr);
    AssetCloser assetCloser(asset, AAsset_close);

    const void* buffer = AAsset_getBuffer(asset);
    ASSERT_NE(buffer, nullptr);

    AImageDecoder* decoder;
    int result = AImageDecoder_createFromBuffer(buffer, truncatedLength, &decoder);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_INCOMPLETE, result);
    ASSERT_EQ(decoder, nullptr);
}

static void testCreateUnsupported(JNIEnv* env, jclass, jobject jAssets, jstring jFile) {
    AAsset* asset = openAsset(env, jAssets, jFile, AASSET_MODE_UNKNOWN);
    ASSERT_NE(asset, nullptr);
    AssetCloser assetCloser(asset, AAsset_close);

    AImageDecoder* decoder;
    int result = AImageDecoder_createFromAAsset(asset, &decoder);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_UNSUPPORTED_FORMAT, result);
    ASSERT_EQ(decoder, nullptr);
}

static void testSetFormat(JNIEnv* env, jclass, jlong imageDecoderPtr,
                          jboolean isF16, jboolean isGray) {
    AImageDecoder* decoder = reinterpret_cast<AImageDecoder*>(imageDecoderPtr);
    DecoderDeleter decoderDeleter(decoder, AImageDecoder_delete);

    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder);
    ASSERT_NE(info, nullptr);

    // Store the format so we can ensure that it doesn't change when we call
    // AImageDecoder_setAndroidBitmapFormat.
    const auto format = AImageDecoderHeaderInfo_getAndroidBitmapFormat(info);
    if (isF16) {
        ASSERT_EQ(ANDROID_BITMAP_FORMAT_RGBA_F16, format);
    } else {
        ASSERT_EQ(ANDROID_BITMAP_FORMAT_RGBA_8888, format);
    }

    int result = AImageDecoder_setAndroidBitmapFormat(decoder, ANDROID_BITMAP_FORMAT_A_8);
    if (isGray) {
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
    } else {
        ASSERT_EQ(ANDROID_IMAGE_DECODER_INVALID_CONVERSION, result);
    }
    ASSERT_EQ(format, AImageDecoderHeaderInfo_getAndroidBitmapFormat(info));

    result = AImageDecoder_setAndroidBitmapFormat(decoder, ANDROID_BITMAP_FORMAT_RGB_565);
    int alpha = AImageDecoderHeaderInfo_getAlphaFlags(info);
    if (alpha == ANDROID_BITMAP_FLAGS_ALPHA_OPAQUE) {
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
    } else {
        ASSERT_EQ(ANDROID_BITMAP_FLAGS_ALPHA_PREMUL, alpha);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_INVALID_CONVERSION, result);
    }
    ASSERT_EQ(format, AImageDecoderHeaderInfo_getAndroidBitmapFormat(info));

    for (auto newFormat : { ANDROID_BITMAP_FORMAT_RGBA_4444, ANDROID_BITMAP_FORMAT_NONE }) {
        result = AImageDecoder_setAndroidBitmapFormat(decoder, newFormat);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_INVALID_CONVERSION, result);
        ASSERT_EQ(format, AImageDecoderHeaderInfo_getAndroidBitmapFormat(info));
    }

    for (auto newFormat : { ANDROID_BITMAP_FORMAT_RGBA_8888, ANDROID_BITMAP_FORMAT_RGBA_F16 }) {
        result = AImageDecoder_setAndroidBitmapFormat(decoder, newFormat);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
        ASSERT_EQ(format, AImageDecoderHeaderInfo_getAndroidBitmapFormat(info));
    }

    for (auto invalidFormat : { -1, 42, 67 }) {
        result = AImageDecoder_setAndroidBitmapFormat(decoder, invalidFormat);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
        ASSERT_EQ(format, AImageDecoderHeaderInfo_getAndroidBitmapFormat(info));
    }
}

static void testSetUnpremul(JNIEnv* env, jclass, jlong imageDecoderPtr, jboolean hasAlpha) {
    AImageDecoder* decoder = reinterpret_cast<AImageDecoder*>(imageDecoderPtr);
    DecoderDeleter decoderDeleter(decoder, AImageDecoder_delete);

    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder);
    ASSERT_NE(info, nullptr);

    // Store the alpha so we can ensure that it doesn't change when we call
    // AImageDecoder_setUnpremultipliedRequired.
    const int alpha = AImageDecoderHeaderInfo_getAlphaFlags(info);
    if (hasAlpha) {
        ASSERT_EQ(ANDROID_BITMAP_FLAGS_ALPHA_PREMUL, alpha);
    } else {
        ASSERT_EQ(ANDROID_BITMAP_FLAGS_ALPHA_OPAQUE, alpha);
    }

    for (bool required : { true, false }) {
        int result = AImageDecoder_setUnpremultipliedRequired(decoder, required);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
        ASSERT_EQ(alpha, AImageDecoderHeaderInfo_getAlphaFlags(info));
    }
}

static int bytesPerPixel(int32_t format) {
    switch (format) {
        case ANDROID_BITMAP_FORMAT_RGBA_8888:
            return 4;
        case ANDROID_BITMAP_FORMAT_RGB_565:
            return 2;
        case ANDROID_BITMAP_FORMAT_A_8:
            return 1;
        case ANDROID_BITMAP_FORMAT_RGBA_F16:
            return 8;
        case ANDROID_BITMAP_FORMAT_NONE:
        case ANDROID_BITMAP_FORMAT_RGBA_4444:
        default:
            return 0;
    }
}

static void testGetMinimumStride(JNIEnv* env, jclass, jlong imageDecoderPtr,
                                 jboolean isF16, jboolean isGray) {
    AImageDecoder* decoder = reinterpret_cast<AImageDecoder*>(imageDecoderPtr);
    DecoderDeleter decoderDeleter(decoder, AImageDecoder_delete);

    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder);
    ASSERT_NE(info, nullptr);

    const int32_t width = AImageDecoderHeaderInfo_getWidth(info);
    size_t stride = AImageDecoder_getMinimumStride(decoder);

    if (isF16) {
        ASSERT_EQ(bytesPerPixel(ANDROID_BITMAP_FORMAT_RGBA_F16) * width, stride);
    } else {
        ASSERT_EQ(bytesPerPixel(ANDROID_BITMAP_FORMAT_RGBA_8888) * width, stride);
    }

    auto setFormatAndCheckStride = [env, decoder, width, &stride](AndroidBitmapFormat format) {
        int result = AImageDecoder_setAndroidBitmapFormat(decoder, format);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

        stride = AImageDecoder_getMinimumStride(decoder);
        ASSERT_EQ(bytesPerPixel(format) * width, stride);
    };

    int alpha = AImageDecoderHeaderInfo_getAlphaFlags(info);
    if (alpha == ANDROID_BITMAP_FLAGS_ALPHA_OPAQUE) {
        setFormatAndCheckStride(ANDROID_BITMAP_FORMAT_RGB_565);
    }

    if (isGray) {
        setFormatAndCheckStride(ANDROID_BITMAP_FORMAT_A_8);
    }

    for (auto newFormat : { ANDROID_BITMAP_FORMAT_RGBA_8888, ANDROID_BITMAP_FORMAT_RGBA_F16 }) {
        setFormatAndCheckStride(newFormat);
    }

    for (auto badFormat : { ANDROID_BITMAP_FORMAT_RGBA_4444, ANDROID_BITMAP_FORMAT_NONE }) {
        int result = AImageDecoder_setAndroidBitmapFormat(decoder, badFormat);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_INVALID_CONVERSION, result);

        // Stride is unchanged.
        ASSERT_EQ(stride, AImageDecoder_getMinimumStride(decoder));
    }
}

static bool bitmapsEqual(size_t minStride, int height,
                         void* pixelsA, size_t strideA,
                         void* pixelsB, size_t strideB) {
    for (int y = 0; y < height; ++y) {
        auto* rowA = reinterpret_cast<char*>(pixelsA) + strideA * y;
        auto* rowB = reinterpret_cast<char*>(pixelsB) + strideB * y;
        if (memcmp(rowA, rowB, minStride) != 0) {
            ALOGE("Bitmap mismatch on line %i", y);
            return false;
        }
    }
    return true;
}

#define EXPECT_EQ(msg, a, b)    \
    if ((a) != (b)) {           \
        ALOGE(msg);             \
        return false;           \
    }

#define EXPECT_GE(msg, a, b)    \
    if ((a) < (b)) {            \
        ALOGE(msg);             \
        return false;           \
    }

static bool bitmapsEqual(JNIEnv* env, jobject jbitmap, int32_t androidBitmapFormat,
                         int width, int height, int alphaFlags, size_t minStride,
                         void* pixelsA, size_t strideA) {
    AndroidBitmapInfo jInfo;
    int bitmapResult = AndroidBitmap_getInfo(env, jbitmap, &jInfo);
    EXPECT_EQ("Failed to getInfo on Bitmap", ANDROID_BITMAP_RESULT_SUCCESS, bitmapResult);

    EXPECT_EQ("Wrong format", jInfo.format, androidBitmapFormat);

    // If the image is truly opaque, the Java Bitmap will report OPAQUE, even if
    // the AImageDecoder requested PREMUL/UNPREMUL. In that case, it is okay for
    // the two to disagree. We must ensure that we don't end up with one PREMUL
    // and the other UNPREMUL, though.
    int jAlphaFlags = jInfo.flags & ANDROID_BITMAP_FLAGS_ALPHA_MASK;
    if (jAlphaFlags != ANDROID_BITMAP_FLAGS_ALPHA_OPAQUE
            && alphaFlags != ANDROID_BITMAP_FLAGS_ALPHA_OPAQUE) {
        EXPECT_EQ("Wrong alpha type", jAlphaFlags, alphaFlags);
    }

    EXPECT_EQ("Wrong width", jInfo.width, width);
    EXPECT_EQ("Wrong height", jInfo.height, height);

    EXPECT_GE("Stride too small", jInfo.stride, minStride);

    void* jPixels;
    bitmapResult = AndroidBitmap_lockPixels(env, jbitmap, &jPixels);
    EXPECT_EQ("Failed to lockPixels", ANDROID_BITMAP_RESULT_SUCCESS, bitmapResult);

    bool equal = bitmapsEqual(minStride, height, pixelsA, strideA, jPixels, jInfo.stride);

    bitmapResult = AndroidBitmap_unlockPixels(env, jbitmap);
    EXPECT_EQ("Failed to unlockPixels", ANDROID_BITMAP_RESULT_SUCCESS, bitmapResult);

    return equal;
}

static void testDecode(JNIEnv* env, jclass, jlong imageDecoderPtr,
                       jint androidBitmapFormat, jboolean unpremul, jobject jbitmap) {
    AImageDecoder* decoder = reinterpret_cast<AImageDecoder*>(imageDecoderPtr);
    DecoderDeleter decoderDeleter(decoder, AImageDecoder_delete);

    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder);
    ASSERT_NE(info, nullptr);

    int result;
    int alphaFlags = AImageDecoderHeaderInfo_getAlphaFlags(info);
    if (androidBitmapFormat == ANDROID_BITMAP_FORMAT_NONE) {
        androidBitmapFormat = AImageDecoderHeaderInfo_getAndroidBitmapFormat(info);
    } else {
        result = AImageDecoder_setAndroidBitmapFormat(decoder, androidBitmapFormat);
        if (androidBitmapFormat == ANDROID_BITMAP_FORMAT_RGB_565) {
            if (alphaFlags != ANDROID_BITMAP_FLAGS_ALPHA_OPAQUE) {
                ASSERT_EQ(ANDROID_IMAGE_DECODER_INVALID_CONVERSION, result);

                // The caller only passes down the Bitmap if it is opaque.
                ASSERT_EQ(nullptr, jbitmap);
                return;
            }
        }
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
    }

    if (unpremul) {
        result = AImageDecoder_setUnpremultipliedRequired(decoder, true);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
        alphaFlags = ANDROID_BITMAP_FLAGS_ALPHA_UNPREMUL;
    }

    const int32_t width = AImageDecoderHeaderInfo_getWidth(info);
    const int32_t height = AImageDecoderHeaderInfo_getHeight(info);
    size_t minStride = AImageDecoder_getMinimumStride(decoder);

    size_t size = minStride * height;
    void* pixels = malloc(size);

    {
        // Try some invalid parameters.
        result = AImageDecoder_decodeImage(decoder, nullptr, minStride, size);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);

        result = AImageDecoder_decodeImage(decoder, pixels, minStride - 1, size);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);

        result = AImageDecoder_decodeImage(decoder, pixels, minStride, size - minStride);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);

        result = AImageDecoder_decodeImage(decoder, pixels, 0, size);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
    }

    result = AImageDecoder_decodeImage(decoder, pixels, minStride, size);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

    ASSERT_NE(jbitmap, nullptr);
    ASSERT_TRUE(bitmapsEqual(env, jbitmap, (AndroidBitmapFormat) androidBitmapFormat,
                             width, height, alphaFlags, minStride, pixels, minStride));

    // Setting to an invalid data space is unsupported, and has no effect on the
    // decodes below.
    for (int32_t dataSpace : std::initializer_list<int32_t>{ -1, ADATASPACE_UNKNOWN, 400 }) {
        result = AImageDecoder_setDataSpace(decoder, dataSpace);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
    }

    // Used for subsequent decodes, to ensure they are identical to the
    // original. For opaque images, this verifies that using PREMUL or UNPREMUL
    // look the same. For all images, this verifies that decodeImage can be
    // called multiple times.
    auto decodeAgain = [=](bool unpremultipliedRequired) {
        int r = AImageDecoder_setUnpremultipliedRequired(decoder, unpremultipliedRequired);
        if (ANDROID_IMAGE_DECODER_SUCCESS != r) {
            fail(env, "Failed to set alpha");
            return false;
        }

        void* otherPixels = malloc(size);
        r = AImageDecoder_decodeImage(decoder, otherPixels, minStride, size);
        if (ANDROID_IMAGE_DECODER_SUCCESS != r) {
            fail(env, "Failed to decode again with different settings");
            return false;
        }

        if (!bitmapsEqual(minStride, height, pixels, minStride, otherPixels, minStride)) {
            free(otherPixels);
            fail(env, "Decoding again with different settings did not match!");
            return false;
        }
        free(otherPixels);
        return true;
    };
    if (alphaFlags == ANDROID_BITMAP_FLAGS_ALPHA_OPAQUE) {
        for (bool unpremultipliedRequired: { true, false }) {
            if (!decodeAgain(unpremultipliedRequired)) return;
        }
    } else {
        if (!decodeAgain(unpremul)) return;
    }

    if (androidBitmapFormat == ANDROID_BITMAP_FORMAT_A_8) {
        // Attempting to set an ADataSpace is ignored by an A_8 decode.
        for (ADataSpace dataSpace : { ADATASPACE_DCI_P3, ADATASPACE_ADOBE_RGB }) {
            result = AImageDecoder_setDataSpace(decoder, dataSpace);
            ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
            if (!decodeAgain(alphaFlags)) return;
        }
    }

    free(pixels);
}

static void testDecodeStride(JNIEnv* env, jclass, jlong imageDecoderPtr) {
    AImageDecoder* decoder = reinterpret_cast<AImageDecoder*>(imageDecoderPtr);
    DecoderDeleter decoderDeleter(decoder, AImageDecoder_delete);

    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder);
    ASSERT_NE(info, nullptr);

    const int height = AImageDecoderHeaderInfo_getHeight(info);
    const int origWidth = AImageDecoderHeaderInfo_getWidth(info);

    for (int width : { origWidth, origWidth / 3 }) {
        if (width == 0) {
            // The 1 x 1 image cannot be downscaled.
            continue;
        }
        int result = AImageDecoder_setTargetSize(decoder, width, height);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
        for (AndroidBitmapFormat format : {
            ANDROID_BITMAP_FORMAT_RGBA_8888,
            ANDROID_BITMAP_FORMAT_RGB_565,
            ANDROID_BITMAP_FORMAT_A_8,
            ANDROID_BITMAP_FORMAT_RGBA_F16,
        }) {
            result = AImageDecoder_setAndroidBitmapFormat(decoder, format);
            if (result != ANDROID_IMAGE_DECODER_SUCCESS) {
                // Not all images can be decoded to all formats. This is okay, and
                // we've tested that we can decode to the expected formats elsewhere.
                continue;
            }

            size_t minStride = AImageDecoder_getMinimumStride(decoder);
            void* pixels = nullptr;

            // The code in the below loop relies on minStride being used first.
            std::vector<size_t> strides;
            strides.push_back(minStride);
            for (int i = 1; i <= 16; i++) {
                strides.push_back(i + minStride);
            }
            strides.push_back(minStride * 2);
            strides.push_back(minStride * 3);
            for (size_t stride : strides) {
                size_t size = stride * (height - 1) + minStride;
                void* decodePixels = malloc(size);
                result = AImageDecoder_decodeImage(decoder, decodePixels, stride, size);
                if ((stride - minStride) % bytesPerPixel(format) != 0) {
                    // The stride is not pixel aligned.
                    ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
                    continue;
                }
                ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

                if (pixels == nullptr) {
                    pixels = decodePixels;
                } else {
                    ASSERT_TRUE(bitmapsEqual(minStride, height, pixels, minStride, decodePixels,
                                             stride));
                    free(decodePixels);
                }
            }

            free(pixels);
        }
    }
}

static void testSetTargetSize(JNIEnv* env, jclass, jlong imageDecoderPtr) {
    AImageDecoder* decoder = reinterpret_cast<AImageDecoder*>(imageDecoderPtr);
    DecoderDeleter decoderDeleter(decoder, AImageDecoder_delete);

    const size_t defaultStride = AImageDecoder_getMinimumStride(decoder);

    for (int32_t width : { -1, 0, -500 }) {
        int result = AImageDecoder_setTargetSize(decoder, width, 100);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_INVALID_SCALE, result);
        // stride is unchanged, as the target size did not change.
        ASSERT_EQ(defaultStride, AImageDecoder_getMinimumStride(decoder));
    }

    for (int32_t height : { -1, 0, -300 }) {
        int result = AImageDecoder_setTargetSize(decoder, 100, height);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_INVALID_SCALE, result);
        // stride is unchanged, as the target size did not change.
        ASSERT_EQ(defaultStride, AImageDecoder_getMinimumStride(decoder));
    }

    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder);
    ASSERT_NE(info, nullptr);
    const int bpp = bytesPerPixel(AImageDecoderHeaderInfo_getAndroidBitmapFormat(info));

    for (int32_t width : { 7, 100, 275, 300 }) {
        int result = AImageDecoder_setTargetSize(decoder, width, 100);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

        size_t actualStride = AImageDecoder_getMinimumStride(decoder);
        size_t expectedStride = bpp * width;
        ASSERT_EQ(expectedStride, actualStride);
    }

    // Verify that setting a large enough width to overflow 31 bits fails.
    constexpr auto kMaxInt32 = std::numeric_limits<int32_t>::max();
    int32_t maxWidth = kMaxInt32 / bpp;
    for (int32_t width : { maxWidth / 2, maxWidth - 1, maxWidth }) {
        int result = AImageDecoder_setTargetSize(decoder, width, 1);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

        size_t actualStride = AImageDecoder_getMinimumStride(decoder);
        size_t expectedStride = bpp * width;
        ASSERT_EQ(expectedStride, actualStride);
    }

    for (int32_t width : { maxWidth + 1, (int32_t) (maxWidth * 1.5) }) {
        int result = AImageDecoder_setTargetSize(decoder, width, 1);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_INVALID_SCALE, result);
    }

    // A height that results in overflowing 31 bits also fails.
    int32_t maxHeight = kMaxInt32 / defaultStride;
    const int32_t width = AImageDecoderHeaderInfo_getWidth(info);
    for (int32_t height : { maxHeight / 2, maxHeight - 1, maxHeight }) {
        int result = AImageDecoder_setTargetSize(decoder, width, height);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

        size_t actualStride = AImageDecoder_getMinimumStride(decoder);
        size_t expectedStride = bpp * width;
        ASSERT_EQ(expectedStride, actualStride);
    }

    for (int32_t height : { maxHeight + 1, (int32_t) (maxHeight * 1.5) }) {
        int result = AImageDecoder_setTargetSize(decoder, width, height);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_INVALID_SCALE, result);
    }
}

struct SampledSizeParams {
    AImageDecoder* decoder;
    int sampleSize;
    int32_t* width;
    int32_t* height;
};

static void testComputeSampledSize(JNIEnv* env, jclass, jlong imageDecoderPtr,
                                   jobject jbitmap, jint sampleSize) {
    AImageDecoder* decoder = reinterpret_cast<AImageDecoder*>(imageDecoderPtr);
    DecoderDeleter decoderDeleter(decoder, AImageDecoder_delete);

    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder);
    ASSERT_NE(info, nullptr);
    const int32_t origWidth = AImageDecoderHeaderInfo_getWidth(info);
    const int32_t origHeight = AImageDecoderHeaderInfo_getHeight(info);

    int32_t width, height;
    // Test some bad parameters.
    for (SampledSizeParams p : std::initializer_list<SampledSizeParams>{
        { nullptr, 2, &width, &height },
        { decoder, 0, &width, &height },
        { decoder, -1, &width, &height },
        { decoder, 2, nullptr, &height },
        { decoder, 2, &width, nullptr },
    }) {
        int result = AImageDecoder_computeSampledSize(p.decoder, p.sampleSize, p.width, p.height);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
    }

    // Verify that width and height will never be less than one.
    for (SampledSizeParams p : std::initializer_list<SampledSizeParams>{
        { decoder, origWidth, &width, &height },
        { decoder, origWidth + 5, &width, &height },
        { decoder, origWidth * 2, &width, &height },
        { decoder, origHeight, &width, &height },
        { decoder, origHeight + 5, &width, &height },
        { decoder, origHeight * 2, &width, &height },
    }) {
        width = height = 0;
        int result = AImageDecoder_computeSampledSize(p.decoder, p.sampleSize, p.width, p.height);
        if (ANDROID_IMAGE_DECODER_SUCCESS != result) {
            fail(env, "computeSampledSize(%i) failed on image with dims %i x %i",
                 p.sampleSize, origWidth, origHeight);
            return;
        }

        ASSERT_GE(width, 1);
        ASSERT_GE(height, 1);
    }

    // jbitmap was created with the same sampleSize. Verify that AImageDecoder
    // computes the same output dimensions, that using those dimensions succeeds
    // with AImageDecoder, and the output matches.
    AndroidBitmapInfo jInfo;
    int bitmapResult = AndroidBitmap_getInfo(env, jbitmap, &jInfo);
    ASSERT_EQ(ANDROID_BITMAP_RESULT_SUCCESS, bitmapResult);

    width = height = 0;
    int result = AImageDecoder_computeSampledSize(decoder, sampleSize, &width, &height);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

    if (jInfo.width != width) {
        fail(env, "Orig image: %i x %i sampled by %i yields %i x %i expected %i x %i",
             origWidth, origHeight, sampleSize, width, height, jInfo.width, jInfo.height);
        return;
    }
    ASSERT_EQ(jInfo.width, width);
    ASSERT_EQ(jInfo.height, height);
    {

        ASSERT_LT(width, origWidth);
        ASSERT_LT(height, origHeight);

        ASSERT_LT(width, origWidth / sampleSize + sampleSize);
        ASSERT_LT(height, origHeight / sampleSize + sampleSize);
    }

    result = AImageDecoder_setTargetSize(decoder, width, height);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

    size_t minStride = AImageDecoder_getMinimumStride(decoder);
    size_t size = minStride * height;
    void* pixels = malloc(size);
    result = AImageDecoder_decodeImage(decoder, pixels, minStride, size);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

    ASSERT_TRUE(bitmapsEqual(env, jbitmap, AImageDecoderHeaderInfo_getAndroidBitmapFormat(info),
                             width, height, AImageDecoderHeaderInfo_getAlphaFlags(info),
                             minStride, pixels, minStride));
    free(pixels);
}

static void testDecodeScaled(JNIEnv* env, jclass, jlong imageDecoderPtr,
                             jobject jbitmap) {
    AImageDecoder* decoder = reinterpret_cast<AImageDecoder*>(imageDecoderPtr);
    DecoderDeleter decoderDeleter(decoder, AImageDecoder_delete);

    AndroidBitmapInfo jInfo;
    int bitmapResult = AndroidBitmap_getInfo(env, jbitmap, &jInfo);
    ASSERT_EQ(ANDROID_BITMAP_RESULT_SUCCESS, bitmapResult);

    int result = AImageDecoder_setTargetSize(decoder, jInfo.width, jInfo.height);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

    size_t minStride = AImageDecoder_getMinimumStride(decoder);
    size_t size = minStride * jInfo.height;
    void* pixels = malloc(size);

    {
        // Try some invalid parameters.
        result = AImageDecoder_decodeImage(decoder, nullptr, minStride, size);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);

        result = AImageDecoder_decodeImage(decoder, pixels, minStride - 1, size);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);

        result = AImageDecoder_decodeImage(decoder, pixels, minStride, size - minStride);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
    }

    result = AImageDecoder_decodeImage(decoder, pixels, minStride, size);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder);
    ASSERT_NE(info, nullptr);

    ASSERT_NE(jbitmap, nullptr);
    ASSERT_TRUE(bitmapsEqual(env, jbitmap, AImageDecoderHeaderInfo_getAndroidBitmapFormat(info),
                             jInfo.width, jInfo.height, AImageDecoderHeaderInfo_getAlphaFlags(info),
                             minStride, pixels, minStride));

    // Verify that larger strides still behave as expected.
    for (size_t stride : { minStride * 2, minStride * 3 }) {
        size_t size = stride * (jInfo.height - 1) + minStride;
        void* decodePixels = malloc(size);
        result = AImageDecoder_decodeImage(decoder, decodePixels, stride, size);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

        ASSERT_TRUE(bitmapsEqual(minStride, jInfo.height, pixels, minStride, decodePixels, stride));
        free(decodePixels);
    }

    free(pixels);
}

static void testSetCrop(JNIEnv* env, jclass, jobject jAssets, jstring jFile) {
    AAsset* asset = openAsset(env, jAssets, jFile, AASSET_MODE_UNKNOWN);
    ASSERT_NE(asset, nullptr);
    AssetCloser assetCloser(asset, AAsset_close);

    AImageDecoder* decoder;
    int result = AImageDecoder_createFromAAsset(asset, &decoder);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
    ASSERT_NE(decoder, nullptr);
    DecoderDeleter decoderDeleter(decoder, AImageDecoder_delete);

    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder);
    ASSERT_NE(info, nullptr);

    const int32_t width = AImageDecoderHeaderInfo_getWidth(info);
    const int32_t height = AImageDecoderHeaderInfo_getHeight(info);
    const auto format = AImageDecoderHeaderInfo_getAndroidBitmapFormat(info);
    const size_t defaultStride = AImageDecoder_getMinimumStride(decoder);

    if (width == 1 && height == 1) {
        // The more general crop tests do not map well to this image. Test 1 x 1
        // specifically.
        for (ARect invalidCrop : std::initializer_list<ARect> {
                { -1, 0, width, height },
                { 0, -1, width, height },
                { width, 0, 2 * width, height },
                { 0, height, width, 2 * height },
                { 1, 0, width + 1, height },
                { 0, 1, width, height + 1 },
                { 0, 0, 0, height },
                { 0, 0, width, 0 },
        }) {
            int result = AImageDecoder_setCrop(decoder, invalidCrop);
            ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
            ASSERT_EQ(defaultStride, AImageDecoder_getMinimumStride(decoder));
        }
        return;
    }

    for (ARect invalidCrop : std::initializer_list<ARect> {
            { -1, 0, width, height },
            { 0, -1, width, height },
            { width, 0, 2 * width, height },
            { 0, height, width, 2 * height },
            { 1, 0, width + 1, height },
            { 0, 1, width, height + 1 },
            { width - 1, 0, 1, height },
            { 0, height - 1, width, 1 },
            { 0, 0, 0, height },
            { 0, 0, width, 0 },
            { 1, 1, 1, 1 },
            { width, height, 0, 0 },
    }) {
        int result = AImageDecoder_setCrop(decoder, invalidCrop);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
        ASSERT_EQ(defaultStride, AImageDecoder_getMinimumStride(decoder));
    }

    for (ARect validCrop : std::initializer_list<ARect> {
            { 0, 0, width, height },
            { 0, 0, width / 2, height / 2},
            { 0, 0, width / 3, height },
            { 0, 0, width, height / 4},
            { width / 2, 0, width, height / 2},
            { 0, height / 2, width / 2, height },
            { width / 2, height / 2, width, height },
            { 1, 1, width - 1, height - 1 },
    }) {
        int result = AImageDecoder_setCrop(decoder, validCrop);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
        size_t actualStride = AImageDecoder_getMinimumStride(decoder);
        size_t expectedStride = bytesPerPixel(format) * (validCrop.right - validCrop.left);
        ASSERT_EQ(expectedStride, actualStride);
    }

    // Reset the crop, so we can test setting a crop *after* changing the
    // target size.
    result = AImageDecoder_setCrop(decoder, { 0, 0, 0, 0 });
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
    ASSERT_EQ(defaultStride, AImageDecoder_getMinimumStride(decoder));

    int32_t newWidth = width / 2, newHeight = height / 2;
    result = AImageDecoder_setTargetSize(decoder, newWidth, newHeight);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
    const size_t halfStride = AImageDecoder_getMinimumStride(decoder);
    {
        size_t expectedStride = bytesPerPixel(format) * newWidth;
        ASSERT_EQ(expectedStride, halfStride);
    }

    // At the smaller target size, crops that were previously valid no longer
    // are.
    for (ARect invalidCrop : std::initializer_list<ARect> {
            { 0, 0, width / 3, height },
            { 0, 0, width, height / 4},
            { width / 2, 0, width, height / 2},
            { 0, height / 2, width / 2, height },
            { width / 2, height / 2, width, height },
            { 1, 1, width - 1, height - 1 },
    }) {
        int result = AImageDecoder_setCrop(decoder, invalidCrop);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
        ASSERT_EQ(halfStride, AImageDecoder_getMinimumStride(decoder));
    }

    for (ARect validCrop : std::initializer_list<ARect> {
            { 0, 0, newWidth, newHeight },
            { 0, 0, newWidth / 3, newHeight },
            { 0, 0, newWidth, newHeight / 4},
            { newWidth / 2, 0, newWidth, newHeight / 2},
            { 0, newHeight / 2, newWidth / 2, newHeight },
            { newWidth / 2, newHeight / 2, newWidth, newHeight },
            { 1, 1, newWidth - 1, newHeight - 1 },
    }) {
        int result = AImageDecoder_setCrop(decoder, validCrop);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
        size_t actualStride = AImageDecoder_getMinimumStride(decoder);
        size_t expectedStride = bytesPerPixel(format) * (validCrop.right - validCrop.left);
        ASSERT_EQ(expectedStride, actualStride);
    }

    newWidth = width * 2;
    newHeight = height * 2;
    result = AImageDecoder_setTargetSize(decoder, newWidth, newHeight);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

    for (ARect validCrop : std::initializer_list<ARect> {
            { width, 0, newWidth, height },
            { 0, height * 3 / 4, width * 4 / 3, height }
    }) {
        int result = AImageDecoder_setCrop(decoder, validCrop);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
        size_t actualStride = AImageDecoder_getMinimumStride(decoder);
        size_t expectedStride = bytesPerPixel(format) * (validCrop.right - validCrop.left);
        ASSERT_EQ(expectedStride, actualStride);
    }

    // Reset crop and target size, so that we can verify that setting a crop and
    // then setting target size that will not support the crop fails.
    result = AImageDecoder_setCrop(decoder, { 0, 0, 0, 0 });
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
    result = AImageDecoder_setTargetSize(decoder, width, height);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
    ASSERT_EQ(defaultStride, AImageDecoder_getMinimumStride(decoder));

    ARect crop{ width / 2, height / 2, width, height };
    result = AImageDecoder_setCrop(decoder, crop);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
    const size_t croppedStride = AImageDecoder_getMinimumStride(decoder);
    {
        size_t expectedStride = bytesPerPixel(format) * (crop.right - crop.left);
        ASSERT_EQ(expectedStride, croppedStride);
    }
    result = AImageDecoder_setTargetSize(decoder, width / 2, height / 2);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_INVALID_SCALE, result);
    ASSERT_EQ(croppedStride, AImageDecoder_getMinimumStride(decoder));
}

static void testDecodeCrop(JNIEnv* env, jclass, jlong imageDecoderPtr,
                           jobject jbitmap, jint targetWidth, jint targetHeight,
                           jint left, jint top, jint right, jint bottom) {
    AImageDecoder* decoder = reinterpret_cast<AImageDecoder*>(imageDecoderPtr);
    DecoderDeleter decoderDeleter(decoder, AImageDecoder_delete);

    int result;
    if (targetWidth != 0 && targetHeight != 0) {
        result = AImageDecoder_setTargetSize(decoder, targetWidth, targetHeight);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
    }

    ARect rect { left, top, right, bottom };
    result = AImageDecoder_setCrop(decoder, rect);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

    const int32_t width = right - left;
    const int32_t height = bottom - top;
    size_t minStride = AImageDecoder_getMinimumStride(decoder);
    size_t size = minStride * height;
    void* pixels = malloc(size);

    {
        // Try some invalid parameters.
        result = AImageDecoder_decodeImage(decoder, nullptr, minStride, size);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);

        result = AImageDecoder_decodeImage(decoder, pixels, minStride - 1, size);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);

        result = AImageDecoder_decodeImage(decoder, pixels, minStride, size - minStride);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_BAD_PARAMETER, result);
    }

    result = AImageDecoder_decodeImage(decoder, pixels, minStride, size);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder);
    ASSERT_NE(info, nullptr);

    ASSERT_NE(jbitmap, nullptr);
    ASSERT_TRUE(bitmapsEqual(env, jbitmap, AImageDecoderHeaderInfo_getAndroidBitmapFormat(info),
                             width, height, AImageDecoderHeaderInfo_getAlphaFlags(info),
                             minStride, pixels, minStride));

    // Verify that larger strides still behave as expected.
    for (size_t stride : { minStride * 2, minStride * 3 }) {
        size_t size = stride * (height - 1) + minStride;
        void* decodePixels = malloc(size);
        result = AImageDecoder_decodeImage(decoder, decodePixels, stride, size);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

        ASSERT_TRUE(bitmapsEqual(minStride, height, pixels, minStride, decodePixels, stride));
        free(decodePixels);
    }

    free(pixels);
}

static void testScalePlusUnpremul(JNIEnv* env, jclass, jlong imageDecoderPtr) {
    AImageDecoder* decoder = reinterpret_cast<AImageDecoder*>(imageDecoderPtr);
    DecoderDeleter decoderDeleter(decoder, AImageDecoder_delete);

    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder);
    ASSERT_NE(nullptr, info);

    const int32_t width = AImageDecoderHeaderInfo_getWidth(info);
    const int32_t height = AImageDecoderHeaderInfo_getHeight(info);
    const int alpha = AImageDecoderHeaderInfo_getAlphaFlags(info);

    if (alpha == ANDROID_BITMAP_FLAGS_ALPHA_OPAQUE) {
        // Set unpremul, then scale. This succeeds for an opaque image.
        int result = AImageDecoder_setUnpremultipliedRequired(decoder, true);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

        result = AImageDecoder_setTargetSize(decoder, width * 2, height * 2);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

        result = AImageDecoder_setTargetSize(decoder, width * 2/3, height * 2/3);
        if (width * 2/3 == 0 || height * 2/3 == 0) {
            // The image that is 1x1 cannot be downscaled.
            ASSERT_EQ(ANDROID_IMAGE_DECODER_INVALID_SCALE, result);
        } else {
            ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
        }

        // Reset to the original settings to test the other order.
        result = AImageDecoder_setUnpremultipliedRequired(decoder, false);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

        result = AImageDecoder_setTargetSize(decoder, width, height);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

        // Specify scale and then unpremul.
        if (width * 2/3 == 0 || height * 2/3 == 0) {
            // The image that is 1x1 cannot be downscaled. Scale up instead.
            result = AImageDecoder_setTargetSize(decoder, width * 2, height * 2);
        } else {
            result = AImageDecoder_setTargetSize(decoder, width * 2/3, height * 2/3);
        }
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

        result = AImageDecoder_setUnpremultipliedRequired(decoder, true);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
    } else {
        // Use unpremul and then scale. Setting to unpremul is successful, but
        // later calls to change the scale fail.
        int result = AImageDecoder_setUnpremultipliedRequired(decoder, true);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

        result = AImageDecoder_setTargetSize(decoder, width * 2, height * 2);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_INVALID_SCALE, result);

        result = AImageDecoder_setTargetSize(decoder, width * 2/3, height * 2/3);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_INVALID_SCALE, result);

        // Set back to premul to verify that the opposite order also fails.
        result = AImageDecoder_setUnpremultipliedRequired(decoder, false);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

        result = AImageDecoder_setTargetSize(decoder, width * 2, height * 2);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
        result = AImageDecoder_setUnpremultipliedRequired(decoder, true);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_INVALID_CONVERSION, result);

        result = AImageDecoder_setTargetSize(decoder, width * 2/3, height * 2/3);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);
        result = AImageDecoder_setUnpremultipliedRequired(decoder, true);
        ASSERT_EQ(ANDROID_IMAGE_DECODER_INVALID_CONVERSION, result);
    }
}

static void testDecodeSetDataSpace(JNIEnv* env, jclass, jlong imageDecoderPtr,
                                   jobject jbitmap, jint dataSpace) {
    AImageDecoder* decoder = reinterpret_cast<AImageDecoder*>(imageDecoderPtr);
    DecoderDeleter decoderDeleter(decoder, AImageDecoder_delete);

    ASSERT_EQ(dataSpace, AndroidBitmap_getDataSpace(env, jbitmap));

    int result = AImageDecoder_setDataSpace(decoder, dataSpace);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

    AndroidBitmapInfo jInfo;
    int bitmapResult = AndroidBitmap_getInfo(env, jbitmap, &jInfo);
    ASSERT_EQ(ANDROID_BITMAP_RESULT_SUCCESS, bitmapResult);

    result = AImageDecoder_setAndroidBitmapFormat(decoder, jInfo.format);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder);
    ASSERT_NE(info, nullptr);

    const int32_t width = AImageDecoderHeaderInfo_getWidth(info);
    const int32_t height = AImageDecoderHeaderInfo_getHeight(info);
    int alphaFlags = AImageDecoderHeaderInfo_getAlphaFlags(info);

    size_t minStride = AImageDecoder_getMinimumStride(decoder);
    size_t size = minStride * height;
    void* pixels = malloc(size);

    result = AImageDecoder_decodeImage(decoder, pixels, minStride, size);
    ASSERT_EQ(ANDROID_IMAGE_DECODER_SUCCESS, result);

    ASSERT_TRUE(bitmapsEqual(env, jbitmap, (AndroidBitmapFormat) jInfo.format,
                             width, height, alphaFlags, minStride, pixels, minStride));
    free(pixels);
}

#define ASSET_MANAGER "Landroid/content/res/AssetManager;"
#define STRING "Ljava/lang/String;"
#define BITMAP "Landroid/graphics/Bitmap;"

static JNINativeMethod gMethods[] = {
    { "nTestEmptyCreate", "()V", (void*) testEmptyCreate },
    { "nTestNullDecoder", "(" ASSET_MANAGER STRING ")V", (void*) testNullDecoder },
    { "nTestInfo", "(JII" STRING "ZI)V", (void*) testInfo },
    { "nOpenAsset", "(" ASSET_MANAGER STRING ")J", (void*) openAssetNative },
    { "nCloseAsset", "(J)V", (void*) closeAsset },
    { "nCreateFromAsset", "(J)J", (void*) createFromAsset },
    { "nCreateFromAssetFd", "(J)J", (void*) createFromAssetFd },
    { "nCreateFromAssetBuffer", "(J)J", (void*) createFromAssetBuffer },
    { "nCreateFromFd", "(I)J", (void*) createFromFd },
    { "nTestCreateIncomplete", "(" ASSET_MANAGER STRING "I)V", (void*) testCreateIncomplete },
    { "nTestCreateUnsupported", "(" ASSET_MANAGER STRING ")V", (void*) testCreateUnsupported },
    { "nTestSetFormat", "(JZZ)V", (void*) testSetFormat },
    { "nTestSetUnpremul", "(JZ)V", (void*) testSetUnpremul },
    { "nTestGetMinimumStride", "(JZZ)V", (void*) testGetMinimumStride },
    { "nTestDecode", "(JIZ" BITMAP ")V", (void*) testDecode },
    { "nTestDecodeStride", "(J)V", (void*) testDecodeStride },
    { "nTestSetTargetSize", "(J)V", (void*) testSetTargetSize },
    { "nTestComputeSampledSize", "(J" BITMAP "I)V", (void*) testComputeSampledSize },
    { "nTestDecodeScaled", "(J" BITMAP ")V", (void*) testDecodeScaled },
    { "nTestSetCrop", "(" ASSET_MANAGER STRING ")V", (void*) testSetCrop },
    { "nTestDecodeCrop", "(J" BITMAP "IIIIII)V", (void*) testDecodeCrop },
    { "nTestScalePlusUnpremul", "(J)V", (void*) testScalePlusUnpremul },
    { "nTestDecode", "(J" BITMAP "I)V", (void*) testDecodeSetDataSpace },
};

int register_android_graphics_cts_AImageDecoderTest(JNIEnv* env) {
    jclass clazz = env->FindClass("android/graphics/cts/AImageDecoderTest");
    return env->RegisterNatives(clazz, gMethods,
            sizeof(gMethods) / sizeof(JNINativeMethod));
}

