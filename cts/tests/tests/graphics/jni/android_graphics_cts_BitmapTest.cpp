/*
 * Copyright 2017 The Android Open Source Project
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

#define LOG_TAG "BitmapTest"

#include <jni.h>
#include <android/bitmap.h>
#include <android/data_space.h>
#include <android/hardware_buffer.h>
#include <android/hardware_buffer_jni.h>

#include "NativeTestHelpers.h"

#include <initializer_list>
#include <limits>

static jclass gOutputStream_class;
static jmethodID gOutputStream_writeMethodID;

static void validateBitmapInfo(JNIEnv* env, jclass, jobject jbitmap, jint width, jint height,
        jboolean is565) {
    AndroidBitmapInfo info;
    int err = 0;
    err = AndroidBitmap_getInfo(env, jbitmap, &info);
    ASSERT_EQ(ANDROID_BITMAP_RESULT_SUCCESS, err);
    ASSERT_TRUE(width >= 0 && height >= 0);
    ASSERT_EQ((uint32_t) width, info.width);
    ASSERT_EQ((uint32_t) height, info.height);
    int32_t format = is565 ? ANDROID_BITMAP_FORMAT_RGB_565 : ANDROID_BITMAP_FORMAT_RGBA_8888;
    ASSERT_EQ(format, info.format);
}

static void validateNdkAccessFails(JNIEnv* env, jclass, jobject jbitmap) {
    void* pixels = nullptr;
    int err = AndroidBitmap_lockPixels(env, jbitmap, &pixels);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_JNI_EXCEPTION);

    int32_t dataSpace = AndroidBitmap_getDataSpace(env, jbitmap);
    ASSERT_EQ(ADATASPACE_UNKNOWN, dataSpace);

    AHardwareBuffer* buffer;
    err = AndroidBitmap_getHardwareBuffer(env, jbitmap, &buffer);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_JNI_EXCEPTION);
}

static void fillRgbaHardwareBuffer(JNIEnv* env, jclass, jobject hwBuffer) {
    AHardwareBuffer* hardware_buffer = AHardwareBuffer_fromHardwareBuffer(env, hwBuffer);
    AHardwareBuffer_Desc description;
    AHardwareBuffer_describe(hardware_buffer, &description);
    ASSERT_EQ(AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM, description.format);

    uint8_t* rgbaBytes;
    AHardwareBuffer_lock(hardware_buffer,
                         AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY,
                         -1,
                         nullptr,
                         reinterpret_cast<void**>(&rgbaBytes));
    int c = 0;
    for (int y = 0; y < description.width; ++y) {
        for (int x = 0; x < description.height; ++x) {
            rgbaBytes[c++] = static_cast<uint8_t>(x % 255);
            rgbaBytes[c++] = static_cast<uint8_t>(y % 255);
            rgbaBytes[c++] = 42;
            rgbaBytes[c++] = 255;
        }
    }
    AHardwareBuffer_unlock(hardware_buffer, nullptr);
}

static jint getFormat(JNIEnv* env, jclass, jobject jbitmap) {
    AndroidBitmapInfo info;
    info.format = ANDROID_BITMAP_FORMAT_NONE;
    int err = 0;
    err = AndroidBitmap_getInfo(env, jbitmap, &info);
    if (err != ANDROID_BITMAP_RESULT_SUCCESS) {
        fail(env, "AndroidBitmap_getInfo failed, err=%d", err);
        return ANDROID_BITMAP_FORMAT_NONE;
    }
    return info.format;
}

static void testNullBitmap(JNIEnv* env, jclass, jobject jbitmap) {
    ASSERT_NE(nullptr, env);
    AndroidBitmapInfo info;
    int err = AndroidBitmap_getInfo(env, nullptr, &info);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);

    err = AndroidBitmap_getInfo(env, jbitmap, nullptr);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_SUCCESS);

    err = AndroidBitmap_getInfo(nullptr, jbitmap, &info);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);

    void* pixels = nullptr;
    err = AndroidBitmap_lockPixels(env, nullptr, &pixels);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);

    err = AndroidBitmap_lockPixels(env, jbitmap, nullptr);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_SUCCESS);

    err = AndroidBitmap_lockPixels(nullptr, jbitmap, &pixels);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);

    err = AndroidBitmap_unlockPixels(env, nullptr);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);

    err = AndroidBitmap_unlockPixels(nullptr, jbitmap);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);

    int32_t dataSpace = AndroidBitmap_getDataSpace(env, nullptr);
    ASSERT_EQ(dataSpace, ADATASPACE_UNKNOWN);

    dataSpace = AndroidBitmap_getDataSpace(nullptr, jbitmap);
    ASSERT_EQ(dataSpace, ADATASPACE_UNKNOWN);

    err = AndroidBitmap_getHardwareBuffer(env, jbitmap, nullptr);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);

    AHardwareBuffer* buffer;
    err = AndroidBitmap_getHardwareBuffer(env, nullptr, &buffer);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);

    err = AndroidBitmap_getHardwareBuffer(nullptr, jbitmap, &buffer);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);
}

static void testInfo(JNIEnv* env, jclass, jobject jbitmap, jint androidBitmapFormat,
                     jint width, jint height, jboolean hasAlpha, jboolean premultiplied,
                     jboolean hardware) {
    AndroidBitmapInfo info;
    int err = AndroidBitmap_getInfo(env, jbitmap, &info);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_SUCCESS);

    ASSERT_EQ(androidBitmapFormat, info.format);
    ASSERT_EQ(width, info.width);
    ASSERT_EQ(height, info.height);

    int ndkAlpha = (info.flags << ANDROID_BITMAP_FLAGS_ALPHA_SHIFT)
            & ANDROID_BITMAP_FLAGS_ALPHA_MASK;
    if (!hasAlpha) {
        ASSERT_EQ(ndkAlpha, ANDROID_BITMAP_FLAGS_ALPHA_OPAQUE);
    } else if (premultiplied) {
        ASSERT_EQ(ndkAlpha, ANDROID_BITMAP_FLAGS_ALPHA_PREMUL);
    } else {
        ASSERT_EQ(ndkAlpha, ANDROID_BITMAP_FLAGS_ALPHA_UNPREMUL);
    }

    bool ndkHardware = info.flags & ANDROID_BITMAP_FLAGS_IS_HARDWARE;
    ASSERT_EQ(ndkHardware, hardware);

    AHardwareBuffer* buffer;
    err = AndroidBitmap_getHardwareBuffer(env, jbitmap, &buffer);
    if (hardware) {
        ASSERT_EQ(err, ANDROID_BITMAP_RESULT_SUCCESS);
        ASSERT_NE(buffer, nullptr);
        AHardwareBuffer_release(buffer);
    } else {
        ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);
        ASSERT_EQ(buffer, nullptr);
    }

    void* pixels = nullptr;
    err = AndroidBitmap_lockPixels(env, jbitmap, &pixels);
    if (hardware) {
        ASSERT_EQ(err, ANDROID_BITMAP_RESULT_JNI_EXCEPTION);
    } else {
        ASSERT_EQ(err, ANDROID_BITMAP_RESULT_SUCCESS);
        err = AndroidBitmap_unlockPixels(env, jbitmap);
        ASSERT_EQ(err, ANDROID_BITMAP_RESULT_SUCCESS);
    }
}

static jint getDataSpace(JNIEnv* env, jclass, jobject jbitmap) {
    return AndroidBitmap_getDataSpace(env, jbitmap);
}

/**
 * Class for writing to a java.io.OutputStream. Passed to
 * AndroidBitmap_compress. This is modelled after SkJavaOutputStream.
 */
