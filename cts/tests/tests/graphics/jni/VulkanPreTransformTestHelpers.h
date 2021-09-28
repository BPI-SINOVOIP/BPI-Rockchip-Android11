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
#ifndef VULKAN_PRE_TRANSFORM_TEST_HELPERS_H
#define VULKAN_PRE_TRANSFORM_TEST_HELPERS_H

#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <vulkan/vulkan.h>
#include <vector>

typedef enum VkTestResult {
    VK_TEST_ERROR = -1,
    VK_TEST_SUCCESS = 0,
    VK_TEST_PHYSICAL_DEVICE_NOT_EXISTED = 1,
    VK_TEST_SURFACE_FORMAT_NOT_SUPPORTED = 2,
    VK_TEST_SUCCESS_SUBOPTIMAL = 3,
} VkTestResult;

class DeviceInfo {
public:
    DeviceInfo();
    ~DeviceInfo();
    VkTestResult init(JNIEnv* env, jobject jSurface);
    VkPhysicalDevice gpu() const { return mGpu; }
    VkSurfaceKHR surface() const { return mSurface; }
    uint32_t queueFamilyIndex() const { return mQueueFamilyIndex; }
    VkDevice device() const { return mDevice; }
    VkQueue queue() const { return mQueue; }

private:
    VkInstance mInstance;
    VkPhysicalDevice mGpu;
    ANativeWindow* mWindow;
    VkSurfaceKHR mSurface;
    uint32_t mQueueFamilyIndex;
    VkDevice mDevice;
    VkQueue mQueue;
};

class SwapchainInfo {
public:
    SwapchainInfo(const DeviceInfo* const deviceInfo);
    ~SwapchainInfo();
    VkTestResult init(bool setPreTransform, int* outPreTransformHint);
    VkFormat format() const { return mFormat; }
    VkExtent2D displaySize() const { return mDisplaySize; }
    VkSwapchainKHR swapchain() const { return mSwapchain; }
    uint32_t swapchainLength() const { return mSwapchainLength; }

private:
    const DeviceInfo* const mDeviceInfo;

    VkFormat mFormat;
    VkExtent2D mDisplaySize;
    VkSwapchainKHR mSwapchain;
    uint32_t mSwapchainLength;
};

class Renderer {
public:
    Renderer(const DeviceInfo* const deviceInfo, const SwapchainInfo* const swapchainInfo);
    ~Renderer();
    VkTestResult init(JNIEnv* env, jobject assetManager);
    VkTestResult drawFrame();

private:
    VkTestResult createRenderPass();
    VkTestResult createFrameBuffers();
    VkTestResult createVertexBuffers();
    VkTestResult loadShaderFromFile(const char* filePath, VkShaderModule* const outShader);
    VkTestResult createGraphicsPipeline();

    const DeviceInfo* const mDeviceInfo;
    const SwapchainInfo* const mSwapchainInfo;

    AAssetManager* mAssetManager;

    VkDeviceMemory mDeviceMemory;
    VkBuffer mVertexBuffer;

    VkRenderPass mRenderPass;
    VkShaderModule mVertexShader;
    VkShaderModule mFragmentShader;
    VkPipelineLayout mPipelineLayout;
    VkPipeline mPipeline;

    VkCommandPool mCommandPool;
    VkSemaphore mSemaphore;
    VkFence mFence;

    std::vector<VkImageView> mImageViews;
    std::vector<VkFramebuffer> mFramebuffers;
    std::vector<VkCommandBuffer> mCommandBuffers;
};

#endif // VULKAN_PRE_TRANSFORM_TEST_HELPERS_H
