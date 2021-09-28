/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "tu_private.h"

#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include "compiler/glsl_types.h"
#include "util/debug.h"
#include "util/disk_cache.h"
#include "util/u_atomic.h"
#include "vk_format.h"
#include "vk_util.h"

/* for fd_get_driver/device_uuid() */
#include "freedreno/common/freedreno_uuid.h"

#define TU_HAS_SURFACE \
   (VK_USE_PLATFORM_WAYLAND_KHR || \
    VK_USE_PLATFORM_XCB_KHR || \
    VK_USE_PLATFORM_XLIB_KHR || \
    VK_USE_PLATFORM_DISPLAY_KHR)

static int
tu_device_get_cache_uuid(uint16_t family, void *uuid)
{
   uint32_t mesa_timestamp;
   uint16_t f = family;
   memset(uuid, 0, VK_UUID_SIZE);
   if (!disk_cache_get_function_timestamp(tu_device_get_cache_uuid,
                                          &mesa_timestamp))
      return -1;

   memcpy(uuid, &mesa_timestamp, 4);
   memcpy((char *) uuid + 4, &f, 2);
   snprintf((char *) uuid + 6, VK_UUID_SIZE - 10, "tu");
   return 0;
}

VkResult
tu_physical_device_init(struct tu_physical_device *device,
                        struct tu_instance *instance)
{
   VkResult result = VK_SUCCESS;

   memset(device->name, 0, sizeof(device->name));
   sprintf(device->name, "FD%d", device->gpu_id);

   device->limited_z24s8 = (device->gpu_id == 630);

   switch (device->gpu_id) {
   case 615:
   case 618:
   case 630:
   case 640:
   case 650:
      freedreno_dev_info_init(&device->info, device->gpu_id);
      break;
   default:
      result = vk_startup_errorf(instance, VK_ERROR_INITIALIZATION_FAILED,
                                 "device %s is unsupported", device->name);
      goto fail;
   }
   if (tu_device_get_cache_uuid(device->gpu_id, device->cache_uuid)) {
      result = vk_startup_errorf(instance, VK_ERROR_INITIALIZATION_FAILED,
                                 "cannot generate UUID");
      goto fail;
   }

   /* The gpu id is already embedded in the uuid so we just pass "tu"
    * when creating the cache.
    */
   char buf[VK_UUID_SIZE * 2 + 1];
   disk_cache_format_hex_id(buf, device->cache_uuid, VK_UUID_SIZE * 2);
   device->disk_cache = disk_cache_create(device->name, buf, 0);

   fprintf(stderr, "WARNING: tu is not a conformant vulkan implementation, "
                   "testing use only.\n");

   fd_get_driver_uuid(device->driver_uuid);
   fd_get_device_uuid(device->device_uuid, device->gpu_id);

   tu_physical_device_get_supported_extensions(device, &device->supported_extensions);

#if TU_HAS_SURFACE
   result = tu_wsi_init(device);
   if (result != VK_SUCCESS) {
      vk_startup_errorf(instance, result, "WSI init failure");
      goto fail;
   }
#endif

   return VK_SUCCESS;

fail:
   close(device->local_fd);
   if (device->master_fd != -1)
      close(device->master_fd);
   return result;
}

static void
tu_physical_device_finish(struct tu_physical_device *device)
{
#if TU_HAS_SURFACE
   tu_wsi_finish(device);
#endif

   disk_cache_destroy(device->disk_cache);
   close(device->local_fd);
   if (device->master_fd != -1)
      close(device->master_fd);

   vk_object_base_finish(&device->base);
}

static VKAPI_ATTR void *
default_alloc_func(void *pUserData,
                   size_t size,
                   size_t align,
                   VkSystemAllocationScope allocationScope)
{
   return malloc(size);
}

static VKAPI_ATTR void *
default_realloc_func(void *pUserData,
                     void *pOriginal,
                     size_t size,
                     size_t align,
                     VkSystemAllocationScope allocationScope)
{
   return realloc(pOriginal, size);
}

static VKAPI_ATTR void
default_free_func(void *pUserData, void *pMemory)
{
   free(pMemory);
}

static const VkAllocationCallbacks default_alloc = {
   .pUserData = NULL,
   .pfnAllocation = default_alloc_func,
   .pfnReallocation = default_realloc_func,
   .pfnFree = default_free_func,
};

static const struct debug_control tu_debug_options[] = {
   { "startup", TU_DEBUG_STARTUP },
   { "nir", TU_DEBUG_NIR },
   { "ir3", TU_DEBUG_IR3 },
   { "nobin", TU_DEBUG_NOBIN },
   { "sysmem", TU_DEBUG_SYSMEM },
   { "forcebin", TU_DEBUG_FORCEBIN },
   { "noubwc", TU_DEBUG_NOUBWC },
   { "nomultipos", TU_DEBUG_NOMULTIPOS },
   { "nolrz", TU_DEBUG_NOLRZ },
   { NULL, 0 }
};

const char *
tu_get_debug_option_name(int id)
{
   assert(id < ARRAY_SIZE(tu_debug_options) - 1);
   return tu_debug_options[id].string;
}

static int
tu_get_instance_extension_index(const char *name)
{
   for (unsigned i = 0; i < TU_INSTANCE_EXTENSION_COUNT; ++i) {
      if (strcmp(name, tu_instance_extensions[i].extensionName) == 0)
         return i;
   }
   return -1;
}

VkResult
tu_CreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                  const VkAllocationCallbacks *pAllocator,
                  VkInstance *pInstance)
{
   struct tu_instance *instance;
   VkResult result;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);

   uint32_t client_version;
   if (pCreateInfo->pApplicationInfo &&
       pCreateInfo->pApplicationInfo->apiVersion != 0) {
      client_version = pCreateInfo->pApplicationInfo->apiVersion;
   } else {
      tu_EnumerateInstanceVersion(&client_version);
   }

   instance = vk_zalloc2(&default_alloc, pAllocator, sizeof(*instance), 8,
                         VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

   if (!instance)
      return vk_error(NULL, VK_ERROR_OUT_OF_HOST_MEMORY);

   vk_object_base_init(NULL, &instance->base, VK_OBJECT_TYPE_INSTANCE);

   if (pAllocator)
      instance->alloc = *pAllocator;
   else
      instance->alloc = default_alloc;

   instance->api_version = client_version;
   instance->physical_device_count = -1;

   instance->debug_flags =
      parse_debug_string(getenv("TU_DEBUG"), tu_debug_options);

#ifdef DEBUG
   /* Enable startup debugging by default on debug drivers.  You almost always
    * want to see your startup failures in that case, and it's hard to set
    * this env var on android.
    */
   instance->debug_flags |= TU_DEBUG_STARTUP;
#endif

   if (instance->debug_flags & TU_DEBUG_STARTUP)
      mesa_logi("Created an instance");

   for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; i++) {
      const char *ext_name = pCreateInfo->ppEnabledExtensionNames[i];
      int index = tu_get_instance_extension_index(ext_name);

      if (index < 0 || !tu_instance_extensions_supported.extensions[index]) {
         vk_object_base_finish(&instance->base);
         vk_free2(&default_alloc, pAllocator, instance);
         return vk_startup_errorf(instance, VK_ERROR_EXTENSION_NOT_PRESENT,
                                  "Missing %s", ext_name);
      }

      instance->enabled_extensions.extensions[index] = true;
   }

   result = vk_debug_report_instance_init(&instance->debug_report_callbacks);
   if (result != VK_SUCCESS) {
      vk_object_base_finish(&instance->base);
      vk_free2(&default_alloc, pAllocator, instance);
      return vk_startup_errorf(instance, result, "debug_report setup failure");
   }

   glsl_type_singleton_init_or_ref();

   VG(VALGRIND_CREATE_MEMPOOL(instance, 0, false));

   *pInstance = tu_instance_to_handle(instance);

   return VK_SUCCESS;
}

void
tu_DestroyInstance(VkInstance _instance,
                   const VkAllocationCallbacks *pAllocator)
{
   TU_FROM_HANDLE(tu_instance, instance, _instance);

   if (!instance)
      return;

   for (int i = 0; i < instance->physical_device_count; ++i) {
      tu_physical_device_finish(instance->physical_devices + i);
   }

   VG(VALGRIND_DESTROY_MEMPOOL(instance));

   glsl_type_singleton_decref();

   vk_debug_report_instance_destroy(&instance->debug_report_callbacks);

   vk_object_base_finish(&instance->base);
   vk_free(&instance->alloc, instance);
}

VkResult
tu_EnumeratePhysicalDevices(VkInstance _instance,
                            uint32_t *pPhysicalDeviceCount,
                            VkPhysicalDevice *pPhysicalDevices)
{
   TU_FROM_HANDLE(tu_instance, instance, _instance);
   VK_OUTARRAY_MAKE(out, pPhysicalDevices, pPhysicalDeviceCount);

   VkResult result;

   if (instance->physical_device_count < 0) {
      result = tu_enumerate_devices(instance);
      if (result != VK_SUCCESS && result != VK_ERROR_INCOMPATIBLE_DRIVER)
         return result;
   }

   for (uint32_t i = 0; i < instance->physical_device_count; ++i) {
      vk_outarray_append(&out, p)
      {
         *p = tu_physical_device_to_handle(instance->physical_devices + i);
      }
   }

   return vk_outarray_status(&out);
}