class Context {
public:
    Context(JNIEnv* env, jobject outputStream, jbyteArray storage)
        : mEnv(env)
        , mOutputStream(outputStream)
        , mStorage(storage)
        , mCapacity(mEnv->GetArrayLength(mStorage))
  {}

    bool compress(const void* data, size_t size) {
        while (size > 0) {
            jint requested = 0;
            if (size > static_cast<size_t>(mCapacity)) {
                requested = mCapacity;
            } else {
                // This is safe because |requested| is clamped to (jint) mCapacity.
                requested = static_cast<jint>(size);
            }

            mEnv->SetByteArrayRegion(mStorage, 0, requested,
                                     reinterpret_cast<const jbyte*>(data));
            if (mEnv->ExceptionCheck()) {
                mEnv->ExceptionDescribe();
                mEnv->ExceptionClear();
                ALOGE("SetByteArrayRegion threw an exception!");
                return false;
            }

            mEnv->CallVoidMethod(mOutputStream, gOutputStream_writeMethodID, mStorage, 0,
                                 requested);
            if (mEnv->ExceptionCheck()) {
                mEnv->ExceptionDescribe();
                mEnv->ExceptionClear();
                ALOGE("write threw an exception!");
                return false;
            }

            data = (void*)((char*) data + requested);
            size -= requested;
        }
        return true;
    }

private:
    JNIEnv* mEnv;
    jobject mOutputStream;
    jbyteArray mStorage;
    jint mCapacity;

    Context(const Context& other) = delete;
    Context& operator=(const Context& other) = delete;
};

