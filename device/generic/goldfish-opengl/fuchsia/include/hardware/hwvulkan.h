#ifndef __HARDWARE_HWVULKAN_H__
#define __HARDWARE_HWVULKAN_H__

#include <hardware/hardware.h>
#include <vulkan/vulkan.h>

#define HWVULKAN_HARDWARE_MODULE_ID "vulkan"
#define HWVULKAN_MODULE_API_VERSION_0_1 0
#define HWVULKAN_DEVICE_API_VERSION_0_1 0

#define HWVULKAN_DEVICE_0 "vk0"

typedef struct hwvulkan_module_t {
    struct hw_module_t common;
} hwvulkan_module_t;

#define HWVULKAN_DISPATCH_MAGIC 0x01CDC0DE
typedef union {
    uintptr_t magic;
    const void* vtbl;
} hwvulkan_dispatch_t;

typedef struct hwvulkan_device_t {
    struct hw_device_t common;

    PFN_vkEnumerateInstanceExtensionProperties
        EnumerateInstanceExtensionProperties;
    PFN_vkCreateInstance CreateInstance;
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
} hwvulkan_device_t;

#endif
