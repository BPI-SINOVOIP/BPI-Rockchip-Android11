/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "DisplayViewport-JNI"

#include <nativehelper/JNIHelp.h>
#include "core_jni_helpers.h"

#include <android_hardware_display_DisplayViewport.h>
#include <android_runtime/AndroidRuntime.h>
#include <android_runtime/Log.h>
#include <utils/Log.h>

#include <nativehelper/ScopedUtfChars.h>

namespace android {

// ----------------------------------------------------------------------------

static struct {
    jclass clazz;

    jfieldID displayId;
    jfieldID isActive;
    jfieldID orientation;
    jfieldID logicalFrame;
    jfieldID physicalFrame;
    jfieldID deviceWidth;
    jfieldID deviceHeight;
    jfieldID uniqueId;
    jfieldID physicalPort;
    jfieldID type;
} gDisplayViewportClassInfo;

static struct {
    jfieldID left;
    jfieldID top;
    jfieldID right;
    jfieldID bottom;
} gRectClassInfo;

// ----------------------------------------------------------------------------

status_t android_hardware_display_DisplayViewport_toNative(JNIEnv* env, jobject viewportObj,
        DisplayViewport* viewport) {
    static const jclass byteClass = FindClassOrDie(env, "java/lang/Byte");
    static const jmethodID byteValue = env->GetMethodID(byteClass, "byteValue", "()B");

    viewport->displayId = env->GetIntField(viewportObj, gDisplayViewportClassInfo.displayId);
    viewport->isActive = env->GetBooleanField(viewportObj, gDisplayViewportClassInfo.isActive);
    viewport->orientation = env->GetIntField(viewportObj, gDisplayViewportClassInfo.orientation);
    viewport->deviceWidth = env->GetIntField(viewportObj, gDisplayViewportClassInfo.deviceWidth);
    viewport->deviceHeight = env->GetIntField(viewportObj, gDisplayViewportClassInfo.deviceHeight);

    jstring uniqueId =
            jstring(env->GetObjectField(viewportObj, gDisplayViewportClassInfo.uniqueId));
    if (uniqueId != nullptr) {
        viewport->uniqueId = ScopedUtfChars(env, uniqueId).c_str();
    }

    viewport->physicalPort = std::nullopt;
    jobject physicalPort = env->GetObjectField(viewportObj, gDisplayViewportClassInfo.physicalPort);
    if (physicalPort != nullptr) {
        viewport->physicalPort = std::make_optional(env->CallByteMethod(physicalPort, byteValue));
    }

    viewport->type = static_cast<ViewportType>(env->GetIntField(viewportObj,
                gDisplayViewportClassInfo.type));

    jobject logicalFrameObj =
            env->GetObjectField(viewportObj, gDisplayViewportClassInfo.logicalFrame);
    viewport->logicalLeft = env->GetIntField(logicalFrameObj, gRectClassInfo.left);
    viewport->logicalTop = env->GetIntField(logicalFrameObj, gRectClassInfo.top);
    viewport->logicalRight = env->GetIntField(logicalFrameObj, gRectClassInfo.right);
    viewport->logicalBottom = env->GetIntField(logicalFrameObj, gRectClassInfo.bottom);

    jobject physicalFrameObj =
            env->GetObjectField(viewportObj, gDisplayViewportClassInfo.physicalFrame);
    viewport->physicalLeft = env->GetIntField(physicalFrameObj, gRectClassInfo.left);
    viewport->physicalTop = env->GetIntField(physicalFrameObj, gRectClassInfo.top);
    viewport->physicalRight = env->GetIntField(physicalFrameObj, gRectClassInfo.right);
    viewport->physicalBottom = env->GetIntField(physicalFrameObj, gRectClassInfo.bottom);

    return OK;
}

// ----------------------------------------------------------------------------

int register_android_hardware_display_DisplayViewport(JNIEnv* env) {
    jclass clazz = FindClassOrDie(env, "android/hardware/display/DisplayViewport");
    gDisplayViewportClassInfo.clazz = MakeGlobalRefOrDie(env, clazz);

    gDisplayViewportClassInfo.displayId = GetFieldIDOrDie(env,
            gDisplayViewportClassInfo.clazz, "displayId", "I");

    gDisplayViewportClassInfo.isActive =
            GetFieldIDOrDie(env, gDisplayViewportClassInfo.clazz, "isActive", "Z");

    gDisplayViewportClassInfo.orientation = GetFieldIDOrDie(env,
            gDisplayViewportClassInfo.clazz, "orientation", "I");

    gDisplayViewportClassInfo.deviceWidth = GetFieldIDOrDie(env,
            gDisplayViewportClassInfo.clazz, "deviceWidth", "I");

    gDisplayViewportClassInfo.deviceHeight = GetFieldIDOrDie(env,
            gDisplayViewportClassInfo.clazz, "deviceHeight", "I");

    gDisplayViewportClassInfo.logicalFrame = GetFieldIDOrDie(env,
            gDisplayViewportClassInfo.clazz, "logicalFrame", "Landroid/graphics/Rect;");

    gDisplayViewportClassInfo.physicalFrame = GetFieldIDOrDie(env,
            gDisplayViewportClassInfo.clazz, "physicalFrame", "Landroid/graphics/Rect;");

    gDisplayViewportClassInfo.uniqueId = GetFieldIDOrDie(env,
            gDisplayViewportClassInfo.clazz, "uniqueId", "Ljava/lang/String;");

    gDisplayViewportClassInfo.physicalPort = GetFieldIDOrDie(env,
            gDisplayViewportClassInfo.clazz, "physicalPort", "Ljava/lang/Byte;");

    gDisplayViewportClassInfo.type = GetFieldIDOrDie(env,
            gDisplayViewportClassInfo.clazz, "type", "I");

    clazz = FindClassOrDie(env, "android/graphics/Rect");
    gRectClassInfo.left = GetFieldIDOrDie(env, clazz, "left", "I");
    gRectClassInfo.top = GetFieldIDOrDie(env, clazz, "top", "I");
    gRectClassInfo.right = GetFieldIDOrDie(env, clazz, "right", "I");
    gRectClassInfo.bottom = GetFieldIDOrDie(env, clazz, "bottom", "I");

    return 0;
}

} // namespace android