VkResult
tu_EnumeratePhysicalDeviceGroups(
   VkInstance _instance,
   uint32_t *pPhysicalDeviceGroupCount,
   VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties)
{
   TU_FROM_HANDLE(tu_instance, instance, _instance);
   VK_OUTARRAY_MAKE(out, pPhysicalDeviceGroupProperties,
                    pPhysicalDeviceGroupCount);
   VkResult result;

   if (instance->physical_device_count < 0) {
      result = tu_enumerate_devices(instance);
      if (result != VK_SUCCESS && result != VK_ERROR_INCOMPATIBLE_DRIVER)
         return result;
   }

   for (uint32_t i = 0; i < instance->physical_device_count; ++i) {
      vk_outarray_append(&out, p)
      {
         p->physicalDeviceCount = 1;
         p->physicalDevices[0] =
            tu_physical_device_to_handle(instance->physical_devices + i);
         p->subsetAllocation = false;
      }
   }

   return vk_outarray_status(&out);
}

void
tu_GetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice,
                              VkPhysicalDeviceFeatures2 *pFeatures)
{
   pFeatures->features = (VkPhysicalDeviceFeatures) {
      .robustBufferAccess = true,
      .fullDrawIndexUint32 = true,
      .imageCubeArray = true,
      .independentBlend = true,
      .geometryShader = true,
      .tessellationShader = true,
      .sampleRateShading = true,
      .dualSrcBlend = true,
      .logicOp = true,
      .multiDrawIndirect = true,
      .drawIndirectFirstInstance = true,
      .depthClamp = true,
      .depthBiasClamp = true,
      .fillModeNonSolid = true,
      .depthBounds = true,
      .wideLines = false,
      .largePoints = true,
      .alphaToOne = true,
      .multiViewport = true,
      .samplerAnisotropy = true,
      .textureCompressionETC2 = true,
      .textureCompressionASTC_LDR = true,
      .textureCompressionBC = true,
      .occlusionQueryPrecise = true,
      .pipelineStatisticsQuery = true,
      .vertexPipelineStoresAndAtomics = true,
      .fragmentStoresAndAtomics = true,
      .shaderTessellationAndGeometryPointSize = false,
      .shaderImageGatherExtended = true,
      .shaderStorageImageExtendedFormats = true,
      .shaderStorageImageMultisample = false,
      .shaderUniformBufferArrayDynamicIndexing = true,
      .shaderSampledImageArrayDynamicIndexing = true,
      .shaderStorageBufferArrayDynamicIndexing = true,
      .shaderStorageImageArrayDynamicIndexing = true,
      .shaderStorageImageReadWithoutFormat = true,
      .shaderStorageImageWriteWithoutFormat = true,
      .shaderClipDistance = true,
      .shaderCullDistance = true,
      .shaderFloat64 = false,
      .shaderInt64 = false,
      .shaderInt16 = false,
      .sparseBinding = false,
      .variableMultisampleRate = false,
      .inheritedQueries = false,
   };

   vk_foreach_struct(ext, pFeatures->pNext)
   {
      switch (ext->sType) {
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES: {
         VkPhysicalDeviceVulkan11Features *features = (void *) ext;
         features->storageBuffer16BitAccess            = false;
         features->uniformAndStorageBuffer16BitAccess  = false;
         features->storagePushConstant16               = false;
         features->storageInputOutput16                = false;
         features->multiview                           = true;
         features->multiviewGeometryShader             = false;
         features->multiviewTessellationShader         = false;
         features->variablePointersStorageBuffer       = true;
         features->variablePointers                    = true;
         features->protectedMemory                     = false;
         features->samplerYcbcrConversion              = true;
         features->shaderDrawParameters                = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES: {
         VkPhysicalDeviceVulkan12Features *features = (void *) ext;
         features->samplerMirrorClampToEdge            = true;
         features->drawIndirectCount                   = true;
         features->storageBuffer8BitAccess             = false;
         features->uniformAndStorageBuffer8BitAccess   = false;
         features->storagePushConstant8                = false;
         features->shaderBufferInt64Atomics            = false;
         features->shaderSharedInt64Atomics            = false;
         features->shaderFloat16                       = false;
         features->shaderInt8                          = false;

         features->descriptorIndexing                                 = false;
         features->shaderInputAttachmentArrayDynamicIndexing          = false;
         features->shaderUniformTexelBufferArrayDynamicIndexing       = false;
         features->shaderStorageTexelBufferArrayDynamicIndexing       = false;
         features->shaderUniformBufferArrayNonUniformIndexing         = false;
         features->shaderSampledImageArrayNonUniformIndexing          = false;
         features->shaderStorageBufferArrayNonUniformIndexing         = false;
         features->shaderStorageImageArrayNonUniformIndexing          = false;
         features->shaderInputAttachmentArrayNonUniformIndexing       = false;
         features->shaderUniformTexelBufferArrayNonUniformIndexing    = false;
         features->shaderStorageTexelBufferArrayNonUniformIndexing    = false;
         features->descriptorBindingUniformBufferUpdateAfterBind      = false;
         features->descriptorBindingSampledImageUpdateAfterBind       = false;
         features->descriptorBindingStorageImageUpdateAfterBind       = false;
         features->descriptorBindingStorageBufferUpdateAfterBind      = false;
         features->descriptorBindingUniformTexelBufferUpdateAfterBind = false;
         features->descriptorBindingStorageTexelBufferUpdateAfterBind = false;
         features->descriptorBindingUpdateUnusedWhilePending          = false;
         features->descriptorBindingPartiallyBound                    = false;
         features->descriptorBindingVariableDescriptorCount           = false;
         features->runtimeDescriptorArray                             = false;

         features->samplerFilterMinmax                 = true;
         features->scalarBlockLayout                   = false;
         features->imagelessFramebuffer                = false;
         features->uniformBufferStandardLayout         = false;
         features->shaderSubgroupExtendedTypes         = false;
         features->separateDepthStencilLayouts         = false;
         features->hostQueryReset                      = true;
         features->timelineSemaphore                   = false;
         features->bufferDeviceAddress                 = false;
         features->bufferDeviceAddressCaptureReplay    = false;
         features->bufferDeviceAddressMultiDevice      = false;
         features->vulkanMemoryModel                   = false;
         features->vulkanMemoryModelDeviceScope        = false;
         features->vulkanMemoryModelAvailabilityVisibilityChains = false;
         features->shaderOutputViewportIndex           = true;
         features->shaderOutputLayer                   = true;
         features->subgroupBroadcastDynamicId          = false;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES: {
         VkPhysicalDeviceVariablePointersFeatures *features = (void *) ext;
         features->variablePointersStorageBuffer = true;
         features->variablePointers = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES: {
         VkPhysicalDeviceMultiviewFeatures *features =
            (VkPhysicalDeviceMultiviewFeatures *) ext;
         features->multiview = true;
         features->multiviewGeometryShader = false;
         features->multiviewTessellationShader = false;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES: {
         VkPhysicalDeviceShaderDrawParametersFeatures *features =
            (VkPhysicalDeviceShaderDrawParametersFeatures *) ext;
         features->shaderDrawParameters = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES: {
         VkPhysicalDeviceProtectedMemoryFeatures *features =
            (VkPhysicalDeviceProtectedMemoryFeatures *) ext;
         features->protectedMemory = false;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES: {
         VkPhysicalDevice16BitStorageFeatures *features =
            (VkPhysicalDevice16BitStorageFeatures *) ext;
         features->storageBuffer16BitAccess = false;
         features->uniformAndStorageBuffer16BitAccess = false;
         features->storagePushConstant16 = false;
         features->storageInputOutput16 = false;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES: {
         VkPhysicalDeviceSamplerYcbcrConversionFeatures *features =
            (VkPhysicalDeviceSamplerYcbcrConversionFeatures *) ext;
         features->samplerYcbcrConversion = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT: {
         VkPhysicalDeviceDescriptorIndexingFeaturesEXT *features =
            (VkPhysicalDeviceDescriptorIndexingFeaturesEXT *) ext;
         features->shaderInputAttachmentArrayDynamicIndexing = false;
         features->shaderUniformTexelBufferArrayDynamicIndexing = false;
         features->shaderStorageTexelBufferArrayDynamicIndexing = false;
         features->shaderUniformBufferArrayNonUniformIndexing = false;
         features->shaderSampledImageArrayNonUniformIndexing = false;
         features->shaderStorageBufferArrayNonUniformIndexing = false;
         features->shaderStorageImageArrayNonUniformIndexing = false;
         features->shaderInputAttachmentArrayNonUniformIndexing = false;
         features->shaderUniformTexelBufferArrayNonUniformIndexing = false;
         features->shaderStorageTexelBufferArrayNonUniformIndexing = false;
         features->descriptorBindingUniformBufferUpdateAfterBind = false;
         features->descriptorBindingSampledImageUpdateAfterBind = false;
         features->descriptorBindingStorageImageUpdateAfterBind = false;
         features->descriptorBindingStorageBufferUpdateAfterBind = false;
         features->descriptorBindingUniformTexelBufferUpdateAfterBind = false;
         features->descriptorBindingStorageTexelBufferUpdateAfterBind = false;
         features->descriptorBindingUpdateUnusedWhilePending = false;
         features->descriptorBindingPartiallyBound = false;
         features->descriptorBindingVariableDescriptorCount = false;
         features->runtimeDescriptorArray = false;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT: {
         VkPhysicalDeviceConditionalRenderingFeaturesEXT *features =
            (VkPhysicalDeviceConditionalRenderingFeaturesEXT *) ext;
         features->conditionalRendering = true;
         features->inheritedConditionalRendering = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT: {
         VkPhysicalDeviceTransformFeedbackFeaturesEXT *features =
            (VkPhysicalDeviceTransformFeedbackFeaturesEXT *) ext;
         features->transformFeedback = true;
         features->geometryStreams = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT: {
         VkPhysicalDeviceIndexTypeUint8FeaturesEXT *features =
            (VkPhysicalDeviceIndexTypeUint8FeaturesEXT *)ext;
         features->indexTypeUint8 = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT: {
         VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT *features =
            (VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT *)ext;
         features->vertexAttributeInstanceRateDivisor = true;
         features->vertexAttributeInstanceRateZeroDivisor = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES_EXT: {
         VkPhysicalDevicePrivateDataFeaturesEXT *features =
            (VkPhysicalDevicePrivateDataFeaturesEXT *)ext;
         features->privateData = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT: {
         VkPhysicalDeviceDepthClipEnableFeaturesEXT *features =
            (VkPhysicalDeviceDepthClipEnableFeaturesEXT *)ext;
         features->depthClipEnable = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_4444_FORMATS_FEATURES_EXT: {
         VkPhysicalDevice4444FormatsFeaturesEXT *features = (void *)ext;
         features->formatA4R4G4B4 = true;
         features->formatA4B4G4R4 = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT: {
         VkPhysicalDeviceCustomBorderColorFeaturesEXT *features = (void *) ext;
         features->customBorderColors = true;
         features->customBorderColorWithoutFormat = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT: {
         VkPhysicalDeviceHostQueryResetFeaturesEXT *features =
            (VkPhysicalDeviceHostQueryResetFeaturesEXT *)ext;
         features->hostQueryReset = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT: {
         VkPhysicalDeviceExtendedDynamicStateFeaturesEXT *features = (void *)ext;
         features->extendedDynamicState = true;
         break;
      }
      default:
         break;
      }
   }
}

void
tu_GetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice,
                                VkPhysicalDeviceProperties2 *pProperties)
{
   TU_FROM_HANDLE(tu_physical_device, pdevice, physicalDevice);
   VkSampleCountFlags sample_counts =
      VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT;

   /* I have no idea what the maximum size is, but the hardware supports very
    * large numbers of descriptors (at least 2^16). This limit is based on
    * CP_LOAD_STATE6, which has a 28-bit field for the DWORD offset, so that
    * we don't have to think about what to do if that overflows, but really
    * nothing is likely to get close to this.
    */
   const size_t max_descriptor_set_size = (1 << 28) / A6XX_TEX_CONST_DWORDS;

   VkPhysicalDeviceLimits limits = {
      .maxImageDimension1D = (1 << 14),
      .maxImageDimension2D = (1 << 14),
      .maxImageDimension3D = (1 << 11),
      .maxImageDimensionCube = (1 << 14),
      .maxImageArrayLayers = (1 << 11),
      .maxTexelBufferElements = 128 * 1024 * 1024,
      .maxUniformBufferRange = MAX_UNIFORM_BUFFER_RANGE,
      .maxStorageBufferRange = MAX_STORAGE_BUFFER_RANGE,
      .maxPushConstantsSize = MAX_PUSH_CONSTANTS_SIZE,
      .maxMemoryAllocationCount = UINT32_MAX,
      .maxSamplerAllocationCount = 64 * 1024,
      .bufferImageGranularity = 64,          /* A cache line */
      .sparseAddressSpaceSize = 0xffffffffu, /* buffer max size */
      .maxBoundDescriptorSets = MAX_SETS,
      .maxPerStageDescriptorSamplers = max_descriptor_set_size,
      .maxPerStageDescriptorUniformBuffers = max_descriptor_set_size,
      .maxPerStageDescriptorStorageBuffers = max_descriptor_set_size,
      .maxPerStageDescriptorSampledImages = max_descriptor_set_size,
      .maxPerStageDescriptorStorageImages = max_descriptor_set_size,
      .maxPerStageDescriptorInputAttachments = MAX_RTS,
      .maxPerStageResources = max_descriptor_set_size,
      .maxDescriptorSetSamplers = max_descriptor_set_size,
      .maxDescriptorSetUniformBuffers = max_descriptor_set_size,
      .maxDescriptorSetUniformBuffersDynamic = MAX_DYNAMIC_UNIFORM_BUFFERS,
      .maxDescriptorSetStorageBuffers = max_descriptor_set_size,
      .maxDescriptorSetStorageBuffersDynamic = MAX_DYNAMIC_STORAGE_BUFFERS,
      .maxDescriptorSetSampledImages = max_descriptor_set_size,
      .maxDescriptorSetStorageImages = max_descriptor_set_size,
      .maxDescriptorSetInputAttachments = MAX_RTS,
      .maxVertexInputAttributes = 32,
      .maxVertexInputBindings = 32,
      .maxVertexInputAttributeOffset = 4095,
      .maxVertexInputBindingStride = 2048,
      .maxVertexOutputComponents = 128,
      .maxTessellationGenerationLevel = 64,
      .maxTessellationPatchSize = 32,
      .maxTessellationControlPerVertexInputComponents = 128,
      .maxTessellationControlPerVertexOutputComponents = 128,
      .maxTessellationControlPerPatchOutputComponents = 120,
      .maxTessellationControlTotalOutputComponents = 4096,
      .maxTessellationEvaluationInputComponents = 128,
      .maxTessellationEvaluationOutputComponents = 128,
      .maxGeometryShaderInvocations = 32,
      .maxGeometryInputComponents = 64,
      .maxGeometryOutputComponents = 128,
      .maxGeometryOutputVertices = 256,
      .maxGeometryTotalOutputComponents = 1024,
      .maxFragmentInputComponents = 124,
      .maxFragmentOutputAttachments = 8,
      .maxFragmentDualSrcAttachments = 1,
      .maxFragmentCombinedOutputResources = 8,
      .maxComputeSharedMemorySize = 32768,
      .maxComputeWorkGroupCount = { 65535, 65535, 65535 },
      .maxComputeWorkGroupInvocations = 2048,
      .maxComputeWorkGroupSize = { 2048, 2048, 2048 },
      .subPixelPrecisionBits = 8,
      .subTexelPrecisionBits = 8,
      .mipmapPrecisionBits = 8,
      .maxDrawIndexedIndexValue = UINT32_MAX,
      .maxDrawIndirectCount = UINT32_MAX,
      .maxSamplerLodBias = 4095.0 / 256.0, /* [-16, 15.99609375] */
      .maxSamplerAnisotropy = 16,
      .maxViewports = MAX_VIEWPORTS,
      .maxViewportDimensions = { (1 << 14), (1 << 14) },
      .viewportBoundsRange = { INT16_MIN, INT16_MAX },
      .viewportSubPixelBits = 8,
      .minMemoryMapAlignment = 4096, /* A page */
      .minTexelBufferOffsetAlignment = 64,
      .minUniformBufferOffsetAlignment = 64,
      .minStorageBufferOffsetAlignment = 64,
      .minTexelOffset = -16,
      .maxTexelOffset = 15,
      .minTexelGatherOffset = -32,
      .maxTexelGatherOffset = 31,
      .minInterpolationOffset = -0.5,
      .maxInterpolationOffset = 0.4375,
      .subPixelInterpolationOffsetBits = 4,
      .maxFramebufferWidth = (1 << 14),
      .maxFramebufferHeight = (1 << 14),
      .maxFramebufferLayers = (1 << 10),
      .framebufferColorSampleCounts = sample_counts,
      .framebufferDepthSampleCounts = sample_counts,
      .framebufferStencilSampleCounts = sample_counts,
      .framebufferNoAttachmentsSampleCounts = sample_counts,
      .maxColorAttachments = MAX_RTS,
      .sampledImageColorSampleCounts = sample_counts,
      .sampledImageIntegerSampleCounts = VK_SAMPLE_COUNT_1_BIT,
      .sampledImageDepthSampleCounts = sample_counts,
      .sampledImageStencilSampleCounts = sample_counts,
      .storageImageSampleCounts = VK_SAMPLE_COUNT_1_BIT,
      .maxSampleMaskWords = 1,
      .timestampComputeAndGraphics = true,
      .timestampPeriod = 1000000000.0 / 19200000.0, /* CP_ALWAYS_ON_COUNTER is fixed 19.2MHz */
      .maxClipDistances = 8,
      .maxCullDistances = 8,
      .maxCombinedClipAndCullDistances = 8,
      .discreteQueuePriorities = 1,
      .pointSizeRange = { 1, 4092 },
      .lineWidthRange = { 0.0, 7.9921875 },
      .pointSizeGranularity = 	0.0625,
      .lineWidthGranularity = (1.0 / 128.0),
      .strictLines = false, /* FINISHME */
      .standardSampleLocations = true,
      .optimalBufferCopyOffsetAlignment = 128,
      .optimalBufferCopyRowPitchAlignment = 128,
      .nonCoherentAtomSize = 64,
   };

   pProperties->properties = (VkPhysicalDeviceProperties) {
      .apiVersion = tu_physical_device_api_version(pdevice),
      .driverVersion = vk_get_driver_version(),
      .vendorID = 0, /* TODO */
      .deviceID = 0,
      .deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
      .limits = limits,
      .sparseProperties = { 0 },
   };

   strcpy(pProperties->properties.deviceName, pdevice->name);
   memcpy(pProperties->properties.pipelineCacheUUID, pdevice->cache_uuid, VK_UUID_SIZE);

   vk_foreach_struct(ext, pProperties->pNext)
   {
      switch (ext->sType) {
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR: {
         VkPhysicalDevicePushDescriptorPropertiesKHR *properties =
            (VkPhysicalDevicePushDescriptorPropertiesKHR *) ext;
         properties->maxPushDescriptors = MAX_PUSH_DESCRIPTORS;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES: {
         VkPhysicalDeviceIDProperties *properties =
            (VkPhysicalDeviceIDProperties *) ext;
         memcpy(properties->driverUUID, pdevice->driver_uuid, VK_UUID_SIZE);
         memcpy(properties->deviceUUID, pdevice->device_uuid, VK_UUID_SIZE);
         properties->deviceLUIDValid = false;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES: {
         VkPhysicalDeviceMultiviewProperties *properties =
            (VkPhysicalDeviceMultiviewProperties *) ext;
         properties->maxMultiviewViewCount = MAX_VIEWS;
         properties->maxMultiviewInstanceIndex = INT_MAX;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES: {
         VkPhysicalDevicePointClippingProperties *properties =
            (VkPhysicalDevicePointClippingProperties *) ext;
         properties->pointClippingBehavior =
            VK_POINT_CLIPPING_BEHAVIOR_ALL_CLIP_PLANES;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES: {
         VkPhysicalDeviceMaintenance3Properties *properties =
            (VkPhysicalDeviceMaintenance3Properties *) ext;
         /* Make sure everything is addressable by a signed 32-bit int, and
          * our largest descriptors are 96 bytes. */
         properties->maxPerSetDescriptors = (1ull << 31) / 96;
         /* Our buffer size fields allow only this much */
         properties->maxMemoryAllocationSize = 0xFFFFFFFFull;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT: {
         VkPhysicalDeviceTransformFeedbackPropertiesEXT *properties =
            (VkPhysicalDeviceTransformFeedbackPropertiesEXT *)ext;

         properties->maxTransformFeedbackStreams = IR3_MAX_SO_STREAMS;
         properties->maxTransformFeedbackBuffers = IR3_MAX_SO_BUFFERS;
         properties->maxTransformFeedbackBufferSize = UINT32_MAX;
         properties->maxTransformFeedbackStreamDataSize = 512;
         properties->maxTransformFeedbackBufferDataSize = 512;
         properties->maxTransformFeedbackBufferDataStride = 512;
         properties->transformFeedbackQueries = true;
         properties->transformFeedbackStreamsLinesTriangles = true;
         properties->transformFeedbackRasterizationStreamSelect = true;
         properties->transformFeedbackDraw = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT: {
         VkPhysicalDeviceSampleLocationsPropertiesEXT *properties =
            (VkPhysicalDeviceSampleLocationsPropertiesEXT *)ext;
         properties->sampleLocationSampleCounts = 0;
         if (pdevice->supported_extensions.EXT_sample_locations) {
            properties->sampleLocationSampleCounts =
               VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT;
         }
         properties->maxSampleLocationGridSize = (VkExtent2D) { 1 , 1 };
         properties->sampleLocationCoordinateRange[0] = 0.0f;
         properties->sampleLocationCoordinateRange[1] = 0.9375f;
         properties->sampleLocationSubPixelBits = 4;
         properties->variableSampleLocations = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES: {
         VkPhysicalDeviceSamplerFilterMinmaxProperties *properties =
            (VkPhysicalDeviceSamplerFilterMinmaxProperties *)ext;
         properties->filterMinmaxImageComponentMapping = true;
         properties->filterMinmaxSingleComponentFormats = true;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES: {
         VkPhysicalDeviceSubgroupProperties *properties =
            (VkPhysicalDeviceSubgroupProperties *)ext;
         properties->subgroupSize = 64;
         properties->supportedStages = VK_SHADER_STAGE_COMPUTE_BIT;
         properties->supportedOperations = VK_SUBGROUP_FEATURE_BASIC_BIT |
                                           VK_SUBGROUP_FEATURE_VOTE_BIT;
         properties->quadOperationsInAllStages = false;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT: {
         VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT *props =
            (VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT *)ext;
         props->maxVertexAttribDivisor = UINT32_MAX;
         break;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_PROPERTIES_EXT: {
         VkPhysicalDeviceCustomBorderColorPropertiesEXT *props = (void *)ext;
         props->maxCustomBorderColorSamplers = TU_BORDER_COLOR_COUNT;
         break;
      }
      default:
         break;
      }
   }
}

static const VkQueueFamilyProperties tu_queue_family_properties = {
   .queueFlags =
      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT,
   .queueCount = 1,
   .timestampValidBits = 48,
   .minImageTransferGranularity = { 1, 1, 1 },
};

void
tu_GetPhysicalDeviceQueueFamilyProperties2(
   VkPhysicalDevice physicalDevice,
   uint32_t *pQueueFamilyPropertyCount,
   VkQueueFamilyProperties2 *pQueueFamilyProperties)
{
   VK_OUTARRAY_MAKE(out, pQueueFamilyProperties, pQueueFamilyPropertyCount);

   vk_outarray_append(&out, p)
   {
      p->queueFamilyProperties = tu_queue_family_properties;
   }
}

static uint64_t
tu_get_system_heap_size()
{
   struct sysinfo info;
   sysinfo(&info);

   uint64_t total_ram = (uint64_t) info.totalram * (uint64_t) info.mem_unit;

   /* We don't want to burn too much ram with the GPU.  If the user has 4GiB
    * or less, we use at most half.  If they have more than 4GiB, we use 3/4.
    */
   uint64_t available_ram;
   if (total_ram <= 4ull * 1024ull * 1024ull * 1024ull)
      available_ram = total_ram / 2;
   else
      available_ram = total_ram * 3 / 4;

   return available_ram;
}

void
tu_GetPhysicalDeviceMemoryProperties2(VkPhysicalDevice pdev,
                                      VkPhysicalDeviceMemoryProperties2 *props2)
{
   VkPhysicalDeviceMemoryProperties *props = &props2->memoryProperties;

   props->memoryHeapCount = 1;
   props->memoryHeaps[0].size = tu_get_system_heap_size();
   props->memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;

   props->memoryTypeCount = 1;
   props->memoryTypes[0].propertyFlags =
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
   props->memoryTypes[0].heapIndex = 0;
}

static VkResult
tu_queue_init(struct tu_device *device,
              struct tu_queue *queue,
              uint32_t queue_family_index,
              int idx,
              VkDeviceQueueCreateFlags flags)
{
   vk_object_base_init(&device->vk, &queue->base, VK_OBJECT_TYPE_QUEUE);

   queue->device = device;
   queue->queue_family_index = queue_family_index;
   queue->queue_idx = idx;
   queue->flags = flags;

   int ret = tu_drm_submitqueue_new(device, 0, &queue->msm_queue_id);
   if (ret)
      return vk_startup_errorf(device->instance, VK_ERROR_INITIALIZATION_FAILED,
                               "submitqueue create failed");

   queue->fence = -1;

   return VK_SUCCESS;
}

static void
tu_queue_finish(struct tu_queue *queue)
{
   if (queue->fence >= 0)
      close(queue->fence);
   tu_drm_submitqueue_close(queue->device, queue->msm_queue_id);
}

static int
tu_get_device_extension_index(const char *name)
{
   for (unsigned i = 0; i < TU_DEVICE_EXTENSION_COUNT; ++i) {
      if (strcmp(name, tu_device_extensions[i].extensionName) == 0)
         return i;
   }
   return -1;
}

VkResult
tu_CreateDevice(VkPhysicalDevice physicalDevice,
                const VkDeviceCreateInfo *pCreateInfo,
                const VkAllocationCallbacks *pAllocator,
                VkDevice *pDevice)
{
   TU_FROM_HANDLE(tu_physical_device, physical_device, physicalDevice);
   VkResult result;
   struct tu_device *device;
   bool custom_border_colors = false;

   /* Check enabled features */
   if (pCreateInfo->pEnabledFeatures) {
      VkPhysicalDeviceFeatures supported_features;
      tu_GetPhysicalDeviceFeatures(physicalDevice, &supported_features);
      VkBool32 *supported_feature = (VkBool32 *) &supported_features;
      VkBool32 *enabled_feature = (VkBool32 *) pCreateInfo->pEnabledFeatures;
      unsigned num_features =
         sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);
      for (uint32_t i = 0; i < num_features; i++) {
         if (enabled_feature[i] && !supported_feature[i])
            return vk_startup_errorf(physical_device->instance,
                                     VK_ERROR_FEATURE_NOT_PRESENT,
                                     "Missing feature bit %d\n", i);
      }
   }

   vk_foreach_struct_const(ext, pCreateInfo->pNext) {
      switch (ext->sType) {
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT: {
         const VkPhysicalDeviceCustomBorderColorFeaturesEXT *border_color_features = (const void *)ext;
         custom_border_colors = border_color_features->customBorderColors;
         break;
      }
      default:
         break;
      }
   }

   device = vk_zalloc2(&physical_device->instance->alloc, pAllocator,
                       sizeof(*device), 8, VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
   if (!device)
      return vk_startup_errorf(physical_device->instance, VK_ERROR_OUT_OF_HOST_MEMORY, "OOM");

   vk_device_init(&device->vk, pCreateInfo,
         &physical_device->instance->alloc, pAllocator);

   device->instance = physical_device->instance;
   device->physical_device = physical_device;
   device->fd = physical_device->local_fd;
   device->_lost = false;

   mtx_init(&device->bo_mutex, mtx_plain);

   for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; i++) {
      const char *ext_name = pCreateInfo->ppEnabledExtensionNames[i];
      int index = tu_get_device_extension_index(ext_name);
      if (index < 0 ||
          !physical_device->supported_extensions.extensions[index]) {
         vk_free(&device->vk.alloc, device);
         return vk_startup_errorf(physical_device->instance,
                                  VK_ERROR_EXTENSION_NOT_PRESENT,
                                  "Missing device extension '%s'", ext_name);
      }

      device->enabled_extensions.extensions[index] = true;
   }

   for (unsigned i = 0; i < pCreateInfo->queueCreateInfoCount; i++) {
      const VkDeviceQueueCreateInfo *queue_create =
         &pCreateInfo->pQueueCreateInfos[i];
      uint32_t qfi = queue_create->queueFamilyIndex;
      device->queues[qfi] = vk_alloc(
         &device->vk.alloc, queue_create->queueCount * sizeof(struct tu_queue),
         8, VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
      if (!device->queues[qfi]) {
         result = vk_startup_errorf(physical_device->instance,
                                    VK_ERROR_OUT_OF_HOST_MEMORY,
                                    "OOM");
         goto fail_queues;
      }

      memset(device->queues[qfi], 0,
             queue_create->queueCount * sizeof(struct tu_queue));

      device->queue_count[qfi] = queue_create->queueCount;

      for (unsigned q = 0; q < queue_create->queueCount; q++) {
         result = tu_queue_init(device, &device->queues[qfi][q], qfi, q,
                                queue_create->flags);
         if (result != VK_SUCCESS)
            goto fail_queues;
      }
   }

   device->compiler = ir3_compiler_create(NULL, physical_device->gpu_id);
   if (!device->compiler) {
      result = vk_startup_errorf(physical_device->instance,
                                 VK_ERROR_INITIALIZATION_FAILED,
                                 "failed to initialize ir3 compiler");
      goto fail_queues;
   }

   /* initial sizes, these will increase if there is overflow */
   device->vsc_draw_strm_pitch = 0x1000 + VSC_PAD;
   device->vsc_prim_strm_pitch = 0x4000 + VSC_PAD;

   uint32_t global_size = sizeof(struct tu6_global);
   if (custom_border_colors)
      global_size += TU_BORDER_COLOR_COUNT * sizeof(struct bcolor_entry);

   result = tu_bo_init_new(device, &device->global_bo, global_size, false);
   if (result != VK_SUCCESS) {
      vk_startup_errorf(device->instance, result, "BO init");
      goto fail_global_bo;
   }

   result = tu_bo_map(device, &device->global_bo);
   if (result != VK_SUCCESS) {
      vk_startup_errorf(device->instance, result, "BO map");
      goto fail_global_bo_map;
   }

   struct tu6_global *global = device->global_bo.map;
   tu_init_clear_blit_shaders(device->global_bo.map);
   global->predicate = 0;
   tu6_pack_border_color(&global->bcolor_builtin[VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK],
                         &(VkClearColorValue) {}, false);
   tu6_pack_border_color(&global->bcolor_builtin[VK_BORDER_COLOR_INT_TRANSPARENT_BLACK],
                         &(VkClearColorValue) {}, true);
   tu6_pack_border_color(&global->bcolor_builtin[VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK],
                         &(VkClearColorValue) { .float32[3] = 1.0f }, false);
   tu6_pack_border_color(&global->bcolor_builtin[VK_BORDER_COLOR_INT_OPAQUE_BLACK],
                         &(VkClearColorValue) { .int32[3] = 1 }, true);
   tu6_pack_border_color(&global->bcolor_builtin[VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE],
                         &(VkClearColorValue) { .float32[0 ... 3] = 1.0f }, false);
   tu6_pack_border_color(&global->bcolor_builtin[VK_BORDER_COLOR_INT_OPAQUE_WHITE],
                         &(VkClearColorValue) { .int32[0 ... 3] = 1 }, true);

   /* initialize to ones so ffs can be used to find unused slots */
   BITSET_ONES(device->custom_border_color);

   VkPipelineCacheCreateInfo ci;
   ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
   ci.pNext = NULL;
   ci.flags = 0;
   ci.pInitialData = NULL;
   ci.initialDataSize = 0;
   VkPipelineCache pc;
   result =
      tu_CreatePipelineCache(tu_device_to_handle(device), &ci, NULL, &pc);
   if (result != VK_SUCCESS) {
      vk_startup_errorf(device->instance, result, "create pipeline cache failed");
      goto fail_pipeline_cache;
   }

   device->mem_cache = tu_pipeline_cache_from_handle(pc);

   for (unsigned i = 0; i < ARRAY_SIZE(device->scratch_bos); i++)
      mtx_init(&device->scratch_bos[i].construct_mtx, mtx_plain);

   mtx_init(&device->mutex, mtx_plain);

   *pDevice = tu_device_to_handle(device);
   return VK_SUCCESS;

fail_pipeline_cache:
fail_global_bo_map:
   tu_bo_finish(device, &device->global_bo);

fail_global_bo:
   ir3_compiler_destroy(device->compiler);

fail_queues:
   for (unsigned i = 0; i < TU_MAX_QUEUE_FAMILIES; i++) {
      for (unsigned q = 0; q < device->queue_count[i]; q++)
         tu_queue_finish(&device->queues[i][q]);
      if (device->queue_count[i])
         vk_object_free(&device->vk, NULL, device->queues[i]);
   }

   vk_free(&device->vk.alloc, device);
   return result;
}

void
tu_DestroyDevice(VkDevice _device, const VkAllocationCallbacks *pAllocator)
{
   TU_FROM_HANDLE(tu_device, device, _device);

   if (!device)
      return;

   for (unsigned i = 0; i < TU_MAX_QUEUE_FAMILIES; i++) {
      for (unsigned q = 0; q < device->queue_count[i]; q++)
         tu_queue_finish(&device->queues[i][q]);
      if (device->queue_count[i])
         vk_object_free(&device->vk, NULL, device->queues[i]);
   }

   for (unsigned i = 0; i < ARRAY_SIZE(device->scratch_bos); i++) {
      if (device->scratch_bos[i].initialized)
         tu_bo_finish(device, &device->scratch_bos[i].bo);
   }

   ir3_compiler_destroy(device->compiler);

   VkPipelineCache pc = tu_pipeline_cache_to_handle(device->mem_cache);
   tu_DestroyPipelineCache(tu_device_to_handle(device), pc, NULL);

   vk_free(&device->vk.alloc, device->bo_list);
   vk_free(&device->vk.alloc, device->bo_idx);
   vk_free(&device->vk.alloc, device);
}

VkResult
_tu_device_set_lost(struct tu_device *device,
                    const char *msg, ...)
{
   /* Set the flag indicating that waits should return in finite time even
    * after device loss.
    */
   p_atomic_inc(&device->_lost);

   /* TODO: Report the log message through VkDebugReportCallbackEXT instead */
   va_list ap;
   va_start(ap, msg);
   mesa_loge_v(msg, ap);
   va_end(ap);

   if (env_var_as_boolean("TU_ABORT_ON_DEVICE_LOSS", false))
      abort();

   return VK_ERROR_DEVICE_LOST;
}

VkResult
tu_get_scratch_bo(struct tu_device *dev, uint64_t size, struct tu_bo **bo)
{
   unsigned size_log2 = MAX2(util_logbase2_ceil64(size), MIN_SCRATCH_BO_SIZE_LOG2);
   unsigned index = size_log2 - MIN_SCRATCH_BO_SIZE_LOG2;
   assert(index < ARRAY_SIZE(dev->scratch_bos));

   for (unsigned i = index; i < ARRAY_SIZE(dev->scratch_bos); i++) {
      if (p_atomic_read(&dev->scratch_bos[i].initialized)) {
         /* Fast path: just return the already-allocated BO. */
         *bo = &dev->scratch_bos[i].bo;
         return VK_SUCCESS;
      }
   }

   /* Slow path: actually allocate the BO. We take a lock because the process
    * of allocating it is slow, and we don't want to block the CPU while it
    * finishes.
   */
   mtx_lock(&dev->scratch_bos[index].construct_mtx);

   /* Another thread may have allocated it already while we were waiting on
    * the lock. We need to check this in order to avoid double-allocating.
    */
   if (dev->scratch_bos[index].initialized) {
      mtx_unlock(&dev->scratch_bos[index].construct_mtx);
      *bo = &dev->scratch_bos[index].bo;
      return VK_SUCCESS;
   }

   unsigned bo_size = 1ull << size_log2;
   VkResult result = tu_bo_init_new(dev, &dev->scratch_bos[index].bo, bo_size, false);
   if (result != VK_SUCCESS) {
      mtx_unlock(&dev->scratch_bos[index].construct_mtx);
      return result;
   }

   p_atomic_set(&dev->scratch_bos[index].initialized, true);

   mtx_unlock(&dev->scratch_bos[index].construct_mtx);

   *bo = &dev->scratch_bos[index].bo;
   return VK_SUCCESS;
}

VkResult
tu_EnumerateInstanceLayerProperties(uint32_t *pPropertyCount,
                                    VkLayerProperties *pProperties)
{
   *pPropertyCount = 0;
   return VK_SUCCESS;
}

VkResult
tu_EnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice,
                                  uint32_t *pPropertyCount,
                                  VkLayerProperties *pProperties)
{
   *pPropertyCount = 0;
   return VK_SUCCESS;
}

void
tu_GetDeviceQueue2(VkDevice _device,
                   const VkDeviceQueueInfo2 *pQueueInfo,
                   VkQueue *pQueue)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   struct tu_queue *queue;

   queue =
      &device->queues[pQueueInfo->queueFamilyIndex][pQueueInfo->queueIndex];
   if (pQueueInfo->flags != queue->flags) {
      /* From the Vulkan 1.1.70 spec:
       *
       * "The queue returned by vkGetDeviceQueue2 must have the same
       * flags value from this structure as that used at device
       * creation time in a VkDeviceQueueCreateInfo instance. If no
       * matching flags were specified at device creation time then
       * pQueue will return VK_NULL_HANDLE."
       */
      *pQueue = VK_NULL_HANDLE;
      return;
   }

   *pQueue = tu_queue_to_handle(queue);
}

VkResult
tu_QueueWaitIdle(VkQueue _queue)
{
   TU_FROM_HANDLE(tu_queue, queue, _queue);

   if (tu_device_is_lost(queue->device))
      return VK_ERROR_DEVICE_LOST;

   if (queue->fence < 0)
      return VK_SUCCESS;

   struct pollfd fds = { .fd = queue->fence, .events = POLLIN };
   int ret;
   do {
      ret = poll(&fds, 1, -1);
   } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

   /* TODO: otherwise set device lost ? */
   assert(ret == 1 && !(fds.revents & (POLLERR | POLLNVAL)));

   close(queue->fence);
   queue->fence = -1;
   return VK_SUCCESS;
}

VkResult
tu_DeviceWaitIdle(VkDevice _device)
{
   TU_FROM_HANDLE(tu_device, device, _device);

   if (tu_device_is_lost(device))
      return VK_ERROR_DEVICE_LOST;

   for (unsigned i = 0; i < TU_MAX_QUEUE_FAMILIES; i++) {
      for (unsigned q = 0; q < device->queue_count[i]; q++) {
         tu_QueueWaitIdle(tu_queue_to_handle(&device->queues[i][q]));
      }
   }
   return VK_SUCCESS;
}

VkResult
tu_EnumerateInstanceExtensionProperties(const char *pLayerName,
                                        uint32_t *pPropertyCount,
                                        VkExtensionProperties *pProperties)
{
   VK_OUTARRAY_MAKE(out, pProperties, pPropertyCount);

   /* We spport no lyaers */
   if (pLayerName)
      return vk_error(NULL, VK_ERROR_LAYER_NOT_PRESENT);

   for (int i = 0; i < TU_INSTANCE_EXTENSION_COUNT; i++) {
      if (tu_instance_extensions_supported.extensions[i]) {
         vk_outarray_append(&out, prop) { *prop = tu_instance_extensions[i]; }
      }
   }

   return vk_outarray_status(&out);
}

VkResult
tu_EnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice,
                                      const char *pLayerName,
                                      uint32_t *pPropertyCount,
                                      VkExtensionProperties *pProperties)
{
   /* We spport no lyaers */
   TU_FROM_HANDLE(tu_physical_device, device, physicalDevice);
   VK_OUTARRAY_MAKE(out, pProperties, pPropertyCount);

   /* We spport no lyaers */
   if (pLayerName)
      return vk_error(NULL, VK_ERROR_LAYER_NOT_PRESENT);

   for (int i = 0; i < TU_DEVICE_EXTENSION_COUNT; i++) {
      if (device->supported_extensions.extensions[i]) {
         vk_outarray_append(&out, prop) { *prop = tu_device_extensions[i]; }
      }
   }

   return vk_outarray_status(&out);
}

PFN_vkVoidFunction
tu_GetInstanceProcAddr(VkInstance _instance, const char *pName)
{
   TU_FROM_HANDLE(tu_instance, instance, _instance);

   return tu_lookup_entrypoint_checked(
      pName, instance ? instance->api_version : 0,
      instance ? &instance->enabled_extensions : NULL, NULL);
}

/* The loader wants us to expose a second GetInstanceProcAddr function
 * to work around certain LD_PRELOAD issues seen in apps.
 */
PUBLIC
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vk_icdGetInstanceProcAddr(VkInstance instance, const char *pName);

PUBLIC
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vk_icdGetInstanceProcAddr(VkInstance instance, const char *pName)
{
   return tu_GetInstanceProcAddr(instance, pName);
}

PFN_vkVoidFunction
tu_GetDeviceProcAddr(VkDevice _device, const char *pName)
{
   TU_FROM_HANDLE(tu_device, device, _device);

   return tu_lookup_entrypoint_checked(pName, device->instance->api_version,
                                       &device->instance->enabled_extensions,
                                       &device->enabled_extensions);
}

VkResult
tu_AllocateMemory(VkDevice _device,
                  const VkMemoryAllocateInfo *pAllocateInfo,
                  const VkAllocationCallbacks *pAllocator,
                  VkDeviceMemory *pMem)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   struct tu_device_memory *mem;
   VkResult result;

   assert(pAllocateInfo->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);

   if (pAllocateInfo->allocationSize == 0) {
      /* Apparently, this is allowed */
      *pMem = VK_NULL_HANDLE;
      return VK_SUCCESS;
   }

   mem = vk_object_alloc(&device->vk, pAllocator, sizeof(*mem),
                         VK_OBJECT_TYPE_DEVICE_MEMORY);
   if (mem == NULL)
      return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   const VkImportMemoryFdInfoKHR *fd_info =
      vk_find_struct_const(pAllocateInfo->pNext, IMPORT_MEMORY_FD_INFO_KHR);
   if (fd_info && !fd_info->handleType)
      fd_info = NULL;

   if (fd_info) {
      assert(fd_info->handleType ==
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT ||
             fd_info->handleType ==
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);

      /*
       * TODO Importing the same fd twice gives us the same handle without
       * reference counting.  We need to maintain a per-instance handle-to-bo
       * table and add reference count to tu_bo.
       */
      result = tu_bo_init_dmabuf(device, &mem->bo,
                                 pAllocateInfo->allocationSize, fd_info->fd);
      if (result == VK_SUCCESS) {
         /* take ownership and close the fd */
         close(fd_info->fd);
      }
   } else {
      result =
         tu_bo_init_new(device, &mem->bo, pAllocateInfo->allocationSize, false);
   }

   if (result != VK_SUCCESS) {
      vk_object_free(&device->vk, pAllocator, mem);
      return result;
   }

   *pMem = tu_device_memory_to_handle(mem);

   return VK_SUCCESS;
}

void
tu_FreeMemory(VkDevice _device,
              VkDeviceMemory _mem,
              const VkAllocationCallbacks *pAllocator)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   TU_FROM_HANDLE(tu_device_memory, mem, _mem);

   if (mem == NULL)
      return;

   tu_bo_finish(device, &mem->bo);
   vk_object_free(&device->vk, pAllocator, mem);
}

VkResult
tu_MapMemory(VkDevice _device,
             VkDeviceMemory _memory,
             VkDeviceSize offset,
             VkDeviceSize size,
             VkMemoryMapFlags flags,
             void **ppData)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   TU_FROM_HANDLE(tu_device_memory, mem, _memory);
   VkResult result;

   if (mem == NULL) {
      *ppData = NULL;
      return VK_SUCCESS;
   }

   if (!mem->bo.map) {
      result = tu_bo_map(device, &mem->bo);
      if (result != VK_SUCCESS)
         return result;
   }

   *ppData = mem->bo.map + offset;
   return VK_SUCCESS;
}

void
tu_UnmapMemory(VkDevice _device, VkDeviceMemory _memory)
{
   /* TODO: unmap here instead of waiting for FreeMemory */
}

VkResult
tu_FlushMappedMemoryRanges(VkDevice _device,
                           uint32_t memoryRangeCount,
                           const VkMappedMemoryRange *pMemoryRanges)
{
   return VK_SUCCESS;
}

VkResult
tu_InvalidateMappedMemoryRanges(VkDevice _device,
                                uint32_t memoryRangeCount,
                                const VkMappedMemoryRange *pMemoryRanges)
{
   return VK_SUCCESS;
}

void
tu_GetBufferMemoryRequirements2(
   VkDevice device,
   const VkBufferMemoryRequirementsInfo2 *pInfo,
   VkMemoryRequirements2 *pMemoryRequirements)
{
   TU_FROM_HANDLE(tu_buffer, buffer, pInfo->buffer);

   pMemoryRequirements->memoryRequirements = (VkMemoryRequirements) {
      .memoryTypeBits = 1,
      .alignment = 64,
      .size = align64(buffer->size, 64),
   };
}

void
tu_GetImageMemoryRequirements2(VkDevice device,
                               const VkImageMemoryRequirementsInfo2 *pInfo,
                               VkMemoryRequirements2 *pMemoryRequirements)
{
   TU_FROM_HANDLE(tu_image, image, pInfo->image);

   pMemoryRequirements->memoryRequirements = (VkMemoryRequirements) {
      .memoryTypeBits = 1,
      .alignment = image->layout[0].base_align,
      .size = image->total_size
   };
}

void
tu_GetImageSparseMemoryRequirements2(
   VkDevice device,
   const VkImageSparseMemoryRequirementsInfo2 *pInfo,
   uint32_t *pSparseMemoryRequirementCount,
   VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements)
{
   tu_stub();
}

void
tu_GetDeviceMemoryCommitment(VkDevice device,
                             VkDeviceMemory memory,
                             VkDeviceSize *pCommittedMemoryInBytes)
{
   *pCommittedMemoryInBytes = 0;
}

VkResult
tu_BindBufferMemory2(VkDevice device,
                     uint32_t bindInfoCount,
                     const VkBindBufferMemoryInfo *pBindInfos)
{
   for (uint32_t i = 0; i < bindInfoCount; ++i) {
      TU_FROM_HANDLE(tu_device_memory, mem, pBindInfos[i].memory);
      TU_FROM_HANDLE(tu_buffer, buffer, pBindInfos[i].buffer);

      if (mem) {
         buffer->bo = &mem->bo;
         buffer->bo_offset = pBindInfos[i].memoryOffset;
      } else {
         buffer->bo = NULL;
      }
   }
   return VK_SUCCESS;
}

VkResult
tu_BindImageMemory2(VkDevice device,
                    uint32_t bindInfoCount,
                    const VkBindImageMemoryInfo *pBindInfos)
{
   for (uint32_t i = 0; i < bindInfoCount; ++i) {
      TU_FROM_HANDLE(tu_image, image, pBindInfos[i].image);
      TU_FROM_HANDLE(tu_device_memory, mem, pBindInfos[i].memory);

      if (mem) {
         image->bo = &mem->bo;
         image->bo_offset = pBindInfos[i].memoryOffset;
      } else {
         image->bo = NULL;
         image->bo_offset = 0;
      }
   }

   return VK_SUCCESS;
}

VkResult
tu_QueueBindSparse(VkQueue _queue,
                   uint32_t bindInfoCount,
                   const VkBindSparseInfo *pBindInfo,
                   VkFence _fence)
{
   return VK_SUCCESS;
}

VkResult
tu_CreateEvent(VkDevice _device,
               const VkEventCreateInfo *pCreateInfo,
               const VkAllocationCallbacks *pAllocator,
               VkEvent *pEvent)
{
   TU_FROM_HANDLE(tu_device, device, _device);

   struct tu_event *event =
         vk_object_alloc(&device->vk, pAllocator, sizeof(*event),
                         VK_OBJECT_TYPE_EVENT);
   if (!event)
      return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   VkResult result = tu_bo_init_new(device, &event->bo, 0x1000, false);
   if (result != VK_SUCCESS)
      goto fail_alloc;

   result = tu_bo_map(device, &event->bo);
   if (result != VK_SUCCESS)
      goto fail_map;

   *pEvent = tu_event_to_handle(event);

   return VK_SUCCESS;

fail_map:
   tu_bo_finish(device, &event->bo);
fail_alloc:
   vk_object_free(&device->vk, pAllocator, event);
   return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);
}

void
tu_DestroyEvent(VkDevice _device,
                VkEvent _event,
                const VkAllocationCallbacks *pAllocator)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   TU_FROM_HANDLE(tu_event, event, _event);

   if (!event)
      return;

   tu_bo_finish(device, &event->bo);
   vk_object_free(&device->vk, pAllocator, event);
}

VkResult
tu_GetEventStatus(VkDevice _device, VkEvent _event)
{
   TU_FROM_HANDLE(tu_event, event, _event);

   if (*(uint64_t*) event->bo.map == 1)
      return VK_EVENT_SET;
   return VK_EVENT_RESET;
}

VkResult
tu_SetEvent(VkDevice _device, VkEvent _event)
{
   TU_FROM_HANDLE(tu_event, event, _event);
   *(uint64_t*) event->bo.map = 1;

   return VK_SUCCESS;
}

VkResult
tu_ResetEvent(VkDevice _device, VkEvent _event)
{
   TU_FROM_HANDLE(tu_event, event, _event);
   *(uint64_t*) event->bo.map = 0;

   return VK_SUCCESS;
}

VkResult
tu_CreateBuffer(VkDevice _device,
                const VkBufferCreateInfo *pCreateInfo,
                const VkAllocationCallbacks *pAllocator,
                VkBuffer *pBuffer)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   struct tu_buffer *buffer;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);

   buffer = vk_object_alloc(&device->vk, pAllocator, sizeof(*buffer),
                            VK_OBJECT_TYPE_BUFFER);
   if (buffer == NULL)
      return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   buffer->size = pCreateInfo->size;
   buffer->usage = pCreateInfo->usage;
   buffer->flags = pCreateInfo->flags;

   *pBuffer = tu_buffer_to_handle(buffer);

   return VK_SUCCESS;
}

void
tu_DestroyBuffer(VkDevice _device,
                 VkBuffer _buffer,
                 const VkAllocationCallbacks *pAllocator)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   TU_FROM_HANDLE(tu_buffer, buffer, _buffer);

   if (!buffer)
      return;

   vk_object_free(&device->vk, pAllocator, buffer);
}

VkResult
tu_CreateFramebuffer(VkDevice _device,
                     const VkFramebufferCreateInfo *pCreateInfo,
                     const VkAllocationCallbacks *pAllocator,
                     VkFramebuffer *pFramebuffer)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   TU_FROM_HANDLE(tu_render_pass, pass, pCreateInfo->renderPass);
   struct tu_framebuffer *framebuffer;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);

   size_t size = sizeof(*framebuffer) + sizeof(struct tu_attachment_info) *
                                           pCreateInfo->attachmentCount;
   framebuffer = vk_object_alloc(&device->vk, pAllocator, size,
                                 VK_OBJECT_TYPE_FRAMEBUFFER);
   if (framebuffer == NULL)
      return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   framebuffer->attachment_count = pCreateInfo->attachmentCount;
   framebuffer->width = pCreateInfo->width;
   framebuffer->height = pCreateInfo->height;
   framebuffer->layers = pCreateInfo->layers;
   for (uint32_t i = 0; i < pCreateInfo->attachmentCount; i++) {
      VkImageView _iview = pCreateInfo->pAttachments[i];
      struct tu_image_view *iview = tu_image_view_from_handle(_iview);
      framebuffer->attachments[i].attachment = iview;
   }

   tu_framebuffer_tiling_config(framebuffer, device, pass);

   *pFramebuffer = tu_framebuffer_to_handle(framebuffer);
   return VK_SUCCESS;
}

void
tu_DestroyFramebuffer(VkDevice _device,
                      VkFramebuffer _fb,
                      const VkAllocationCallbacks *pAllocator)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   TU_FROM_HANDLE(tu_framebuffer, fb, _fb);

   if (!fb)
      return;

   vk_object_free(&device->vk, pAllocator, fb);
}

static void
tu_init_sampler(struct tu_device *device,
                struct tu_sampler *sampler,
                const VkSamplerCreateInfo *pCreateInfo)
{
   const struct VkSamplerReductionModeCreateInfo *reduction =
      vk_find_struct_const(pCreateInfo->pNext, SAMPLER_REDUCTION_MODE_CREATE_INFO);
   const struct VkSamplerYcbcrConversionInfo *ycbcr_conversion =
      vk_find_struct_const(pCreateInfo->pNext,  SAMPLER_YCBCR_CONVERSION_INFO);
   const VkSamplerCustomBorderColorCreateInfoEXT *custom_border_color =
      vk_find_struct_const(pCreateInfo->pNext, SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT);
   /* for non-custom border colors, the VK enum is translated directly to an offset in
    * the border color buffer. custom border colors are located immediately after the
    * builtin colors, and thus an offset of TU_BORDER_COLOR_BUILTIN is added.
    */
   uint32_t border_color = (unsigned) pCreateInfo->borderColor;
   if (pCreateInfo->borderColor == VK_BORDER_COLOR_FLOAT_CUSTOM_EXT ||
       pCreateInfo->borderColor == VK_BORDER_COLOR_INT_CUSTOM_EXT) {
      mtx_lock(&device->mutex);
      border_color = BITSET_FFS(device->custom_border_color);
      BITSET_CLEAR(device->custom_border_color, border_color);
      mtx_unlock(&device->mutex);
      tu6_pack_border_color(device->global_bo.map + gb_offset(bcolor[border_color]),
                            &custom_border_color->customBorderColor,
                            pCreateInfo->borderColor == VK_BORDER_COLOR_INT_CUSTOM_EXT);
      border_color += TU_BORDER_COLOR_BUILTIN;
   }

   unsigned aniso = pCreateInfo->anisotropyEnable ?
      util_last_bit(MIN2((uint32_t)pCreateInfo->maxAnisotropy >> 1, 8)) : 0;
   bool miplinear = (pCreateInfo->mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR);
   float min_lod = CLAMP(pCreateInfo->minLod, 0.0f, 4095.0f / 256.0f);
   float max_lod = CLAMP(pCreateInfo->maxLod, 0.0f, 4095.0f / 256.0f);

   sampler->descriptor[0] =
      COND(miplinear, A6XX_TEX_SAMP_0_MIPFILTER_LINEAR_NEAR) |
      A6XX_TEX_SAMP_0_XY_MAG(tu6_tex_filter(pCreateInfo->magFilter, aniso)) |
      A6XX_TEX_SAMP_0_XY_MIN(tu6_tex_filter(pCreateInfo->minFilter, aniso)) |
      A6XX_TEX_SAMP_0_ANISO(aniso) |
      A6XX_TEX_SAMP_0_WRAP_S(tu6_tex_wrap(pCreateInfo->addressModeU)) |
      A6XX_TEX_SAMP_0_WRAP_T(tu6_tex_wrap(pCreateInfo->addressModeV)) |
      A6XX_TEX_SAMP_0_WRAP_R(tu6_tex_wrap(pCreateInfo->addressModeW)) |
      A6XX_TEX_SAMP_0_LOD_BIAS(pCreateInfo->mipLodBias);
   sampler->descriptor[1] =
      /* COND(!cso->seamless_cube_map, A6XX_TEX_SAMP_1_CUBEMAPSEAMLESSFILTOFF) | */
      COND(pCreateInfo->unnormalizedCoordinates, A6XX_TEX_SAMP_1_UNNORM_COORDS) |
      A6XX_TEX_SAMP_1_MIN_LOD(min_lod) |
      A6XX_TEX_SAMP_1_MAX_LOD(max_lod) |
      COND(pCreateInfo->compareEnable,
           A6XX_TEX_SAMP_1_COMPARE_FUNC(tu6_compare_func(pCreateInfo->compareOp)));
   sampler->descriptor[2] = A6XX_TEX_SAMP_2_BCOLOR(border_color);
   sampler->descriptor[3] = 0;

   if (reduction) {
      sampler->descriptor[2] |= A6XX_TEX_SAMP_2_REDUCTION_MODE(
         tu6_reduction_mode(reduction->reductionMode));
   }

   sampler->ycbcr_sampler = ycbcr_conversion ?
      tu_sampler_ycbcr_conversion_from_handle(ycbcr_conversion->conversion) : NULL;

   if (sampler->ycbcr_sampler &&
       sampler->ycbcr_sampler->chroma_filter == VK_FILTER_LINEAR) {
      sampler->descriptor[2] |= A6XX_TEX_SAMP_2_CHROMA_LINEAR;
   }

   /* TODO:
    * A6XX_TEX_SAMP_1_MIPFILTER_LINEAR_FAR disables mipmapping, but vk has no NONE mipfilter?
    */
}

VkResult
tu_CreateSampler(VkDevice _device,
                 const VkSamplerCreateInfo *pCreateInfo,
                 const VkAllocationCallbacks *pAllocator,
                 VkSampler *pSampler)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   struct tu_sampler *sampler;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);

   sampler = vk_object_alloc(&device->vk, pAllocator, sizeof(*sampler),
                             VK_OBJECT_TYPE_SAMPLER);
   if (!sampler)
      return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   tu_init_sampler(device, sampler, pCreateInfo);
   *pSampler = tu_sampler_to_handle(sampler);

   return VK_SUCCESS;
}

void
tu_DestroySampler(VkDevice _device,
                  VkSampler _sampler,
                  const VkAllocationCallbacks *pAllocator)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   TU_FROM_HANDLE(tu_sampler, sampler, _sampler);
   uint32_t border_color;

   if (!sampler)
      return;

   border_color = (sampler->descriptor[2] & A6XX_TEX_SAMP_2_BCOLOR__MASK) >> A6XX_TEX_SAMP_2_BCOLOR__SHIFT;
   if (border_color >= TU_BORDER_COLOR_BUILTIN) {
      border_color -= TU_BORDER_COLOR_BUILTIN;
      /* if the sampler had a custom border color, free it. TODO: no lock */
      mtx_lock(&device->mutex);
      assert(!BITSET_TEST(device->custom_border_color, border_color));
      BITSET_SET(device->custom_border_color, border_color);
      mtx_unlock(&device->mutex);
   }

   vk_object_free(&device->vk, pAllocator, sampler);
}

/* vk_icd.h does not declare this function, so we declare it here to
 * suppress Wmissing-prototypes.
 */
PUBLIC VKAPI_ATTR VkResult VKAPI_CALL
vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t *pSupportedVersion);

PUBLIC VKAPI_ATTR VkResult VKAPI_CALL
vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t *pSupportedVersion)
{
   /* For the full details on loader interface versioning, see
    * <https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/blob/master/loader/LoaderAndLayerInterface.md>.
    * What follows is a condensed summary, to help you navigate the large and
    * confusing official doc.
    *
    *   - Loader interface v0 is incompatible with later versions. We don't
    *     support it.
    *
    *   - In loader interface v1:
    *       - The first ICD entrypoint called by the loader is
    *         vk_icdGetInstanceProcAddr(). The ICD must statically expose this
    *         entrypoint.
    *       - The ICD must statically expose no other Vulkan symbol unless it
    * is linked with -Bsymbolic.
    *       - Each dispatchable Vulkan handle created by the ICD must be
    *         a pointer to a struct whose first member is VK_LOADER_DATA. The
    *         ICD must initialize VK_LOADER_DATA.loadMagic to
    * ICD_LOADER_MAGIC.
    *       - The loader implements vkCreate{PLATFORM}SurfaceKHR() and
    *         vkDestroySurfaceKHR(). The ICD must be capable of working with
    *         such loader-managed surfaces.
    *
    *    - Loader interface v2 differs from v1 in:
    *       - The first ICD entrypoint called by the loader is
    *         vk_icdNegotiateLoaderICDInterfaceVersion(). The ICD must
    *         statically expose this entrypoint.
    *
    *    - Loader interface v3 differs from v2 in:
    *        - The ICD must implement vkCreate{PLATFORM}SurfaceKHR(),
    *          vkDestroySurfaceKHR(), and other API which uses VKSurfaceKHR,
    *          because the loader no longer does so.
    */
   *pSupportedVersion = MIN2(*pSupportedVersion, 3u);
   return VK_SUCCESS;
}

VkResult
tu_GetMemoryFdKHR(VkDevice _device,
                  const VkMemoryGetFdInfoKHR *pGetFdInfo,
                  int *pFd)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   TU_FROM_HANDLE(tu_device_memory, memory, pGetFdInfo->memory);

   assert(pGetFdInfo->sType == VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR);

   /* At the moment, we support only the below handle types. */
   assert(pGetFdInfo->handleType ==
             VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT ||
          pGetFdInfo->handleType ==
             VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);

   int prime_fd = tu_bo_export_dmabuf(device, &memory->bo);
   if (prime_fd < 0)
      return vk_error(device->instance, VK_ERROR_OUT_OF_DEVICE_MEMORY);

   *pFd = prime_fd;
   return VK_SUCCESS;
}

VkResult
tu_GetMemoryFdPropertiesKHR(VkDevice _device,
                            VkExternalMemoryHandleTypeFlagBits handleType,
                            int fd,
                            VkMemoryFdPropertiesKHR *pMemoryFdProperties)
{
   assert(handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);
   pMemoryFdProperties->memoryTypeBits = 1;
   return VK_SUCCESS;
}

void
tu_GetPhysicalDeviceExternalFenceProperties(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceExternalFenceInfo *pExternalFenceInfo,
   VkExternalFenceProperties *pExternalFenceProperties)
{
   pExternalFenceProperties->exportFromImportedHandleTypes = 0;
   pExternalFenceProperties->compatibleHandleTypes = 0;
   pExternalFenceProperties->externalFenceFeatures = 0;
}

VkResult
tu_CreateDebugReportCallbackEXT(
   VkInstance _instance,
   const VkDebugReportCallbackCreateInfoEXT *pCreateInfo,
   const VkAllocationCallbacks *pAllocator,
   VkDebugReportCallbackEXT *pCallback)
{
   TU_FROM_HANDLE(tu_instance, instance, _instance);
   return vk_create_debug_report_callback(&instance->debug_report_callbacks,
                                          pCreateInfo, pAllocator,
                                          &instance->alloc, pCallback);
}

void
tu_DestroyDebugReportCallbackEXT(VkInstance _instance,
                                 VkDebugReportCallbackEXT _callback,
                                 const VkAllocationCallbacks *pAllocator)
{
   TU_FROM_HANDLE(tu_instance, instance, _instance);
   vk_destroy_debug_report_callback(&instance->debug_report_callbacks,
                                    _callback, pAllocator, &instance->alloc);
}

void
tu_DebugReportMessageEXT(VkInstance _instance,
                         VkDebugReportFlagsEXT flags,
                         VkDebugReportObjectTypeEXT objectType,
                         uint64_t object,
                         size_t location,
                         int32_t messageCode,
                         const char *pLayerPrefix,
                         const char *pMessage)
{
   TU_FROM_HANDLE(tu_instance, instance, _instance);
   vk_debug_report(&instance->debug_report_callbacks, flags, objectType,
                   object, location, messageCode, pLayerPrefix, pMessage);
}

void
tu_GetDeviceGroupPeerMemoryFeatures(
   VkDevice device,
   uint32_t heapIndex,
   uint32_t localDeviceIndex,
   uint32_t remoteDeviceIndex,
   VkPeerMemoryFeatureFlags *pPeerMemoryFeatures)
{
   assert(localDeviceIndex == remoteDeviceIndex);

   *pPeerMemoryFeatures = VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT |
                          VK_PEER_MEMORY_FEATURE_COPY_DST_BIT |
                          VK_PEER_MEMORY_FEATURE_GENERIC_SRC_BIT |
                          VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT;
}

void tu_GetPhysicalDeviceMultisamplePropertiesEXT(
   VkPhysicalDevice                            physicalDevice,
   VkSampleCountFlagBits                       samples,
   VkMultisamplePropertiesEXT*                 pMultisampleProperties)
{
   TU_FROM_HANDLE(tu_physical_device, pdevice, physicalDevice);

   if (samples <= VK_SAMPLE_COUNT_4_BIT && pdevice->supported_extensions.EXT_sample_locations)
      pMultisampleProperties->maxSampleLocationGridSize = (VkExtent2D){ 1, 1 };
   else
      pMultisampleProperties->maxSampleLocationGridSize = (VkExtent2D){ 0, 0 };
}


VkResult
tu_CreatePrivateDataSlotEXT(VkDevice _device,
                            const VkPrivateDataSlotCreateInfoEXT* pCreateInfo,
                            const VkAllocationCallbacks* pAllocator,
                            VkPrivateDataSlotEXT* pPrivateDataSlot)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   return vk_private_data_slot_create(&device->vk,
                                      pCreateInfo,
                                      pAllocator,
                                      pPrivateDataSlot);
}

void
tu_DestroyPrivateDataSlotEXT(VkDevice _device,
                             VkPrivateDataSlotEXT privateDataSlot,
                             const VkAllocationCallbacks* pAllocator)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   vk_private_data_slot_destroy(&device->vk, privateDataSlot, pAllocator);
}

VkResult
tu_SetPrivateDataEXT(VkDevice _device,
                     VkObjectType objectType,
                     uint64_t objectHandle,
                     VkPrivateDataSlotEXT privateDataSlot,
                     uint64_t data)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   return vk_object_base_set_private_data(&device->vk,
                                          objectType,
                                          objectHandle,
                                          privateDataSlot,
                                          data);
}

void
tu_GetPrivateDataEXT(VkDevice _device,
                     VkObjectType objectType,
                     uint64_t objectHandle,
                     VkPrivateDataSlotEXT privateDataSlot,
                     uint64_t* pData)
{
   TU_FROM_HANDLE(tu_device, device, _device);
   vk_object_base_get_private_data(&device->vk,
                                   objectType,
                                   objectHandle,
                                   privateDataSlot,
                                   pData);
}
