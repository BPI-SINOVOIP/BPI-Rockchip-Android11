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

#include <gtest/gtest.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <vector>
#include <vulkan/vulkan.h>
#include <optional>
#include "testutils.h"

char *buffer;
volatile int counter = 0;

static void handler (int, siginfo_t *si, void *) {
    ++counter;
    ASSERT_EQ(mprotect((void *)((size_t)si->si_addr & ~0xFFFull), 1, PROT_READ | PROT_WRITE), 0) <<
        "mprotect() error. Can't reset privileges in signal handler.";
}

TEST(memory, mprotect) {
    ASSUME_GAMECORE_CERTIFIED();

    VkInstance instance;
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "mprotect test";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInstanceInfo = {};
    createInstanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInstanceInfo.pApplicationInfo = &appInfo;

    ASSERT_EQ(vkCreateInstance(&createInstanceInfo, nullptr, &instance), VK_SUCCESS)  << "vkCreateInstance() failed!";



    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    ASSERT_GT(deviceCount, 0) << "vkEnumeratePhysicalDevices() could not find a physical device";
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    physicalDevice = devices[0];



    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
    uint32_t graphicsFamily = queueFamilyCount+1;

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i;
        }

        if (graphicsFamily < queueFamilyCount+1) {
            break;
        }

        i++;
    }

    ASSERT_LT(graphicsFamily, queueFamilyCount + 1) << "No Graphics Queue. Can't init Vulkan";



    VkDevice device;

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsFamily;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures = {};
    VkDeviceCreateInfo createDeviceInfo = {};
    createDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createDeviceInfo.pQueueCreateInfos = &queueCreateInfo;
    createDeviceInfo.queueCreateInfoCount = 1;
    createDeviceInfo.pEnabledFeatures = &deviceFeatures;
    createDeviceInfo.enabledExtensionCount = 0;
    createDeviceInfo.enabledLayerCount = 0;

    ASSERT_EQ(vkCreateDevice(physicalDevice, &createDeviceInfo, nullptr, &device), VK_SUCCESS) << "vkCreateDevice() failed!";



    int pagesize;
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    ASSERT_EQ(sigaction(SIGSEGV, &sa, nullptr), 0) << "sigaction() failed!";

    pagesize = sysconf(_SC_PAGE_SIZE);
    ASSERT_GT(pagesize, -1) << "sysconf() failed!";


    const int bufferOffset = 2;
    const int pagesToWrite = 5;
    const int pagesInBuffer = 4;


    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            VkMemoryAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = pagesToWrite * pagesize;
            allocInfo.memoryTypeIndex = i;

            VkDeviceMemory testBufferMemory;

            ASSERT_EQ(vkAllocateMemory(device, &allocInfo, nullptr, &testBufferMemory), VK_SUCCESS) << "vkAllocateMemory() failed!";

            // Map a "misaligned" 4-page subset of the 5 pages allocated.
            vkMapMemory(device, testBufferMemory, bufferOffset, pagesInBuffer * pagesize, 0, (void**)&buffer);

            ASSERT_NE(buffer, nullptr) << "vkMapMemory() returned a null buffer.";

            // mprotect requires the beginning of a page as its first parameter. We also map a little more than 4 pages
            // to make sure we can attempt to write to the final page.
            ASSERT_EQ(mprotect(buffer - bufferOffset, pagesInBuffer * pagesize + 1, PROT_READ), 0) << "mprotect() failed!";

            for (char *p = buffer; p != (buffer + pagesInBuffer * pagesize); p++) {
                *p = 'a';
            }

            vkUnmapMemory(device, testBufferMemory);
            EXPECT_EQ(pagesToWrite, counter) << "Memory type " << i << " wrote " << counter << " pages instead of " << pagesToWrite;
        }
        counter = 0;
    }
}
