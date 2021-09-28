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
 *
 */

#define LOG_TAG "GpuProfilingData"

#include <vector>

#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>
#include <string>
#include <vulkan/vulkan.h>

#define ALOGI(msg, ...)                                                        \
  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, (msg), __VA_ARGS__)
#define ALOGE(msg, ...)                                                        \
  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, (msg), __VA_ARGS__)
#define ALOGD(msg, ...)                                                        \
  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, (msg), __VA_ARGS__)
#define REQUIRE_SUCCESS(fn, name)                                              \
  do {                                                                         \
    if (VK_SUCCESS != fn) {                                                    \
      ALOGE("Vulkan Error in %s", name);                                       \
      return -1;                                                               \
    }                                                                          \
  } while (0)

namespace {

typedef void (*FN_PTR)(void);

int initVulkan() {
  const VkApplicationInfo appInfo = {
      VK_STRUCTURE_TYPE_APPLICATION_INFO,
      nullptr,            // pNext
      "GpuProfilingData", // app name
      0,                  // app version
      nullptr,            // engine name
      0,                  // engine version
      VK_API_VERSION_1_0,
  };
  const VkInstanceCreateInfo instanceInfo = {
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      nullptr, // pNext
      0,       // flags
      &appInfo,
      0,       // layer count
      nullptr, // layers
      0,       // extension count
      nullptr, // extensions
  };
  VkInstance instance;
  REQUIRE_SUCCESS(vkCreateInstance(&instanceInfo, nullptr, &instance),
                  "vkCreateInstance");

  VkPhysicalDevice physicalDevice = {};
  uint32_t nPhysicalDevices;
  REQUIRE_SUCCESS(
      vkEnumeratePhysicalDevices(instance, &nPhysicalDevices, nullptr),
      "vkEnumeratePhysicalDevices");
  std::vector<VkPhysicalDevice> physicalDevices(nPhysicalDevices);

  REQUIRE_SUCCESS(vkEnumeratePhysicalDevices(instance, &nPhysicalDevices,
                                             physicalDevices.data()),
                  "vkEnumeratePhysicalDevices");

  uint32_t queueFamilyIndex = static_cast<uint32_t>(-1);
  uint32_t i;
  for (i = 0; i < nPhysicalDevices; ++i) {
    uint32_t nQueueProperties = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i],
                                             &nQueueProperties, nullptr);
    std::vector<VkQueueFamilyProperties> queueProperties(nQueueProperties);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevices[i], &nQueueProperties, queueProperties.data());
    for (uint32_t j = 0; j < nQueueProperties; ++j) {
      if (queueProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        queueFamilyIndex = j;
        break;
      }
    }
    if (queueFamilyIndex != static_cast<uint32_t>(-1)) {
      break;
    }
  }
  if (i == nPhysicalDevices) {
    ALOGE("%s",
          "Could not find a physical device that supports a graphics queue");
    return -1;
  }
  physicalDevice = physicalDevices[i];

  float priority = 1.0f;
  VkDeviceQueueCreateInfo queueCreateInfo{
      VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      nullptr, // pNext
      0,       // flags
      queueFamilyIndex,
      1,
      &priority,
  };

  VkDeviceCreateInfo deviceCreateInfo{
      VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      nullptr, // pNext
      0,       // flags
      1,
      &queueCreateInfo,
      0,
      nullptr,
      0,
      nullptr,
      nullptr,
  };

  VkDevice device;

  REQUIRE_SUCCESS(
      vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device),
      "vkCreateDevice");
  return 0;
}

jint android_graphics_cts_GpuProfilingData_nativeInitVulkan(JNIEnv * /*env*/,
                                                            jclass /*clazz*/) {
  return (jint)initVulkan();
}

static JNINativeMethod gMethods[] = {
    {"nativeInitVulkan", "()I",
     (void *)android_graphics_cts_GpuProfilingData_nativeInitVulkan}};
} // anonymous namespace

int register_android_gputools_cts_GpuProfilingData(JNIEnv *env) {
  jclass clazz = env->FindClass(
      "android/graphics/gpuprofiling/app/GpuRenderStagesDeviceActivity");
  return env->RegisterNatives(clazz, gMethods,
                              sizeof(gMethods) / sizeof(JNINativeMethod));
}