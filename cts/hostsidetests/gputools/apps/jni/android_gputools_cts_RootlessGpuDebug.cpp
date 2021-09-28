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

#define LOG_TAG "RootlessGpuDebug"

#include <string>
#include <vector>

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <android/native_window.h>
#include <jni.h>
#include <vulkan/vulkan.h>

#define ALOGI(msg, ...) \
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, (msg), __VA_ARGS__)
#define ALOGE(msg, ...) \
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, (msg), __VA_ARGS__)
#define ALOGD(msg, ...) \
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, (msg), __VA_ARGS__)

namespace {

typedef __eglMustCastToProperFunctionPointerType EGLFuncPointer;

std::string initVulkan() {
    std::string result = "";

    {
      uint32_t count = 0;
      vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
      if (count > 0) {
        std::vector<VkExtensionProperties> properties(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count,
                                               properties.data());
        for (uint32_t i = 0; i < count; ++i) {
          if (!strcmp("VK_EXT_debug_utils", properties[i].extensionName)) {
            ALOGI("VK_EXT_debug_utils: %u", properties[i].specVersion);
            break;
          }
        }
      }
    }

    const VkApplicationInfo app_info = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO,
        nullptr,             // pNext
        "RootlessGpuDebug",  // app name
        0,                   // app version
        nullptr,             // engine name
        0,                   // engine version
        VK_API_VERSION_1_0,
    };
    const VkInstanceCreateInfo instance_info = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        nullptr,  // pNext
        0,        // flags
        &app_info,
        0,        // layer count
        nullptr,  // layers
        0,        // extension count
        nullptr,  // extensions
    };
    VkInstance instance;
    VkResult vkResult = vkCreateInstance(&instance_info, nullptr, &instance);
    if (vkResult == VK_ERROR_INITIALIZATION_FAILED) {
        result = "vkCreateInstance failed, meaning layers could not be chained.";
    } else {
        result = "vkCreateInstance succeeded.";
    }

    return result;
}

std::string initGLES() {
    std::string result = "";

    const EGLint attribs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                              EGL_BLUE_SIZE,    8,
                              EGL_GREEN_SIZE,   8,
                              EGL_RED_SIZE,     8,
                              EGL_NONE};

    // Create an EGL context
    EGLDisplay display;
    EGLConfig config;
    EGLint numConfigs;
    EGLint format;

    // Check for the EGL_ANDROID_GLES_layers
    std::string display_extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (display_extensions.find("EGL_ANDROID_GLES_layers") == std::string::npos)
    {
        result = "Did not find EGL_ANDROID_GLES_layers extension";
        return result;
    }

    if ((display = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
        result = "eglGetDisplay() returned error " + std::to_string(eglGetError());
        return result;
    }

    if (!eglInitialize(display, 0, 0)) {
        result = "eglInitialize() returned error " + std::to_string(eglGetError());
        return result;
    }

    if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs)) {
        result =
            "eglChooseConfig() returned error " + std::to_string(eglGetError());
        return result;
    }

    if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format)) {
        result =
            "eglGetConfigAttrib() returned error " + std::to_string(eglGetError());
        return result;
    }

    eglTerminate(display);

    return result;
}

jstring android_gputools_cts_RootlessGpuDebug_nativeInitVulkan(
        JNIEnv* env, jclass /*clazz*/) {
    std::string result;

    result = initVulkan();

    return env->NewStringUTF(result.c_str());
}

jstring android_gputools_cts_RootlessGpuDebug_nativeInitGLES(JNIEnv* env,
                                                             jclass /*clazz*/) {
    std::string result;

    result = initGLES();

    return env->NewStringUTF(result.c_str());
}

static JNINativeMethod gMethods[] = {
    {"nativeInitVulkan", "()Ljava/lang/String;",
     (void*)android_gputools_cts_RootlessGpuDebug_nativeInitVulkan},
    {"nativeInitGLES", "()Ljava/lang/String;",
     (void*)android_gputools_cts_RootlessGpuDebug_nativeInitGLES}};
}  // anonymous namespace

int register_android_gputools_cts_RootlessGpuDebug(JNIEnv* env) {
    jclass clazz = env->FindClass(
        "android/rootlessgpudebug/app/RootlessGpuDebugDeviceActivity");
    return env->RegisterNatives(clazz, gMethods,
                                sizeof(gMethods) / sizeof(JNINativeMethod));
}