static bool compressWriteFn(void* userContext, const void* data, size_t size) {
    Context* c = reinterpret_cast<Context*>(userContext);
    return c->compress(data, size);
}

#define EXPECT_EQ(msg, a, b)    \
    if ((a) != (b)) {           \
        ALOGE(msg);             \
        return false;           \
    }

static jboolean compress(JNIEnv* env, jclass, jobject jbitmap, jint format, jint quality,
                         jobject joutputStream, jbyteArray jstorage) {
    AndroidBitmapInfo info;
    int err = AndroidBitmap_getInfo(env, jbitmap, &info);
    EXPECT_EQ("Failed to getInfo!", err, ANDROID_BITMAP_RESULT_SUCCESS);

    void* pixels = nullptr;
    err = AndroidBitmap_lockPixels(env, jbitmap, &pixels);
    EXPECT_EQ("Failed to lockPixels!", err, ANDROID_BITMAP_RESULT_SUCCESS);

    Context context(env, joutputStream, jstorage);
    err = AndroidBitmap_compress(&info, AndroidBitmap_getDataSpace(env, jbitmap), pixels, format,
                                 quality, &context, compressWriteFn);
    if (AndroidBitmap_unlockPixels(env, jbitmap) != ANDROID_BITMAP_RESULT_SUCCESS) {
        fail(env, "Failed to unlock pixels!");
    }
    return err == ANDROID_BITMAP_RESULT_SUCCESS;
}

constexpr AndroidBitmapCompressFormat gFormats[] = {
    ANDROID_BITMAP_COMPRESS_FORMAT_JPEG,
    ANDROID_BITMAP_COMPRESS_FORMAT_PNG,
    ANDROID_BITMAP_COMPRESS_FORMAT_WEBP_LOSSY,
    ANDROID_BITMAP_COMPRESS_FORMAT_WEBP_LOSSLESS,
};

constexpr auto kMaxInt32 = std::numeric_limits<int32_t>::max();

/**
 * Class for generating invalid AndroidBitmapInfos based on a valid one.
 */
class BadInfoGenerator {
public:
    BadInfoGenerator(const AndroidBitmapInfo& info)
        : mOriginalInfo(info)
        , mIter(0)
    {}

    /**
     * Each call to this method will return a pointer to an invalid
     * AndroidBitmapInfo, until it has run out of infos to generate, when it
     * will return null.
     */
    AndroidBitmapInfo* next() {
        mBadInfo = mOriginalInfo;
        switch (mIter) {
            case 0:
                mBadInfo.width = -mBadInfo.width;
                break;
            case 1:
                mBadInfo.height = -mBadInfo.height;
                break;
            case 2:
                mBadInfo.width = - mBadInfo.width;
                break;
            case 3:
                // AndroidBitmap_compress uses ANDROID_BITMAP_FLAGS_ALPHA_MASK
                // to ignore bits outside of alpha, so the only invalid value is
                // 3.
                mBadInfo.flags = 3;
                break;
            case 4:
                mBadInfo.stride = mBadInfo.stride / 2;
                break;
            case 5:
                mBadInfo.format = ANDROID_BITMAP_FORMAT_NONE;
                break;
            case 6:
                mBadInfo.format = ANDROID_BITMAP_FORMAT_RGBA_4444;
                break;
            case 7:
                mBadInfo.format = -1;
                break;
            case 8:
                mBadInfo.format = 2;
                break;
            case 9:
                mBadInfo.format = 3;
                break;
            case 10:
                mBadInfo.format = 5;
                break;
            case 11:
                mBadInfo.format = 6;
                break;
            case 12:
                mBadInfo.format = 10;
                break;
            case 13:
                mBadInfo.width = static_cast<uint32_t>(kMaxInt32) + 1;
                mBadInfo.height = 1;
                break;
            case 14:
                mBadInfo.width = 1;
                mBadInfo.height = static_cast<uint32_t>(kMaxInt32) + 1;
                break;
            case 15:
                mBadInfo.width = 3;
                mBadInfo.height = kMaxInt32 / 2;
                break;
            default:
              return nullptr;
        }
        mIter++;
        return &mBadInfo;
    }
private:
    const AndroidBitmapInfo& mOriginalInfo;
    AndroidBitmapInfo mBadInfo;
    int mIter;

    BadInfoGenerator(const BadInfoGenerator&) = delete;
    BadInfoGenerator& operator=(const BadInfoGenerator&) = delete;
};

