/*
 * Copyright 2018 The Android Open Source Project
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

#define LOG_TAG "VulkanSurfaceSupportTest"

#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif

#include <android/log.h>
#include <jni.h>
#include <array>

#include "NativeTestHelpers.h"
#include "VulkanPreTransformTestHelpers.h"

#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

void createNativeTest(JNIEnv* env, jclass /*clazz*/, jobject jAssetManager, jobject jSurface,
                      jboolean jSupported) {
    ASSERT(jAssetManager, "jAssetManager is NULL");
    ASSERT(jSurface, "jSurface is NULL");

    DeviceInfo deviceInfo;
    VkTestResult ret = deviceInfo.init(env, jSurface);
    if (ret == VK_TEST_PHYSICAL_DEVICE_NOT_EXISTED) {
        ALOGD("Hardware not supported for this test");
        return;
    }
    if (ret == VK_TEST_SURFACE_FORMAT_NOT_SUPPORTED) {
        ASSERT(jSupported == false, "Surface format should not be supported");
        return;
    }
    ASSERT(ret == VK_TEST_SUCCESS, "Failed to initialize Vulkan device");
    ASSERT(jSupported == true, "Surface format should be supported");

    SwapchainInfo swapchainInfo(&deviceInfo);
    ASSERT(swapchainInfo.init(false, nullptr) == VK_TEST_SUCCESS,
           "Failed to initialize Vulkan swapchain");

    Renderer renderer(&deviceInfo, &swapchainInfo);
    ASSERT(renderer.init(env, jAssetManager) == VK_TEST_SUCCESS,
           "Failed to initialize Vulkan renderer");

    for (uint32_t i = 0; i < 3; ++i) {
        ASSERT(renderer.drawFrame() == VK_TEST_SUCCESS, "Failed to draw frame");
    }
}

const std::array<JNINativeMethod, 1> JNI_METHODS = {{
        {"nCreateNativeTest", "(Landroid/content/res/AssetManager;Landroid/view/Surface;Z)V",
         (void*)createNativeTest},
}};

} // anonymous namespace

int register_android_graphics_cts_VulkanSurfaceSupportTest(JNIEnv* env) {
    jclass clazz = env->FindClass("android/graphics/cts/VulkanSurfaceSupportTest");
    return env->RegisterNatives(clazz, JNI_METHODS.data(), JNI_METHODS.size());
}
