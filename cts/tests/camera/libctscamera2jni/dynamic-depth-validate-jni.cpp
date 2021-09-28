/*
 * Copyright (C) 2019 The Android Open Source Project
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

#define LOG_TAG "DYNAMIC-DEPTH-JNI"
#include <jni.h>
#include <log/log.h>
#include <dynamic_depth/depth_jpeg.h>

using namespace dynamic_depth;

extern "C" jboolean
Java_android_hardware_camera2_cts_ImageReaderTest_validateDynamicDepthNative(
        JNIEnv* env, jclass /*clazz*/, jbyteArray dynamicDepthBuffer) {

    jbyte* buffer = env->GetByteArrayElements(dynamicDepthBuffer, NULL);
    jsize bufferLength = env->GetArrayLength(dynamicDepthBuffer);
    if (buffer == nullptr) {
        ALOGE("Unable to map dynamic depth buffer to native");
        return JNI_FALSE;
    }

    auto ret = (ValidateAndroidDynamicDepthBuffer(reinterpret_cast<const char *> (buffer),
                bufferLength) == 0) ? JNI_TRUE : JNI_FALSE;

    return ret;
}
