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

//#define LOG_NDEBUG 0
#define LOG_TAG "audio-metadata-native"

#include <algorithm>
#include <atomic>

#include <audio_utils/Metadata.h>
#include <nativehelper/scoped_local_ref.h>
#include <nativehelper/scoped_utf_chars.h>
#include <utils/Log.h>

#include <jni.h>

using namespace android;

static jclass gByteBufferClass;
static jmethodID gByteBufferAllocateDirect;

static std::atomic<bool> gFieldsInitialized = false;

static void initializeGlobalFields(JNIEnv *env)
{
    if (gFieldsInitialized) {
        return;
    }

    ScopedLocalRef<jclass> byteBufferClass(env, env->FindClass("java/nio/ByteBuffer"));
    gByteBufferClass = (jclass) env->NewGlobalRef(byteBufferClass.get());
    gByteBufferAllocateDirect = env->GetStaticMethodID(
            gByteBufferClass, "allocateDirect", "(I)Ljava/nio/ByteBuffer;");
    gFieldsInitialized = true;
}

extern "C" jobject Java_android_media_cts_AudioMetadataTest_nativeGetByteBuffer(
    JNIEnv *env, jclass /*clazz*/, jobject javaByteBuffer, jint sizeInBytes)
{
    initializeGlobalFields(env);

    const uint8_t* bytes =
            reinterpret_cast<const uint8_t*>(env->GetDirectBufferAddress(javaByteBuffer));
    if (bytes == nullptr) {
        ALOGE("Cannot get byte array");
        return nullptr;
    }
    audio_utils::metadata::Data d = audio_utils::metadata::dataFromByteString(
            audio_utils::metadata::ByteString(bytes, sizeInBytes));

    audio_utils::metadata::ByteString bs = byteStringFromData(d);

    jobject byteBuffer = env->CallStaticObjectMethod(
            gByteBufferClass, gByteBufferAllocateDirect, (jint) bs.size());
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    if (byteBuffer == nullptr) {
        ALOGE("Failed to allocate byte buffer");
        return nullptr;
    }

    uint8_t* byteBufferAddr = (uint8_t*)env->GetDirectBufferAddress(byteBuffer);
    std::copy(bs.begin(), bs.end(), byteBufferAddr);
    return byteBuffer;
}