static void testNdkCompressBadParameter(JNIEnv* env, jclass, jobject jbitmap,
                         jobject joutputStream, jbyteArray jstorage) {
    AndroidBitmapInfo info;
    int err = AndroidBitmap_getInfo(env, jbitmap, &info);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_SUCCESS);

    void* pixels = nullptr;
    err = AndroidBitmap_lockPixels(env, jbitmap, &pixels);
    ASSERT_EQ(err, ANDROID_BITMAP_RESULT_SUCCESS);

    Context context(env, joutputStream, jstorage);
    int32_t dataSpace = AndroidBitmap_getDataSpace(env, jbitmap);

    // Bad info.
    for (auto format : gFormats) {
        err = AndroidBitmap_compress(nullptr, dataSpace, pixels, format, 100,
                                     &context, compressWriteFn);
        ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);
    }

    {
        BadInfoGenerator gen(info);
        while (const auto* badInfo = gen.next()) {
            for (auto format : gFormats) {
                err = AndroidBitmap_compress(badInfo, dataSpace, pixels, format, 100,
                                             &context, compressWriteFn);
                ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);
            }
        }
    }

    for (int32_t badDataSpace : std::initializer_list<int32_t>{ ADATASPACE_UNKNOWN, -1 }) {
        for (auto format : gFormats) {
            err = AndroidBitmap_compress(&info, badDataSpace, pixels, format, 100,
                                         &context, compressWriteFn);
            ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);
        }
    }

    // Bad pixels
    for (auto format : gFormats) {
        err = AndroidBitmap_compress(&info, dataSpace, nullptr, format, 100,
                                     &context, compressWriteFn);
        ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);
    }

    // Bad formats
    for (int32_t badFormat : { -1, 2, 5, 16 }) {
        err = AndroidBitmap_compress(&info, dataSpace, pixels, badFormat, 100,
                                     &context, compressWriteFn);
        ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);
    }

    // Bad qualities
    for (int32_t badQuality : { -1, 101, 1024 }) {
        for (auto format : gFormats) {
            err = AndroidBitmap_compress(&info, dataSpace, pixels, format, badQuality,
                                         &context, compressWriteFn);
            ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);
        }
    }

    // Missing compress function
    for (auto format : gFormats) {
        err = AndroidBitmap_compress(&info, dataSpace, pixels, format, 100,
                                     &context, nullptr);
        ASSERT_EQ(err, ANDROID_BITMAP_RESULT_BAD_PARAMETER);
    }

    // No reason to force client to have a context. (Maybe they'll use a global
    // variable?) Demonstrate that it's possible to use a null context.
    auto emptyCompress = [](void*, const void*, size_t) {
        return true;
    };
    for (auto format : gFormats) {
        err = AndroidBitmap_compress(&info, dataSpace, pixels, format, 100,
                                     nullptr, emptyCompress);
        ASSERT_EQ(err, ANDROID_BITMAP_RESULT_SUCCESS);
    }
}

static JNINativeMethod gMethods[] = {
    { "nValidateBitmapInfo", "(Landroid/graphics/Bitmap;IIZ)V",
        (void*) validateBitmapInfo },
    { "nValidateNdkAccessFails", "(Landroid/graphics/Bitmap;)V",
        (void*) validateNdkAccessFails },
    { "nFillRgbaHwBuffer", "(Landroid/hardware/HardwareBuffer;)V",
        (void*) fillRgbaHardwareBuffer },
    { "nGetFormat", "(Landroid/graphics/Bitmap;)I", (void*) getFormat },
    { "nTestNullBitmap", "(Landroid/graphics/Bitmap;)V", (void*) testNullBitmap },
    { "nTestInfo", "(Landroid/graphics/Bitmap;IIIZZZ)V", (void*) testInfo },
    { "nGetDataSpace", "(Landroid/graphics/Bitmap;)I", (void*) getDataSpace },
    { "nCompress", "(Landroid/graphics/Bitmap;IILjava/io/OutputStream;[B)Z", (void*) compress },
    { "nTestNdkCompressBadParameter", "(Landroid/graphics/Bitmap;Ljava/io/OutputStream;[B)V",
        (void*) testNdkCompressBadParameter },
};

int register_android_graphics_cts_BitmapTest(JNIEnv* env) {
    gOutputStream_class = reinterpret_cast<jclass>(env->NewGlobalRef(
            env->FindClass("java/io/OutputStream")));
    if (!gOutputStream_class) {
        ALOGE("Could not find (or create a global ref on) OutputStream!");
        return JNI_ERR;
    }
    gOutputStream_writeMethodID = env->GetMethodID(gOutputStream_class, "write", "([BII)V");
    if (!gOutputStream_writeMethodID) {
        ALOGE("Could not find method write()!");
        return JNI_ERR;
    }
    jclass clazz = env->FindClass("android/graphics/cts/BitmapTest");
    return env->RegisterNatives(clazz, gMethods,
            sizeof(gMethods) / sizeof(JNINativeMethod));
}
