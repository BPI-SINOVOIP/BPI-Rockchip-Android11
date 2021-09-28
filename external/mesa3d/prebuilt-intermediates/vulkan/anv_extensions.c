/*
 * Copyright 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "anv_private.h"

#include "vk_util.h"

/* Convert the VK_USE_PLATFORM_* defines to booleans */
#ifdef VK_USE_PLATFORM_DISPLAY_KHR
#   undef VK_USE_PLATFORM_DISPLAY_KHR
#   define VK_USE_PLATFORM_DISPLAY_KHR true
#else
#   define VK_USE_PLATFORM_DISPLAY_KHR false
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
#   undef VK_USE_PLATFORM_XLIB_KHR
#   define VK_USE_PLATFORM_XLIB_KHR true
#else
#   define VK_USE_PLATFORM_XLIB_KHR false
#endif
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
#   undef VK_USE_PLATFORM_XLIB_XRANDR_EXT
#   define VK_USE_PLATFORM_XLIB_XRANDR_EXT true
#else
#   define VK_USE_PLATFORM_XLIB_XRANDR_EXT false
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
#   undef VK_USE_PLATFORM_XCB_KHR
#   define VK_USE_PLATFORM_XCB_KHR true
#else
#   define VK_USE_PLATFORM_XCB_KHR false
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
#   undef VK_USE_PLATFORM_WAYLAND_KHR
#   define VK_USE_PLATFORM_WAYLAND_KHR true
#else
#   define VK_USE_PLATFORM_WAYLAND_KHR false
#endif
#ifdef VK_USE_PLATFORM_DIRECTFB_EXT
#   undef VK_USE_PLATFORM_DIRECTFB_EXT
#   define VK_USE_PLATFORM_DIRECTFB_EXT true
#else
#   define VK_USE_PLATFORM_DIRECTFB_EXT false
#endif
#ifdef VK_USE_PLATFORM_ANDROID_KHR
#   undef VK_USE_PLATFORM_ANDROID_KHR
#   define VK_USE_PLATFORM_ANDROID_KHR true
#else
#   define VK_USE_PLATFORM_ANDROID_KHR false
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
#   undef VK_USE_PLATFORM_WIN32_KHR
#   define VK_USE_PLATFORM_WIN32_KHR true
#else
#   define VK_USE_PLATFORM_WIN32_KHR false
#endif
#ifdef VK_USE_PLATFORM_VI_NN
#   undef VK_USE_PLATFORM_VI_NN
#   define VK_USE_PLATFORM_VI_NN true
#else
#   define VK_USE_PLATFORM_VI_NN false
#endif
#ifdef VK_USE_PLATFORM_IOS_MVK
#   undef VK_USE_PLATFORM_IOS_MVK
#   define VK_USE_PLATFORM_IOS_MVK true
#else
#   define VK_USE_PLATFORM_IOS_MVK false
#endif
#ifdef VK_USE_PLATFORM_MACOS_MVK
#   undef VK_USE_PLATFORM_MACOS_MVK
#   define VK_USE_PLATFORM_MACOS_MVK true
#else
#   define VK_USE_PLATFORM_MACOS_MVK false
#endif
#ifdef VK_USE_PLATFORM_METAL_EXT
#   undef VK_USE_PLATFORM_METAL_EXT
#   define VK_USE_PLATFORM_METAL_EXT true
#else
#   define VK_USE_PLATFORM_METAL_EXT false
#endif
#ifdef VK_USE_PLATFORM_FUCHSIA
#   undef VK_USE_PLATFORM_FUCHSIA
#   define VK_USE_PLATFORM_FUCHSIA true
#else
#   define VK_USE_PLATFORM_FUCHSIA false
#endif
#ifdef VK_USE_PLATFORM_GGP
#   undef VK_USE_PLATFORM_GGP
#   define VK_USE_PLATFORM_GGP true
#else
#   define VK_USE_PLATFORM_GGP false
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
#   undef VK_ENABLE_BETA_EXTENSIONS
#   define VK_ENABLE_BETA_EXTENSIONS true
#else
#   define VK_ENABLE_BETA_EXTENSIONS false
#endif

/* And ANDROID too */
#ifdef ANDROID
#   undef ANDROID
#   define ANDROID true
#else
#   define ANDROID false
#   define ANDROID_API_LEVEL 0
#endif

#define ANV_HAS_SURFACE (VK_USE_PLATFORM_WAYLAND_KHR ||                                        VK_USE_PLATFORM_XCB_KHR ||                                        VK_USE_PLATFORM_XLIB_KHR ||                                        VK_USE_PLATFORM_DISPLAY_KHR)

static const uint32_t MAX_API_VERSION = VK_MAKE_VERSION(1, 2, 145);

VkResult anv_EnumerateInstanceVersion(
    uint32_t*                                   pApiVersion)
{
    *pApiVersion = MAX_API_VERSION;
    return VK_SUCCESS;
}

const VkExtensionProperties anv_instance_extensions[ANV_INSTANCE_EXTENSION_COUNT] = {
   {"VK_KHR_device_group_creation", 1},
   {"VK_KHR_display", 23},
   {"VK_KHR_external_fence_capabilities", 1},
   {"VK_KHR_external_memory_capabilities", 1},
   {"VK_KHR_external_semaphore_capabilities", 1},
   {"VK_KHR_get_display_properties2", 1},
   {"VK_KHR_get_physical_device_properties2", 1},
   {"VK_KHR_get_surface_capabilities2", 1},
   {"VK_KHR_surface", 25},
   {"VK_KHR_surface_protected_capabilities", 1},
   {"VK_KHR_wayland_surface", 6},
   {"VK_KHR_xcb_surface", 6},
   {"VK_KHR_xlib_surface", 6},
   {"VK_EXT_acquire_xlib_display", 1},
   {"VK_EXT_debug_report", 8},
   {"VK_EXT_direct_mode_display", 1},
   {"VK_EXT_display_surface_counter", 1},
};

const struct anv_instance_extension_table anv_instance_extensions_supported = {
   .KHR_device_group_creation = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
   .KHR_display = (!ANDROID || ANDROID_API_LEVEL >= 26) && (VK_USE_PLATFORM_DISPLAY_KHR),
   .KHR_external_fence_capabilities = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
   .KHR_external_memory_capabilities = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
   .KHR_external_semaphore_capabilities = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
   .KHR_get_display_properties2 = (!ANDROID || ANDROID_API_LEVEL >= 29) && (VK_USE_PLATFORM_DISPLAY_KHR),
   .KHR_get_physical_device_properties2 = (!ANDROID || ANDROID_API_LEVEL >= 26) && (true),
   .KHR_get_surface_capabilities2 = (!ANDROID || ANDROID_API_LEVEL >= 26) && (ANV_HAS_SURFACE),
   .KHR_surface = (!ANDROID || ANDROID_API_LEVEL >= 26) && (ANV_HAS_SURFACE),
   .KHR_surface_protected_capabilities = (!ANDROID || ANDROID_API_LEVEL >= 29) && (ANV_HAS_SURFACE),
   .KHR_wayland_surface = (!ANDROID || ANDROID_API_LEVEL >= 26) && (VK_USE_PLATFORM_WAYLAND_KHR),
   .KHR_xcb_surface = (!ANDROID || ANDROID_API_LEVEL >= 26) && (VK_USE_PLATFORM_XCB_KHR),
   .KHR_xlib_surface = (!ANDROID || ANDROID_API_LEVEL >= 26) && (VK_USE_PLATFORM_XLIB_KHR),
   .EXT_acquire_xlib_display = VK_USE_PLATFORM_XLIB_XRANDR_EXT,
   .EXT_debug_report = true,
   .EXT_direct_mode_display = VK_USE_PLATFORM_DISPLAY_KHR,
   .EXT_display_surface_counter = VK_USE_PLATFORM_DISPLAY_KHR,
};

uint32_t
anv_physical_device_api_version(struct anv_physical_device *device)
{
    uint32_t version = 0;

    uint32_t override = vk_get_version_override();
    if (override)
        return MIN2(override, MAX_API_VERSION);

    if (!(true))
        return version;
    version = VK_MAKE_VERSION(1, 0, 145);

    if (!(true))
        return version;
    version = VK_MAKE_VERSION(1, 1, 145);

    if (!(!ANDROID))
        return version;
    version = VK_MAKE_VERSION(1, 2, 145);

    return version;
}

const VkExtensionProperties anv_device_extensions[ANV_DEVICE_EXTENSION_COUNT] = {
   {"VK_KHR_8bit_storage", 1},
   {"VK_KHR_16bit_storage", 1},
   {"VK_KHR_bind_memory2", 1},
   {"VK_KHR_buffer_device_address", 1},
   {"VK_KHR_copy_commands2", 1},
   {"VK_KHR_create_renderpass2", 1},
   {"VK_KHR_dedicated_allocation", 3},
   {"VK_KHR_depth_stencil_resolve", 1},
   {"VK_KHR_descriptor_update_template", 1},
   {"VK_KHR_device_group", 4},
   {"VK_KHR_draw_indirect_count", 1},
   {"VK_KHR_driver_properties", 1},
   {"VK_KHR_external_fence", 1},
   {"VK_KHR_external_fence_fd", 1},
   {"VK_KHR_external_memory", 1},
   {"VK_KHR_external_memory_fd", 1},
   {"VK_KHR_external_semaphore", 1},
   {"VK_KHR_external_semaphore_fd", 1},
   {"VK_KHR_get_memory_requirements2", 1},
   {"VK_KHR_image_format_list", 1},
   {"VK_KHR_imageless_framebuffer", 1},
   {"VK_KHR_incremental_present", 1},
   {"VK_KHR_maintenance1", 2},
   {"VK_KHR_maintenance2", 1},
   {"VK_KHR_maintenance3", 1},
   {"VK_KHR_multiview", 1},
   {"VK_KHR_performance_query", 1},
   {"VK_KHR_pipeline_executable_properties", 1},
   {"VK_KHR_push_descriptor", 2},
   {"VK_KHR_relaxed_block_layout", 1},
   {"VK_KHR_sampler_mirror_clamp_to_edge", 3},
   {"VK_KHR_sampler_ycbcr_conversion", 14},
   {"VK_KHR_separate_depth_stencil_layouts", 1},
   {"VK_KHR_shader_atomic_int64", 1},
   {"VK_KHR_shader_clock", 1},
   {"VK_KHR_shader_draw_parameters", 1},
   {"VK_KHR_shader_float16_int8", 1},
   {"VK_KHR_shader_float_controls", 4},
   {"VK_KHR_shader_non_semantic_info", 1},
   {"VK_KHR_shader_subgroup_extended_types", 1},
   {"VK_KHR_shader_terminate_invocation", 1},
   {"VK_KHR_spirv_1_4", 1},
   {"VK_KHR_storage_buffer_storage_class", 1},
   {"VK_KHR_swapchain", 70},
   {"VK_KHR_swapchain_mutable_format", 1},
   {"VK_KHR_timeline_semaphore", 2},
   {"VK_KHR_uniform_buffer_standard_layout", 1},
   {"VK_KHR_variable_pointers", 1},
   {"VK_KHR_vulkan_memory_model", 3},
   {"VK_EXT_4444_formats", 1},
   {"VK_EXT_buffer_device_address", 2},
   {"VK_EXT_calibrated_timestamps", 1},
   {"VK_EXT_conditional_rendering", 2},
   {"VK_EXT_custom_border_color", 12},
   {"VK_EXT_depth_clip_enable", 1},
   {"VK_EXT_descriptor_indexing", 2},
   {"VK_EXT_display_control", 1},
   {"VK_EXT_extended_dynamic_state", 1},
   {"VK_EXT_external_memory_dma_buf", 1},
   {"VK_EXT_external_memory_host", 1},
   {"VK_EXT_fragment_shader_interlock", 1},
   {"VK_EXT_global_priority", 2},
   {"VK_EXT_host_query_reset", 1},
   {"VK_EXT_image_drm_format_modifier", 1},
   {"VK_EXT_image_robustness", 1},
   {"VK_EXT_index_type_uint8", 1},
   {"VK_EXT_inline_uniform_block", 1},
   {"VK_EXT_line_rasterization", 1},
   {"VK_EXT_memory_budget", 1},
   {"VK_EXT_pci_bus_info", 2},
   {"VK_EXT_pipeline_creation_cache_control", 3},
   {"VK_EXT_pipeline_creation_feedback", 1},
   {"VK_EXT_post_depth_coverage", 1},
   {"VK_EXT_private_data", 1},
   {"VK_EXT_queue_family_foreign", 1},
   {"VK_EXT_robustness2", 1},
   {"VK_EXT_sampler_filter_minmax", 2},
   {"VK_EXT_scalar_block_layout", 1},
   {"VK_EXT_separate_stencil_usage", 1},
   {"VK_EXT_shader_atomic_float", 1},
   {"VK_EXT_shader_demote_to_helper_invocation", 1},
   {"VK_EXT_shader_stencil_export", 1},
   {"VK_EXT_shader_subgroup_ballot", 1},
   {"VK_EXT_shader_subgroup_vote", 1},
   {"VK_EXT_shader_viewport_index_layer", 1},
   {"VK_EXT_subgroup_size_control", 2},
   {"VK_EXT_texel_buffer_alignment", 1},
   {"VK_EXT_transform_feedback", 1},
   {"VK_EXT_vertex_attribute_divisor", 3},
   {"VK_EXT_ycbcr_image_arrays", 1},
   {"VK_ANDROID_external_memory_android_hardware_buffer", 3},
   {"VK_ANDROID_native_buffer", 7},
   {"VK_GOOGLE_decorate_string", 1},
   {"VK_GOOGLE_hlsl_functionality1", 1},
   {"VK_GOOGLE_user_type", 1},
   {"VK_INTEL_performance_query", 2},
   {"VK_INTEL_shader_integer_functions2", 1},
   {"VK_NV_compute_shader_derivatives", 1},
};

void
anv_physical_device_get_supported_extensions(const struct anv_physical_device *device,
                                                   struct anv_device_extension_table *extensions)
{
   *extensions = (struct anv_device_extension_table) {
      .KHR_8bit_storage = (!ANDROID || ANDROID_API_LEVEL >= 29) && (device->info.gen >= 8),
      .KHR_16bit_storage = (!ANDROID || ANDROID_API_LEVEL >= 28) && (device->info.gen >= 8),
      .KHR_bind_memory2 = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_buffer_device_address = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (device->has_a64_buffer_access),
      .KHR_copy_commands2 = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (true),
      .KHR_create_renderpass2 = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_dedicated_allocation = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_depth_stencil_resolve = (!ANDROID || ANDROID_API_LEVEL >= 29) && (true),
      .KHR_descriptor_update_template = (!ANDROID || ANDROID_API_LEVEL >= 26) && (true),
      .KHR_device_group = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_draw_indirect_count = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_driver_properties = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_external_fence = (!ANDROID || ANDROID_API_LEVEL >= 28) && (device->has_syncobj_wait),
      .KHR_external_fence_fd = (!ANDROID || ANDROID_API_LEVEL >= 28) && (device->has_syncobj_wait),
      .KHR_external_memory = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_external_memory_fd = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_external_semaphore = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_external_semaphore_fd = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_get_memory_requirements2 = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_image_format_list = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_imageless_framebuffer = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (true),
      .KHR_incremental_present = (!ANDROID || ANDROID_API_LEVEL >= 26) && (ANV_HAS_SURFACE),
      .KHR_maintenance1 = (!ANDROID || ANDROID_API_LEVEL >= 26) && (true),
      .KHR_maintenance2 = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_maintenance3 = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_multiview = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_performance_query = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (device->use_softpin && device->perf && device->perf->i915_perf_version >= 3 && device->use_call_secondary),
      .KHR_pipeline_executable_properties = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (true),
      .KHR_push_descriptor = (!ANDROID || ANDROID_API_LEVEL >= 26) && (true),
      .KHR_relaxed_block_layout = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_sampler_mirror_clamp_to_edge = (!ANDROID || ANDROID_API_LEVEL >= 26) && (true),
      .KHR_sampler_ycbcr_conversion = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_separate_depth_stencil_layouts = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (true),
      .KHR_shader_atomic_int64 = (!ANDROID || ANDROID_API_LEVEL >= 29) && (device->info.gen >= 9 && device->use_softpin),
      .KHR_shader_clock = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (true),
      .KHR_shader_draw_parameters = (!ANDROID || ANDROID_API_LEVEL >= 26) && (true),
      .KHR_shader_float16_int8 = (!ANDROID || ANDROID_API_LEVEL >= 29) && (device->info.gen >= 8),
      .KHR_shader_float_controls = (!ANDROID || ANDROID_API_LEVEL >= 29) && (device->info.gen >= 8),
      .KHR_shader_non_semantic_info = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (true),
      .KHR_shader_subgroup_extended_types = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (device->info.gen >= 8),
      .KHR_shader_terminate_invocation = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (true),
      .KHR_spirv_1_4 = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (true),
      .KHR_storage_buffer_storage_class = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_swapchain = (!ANDROID || ANDROID_API_LEVEL >= 26) && (ANV_HAS_SURFACE),
      .KHR_swapchain_mutable_format = (!ANDROID || ANDROID_API_LEVEL >= 29) && (ANV_HAS_SURFACE),
      .KHR_timeline_semaphore = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (true),
      .KHR_uniform_buffer_standard_layout = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (true),
      .KHR_variable_pointers = (!ANDROID || ANDROID_API_LEVEL >= 28) && (true),
      .KHR_vulkan_memory_model = (!ANDROID || ANDROID_API_LEVEL >= 29) && (true),
      .EXT_4444_formats = true,
      .EXT_buffer_device_address = device->has_a64_buffer_access,
      .EXT_calibrated_timestamps = device->has_reg_timestamp,
      .EXT_conditional_rendering = device->info.gen >= 8 || device->info.is_haswell,
      .EXT_custom_border_color = device->info.gen >= 8,
      .EXT_depth_clip_enable = true,
      .EXT_descriptor_indexing = device->has_a64_buffer_access && device->has_bindless_images,
      .EXT_display_control = VK_USE_PLATFORM_DISPLAY_KHR,
      .EXT_extended_dynamic_state = true,
      .EXT_external_memory_dma_buf = true,
      .EXT_external_memory_host = true,
      .EXT_fragment_shader_interlock = device->info.gen >= 9,
      .EXT_global_priority = device->has_context_priority,
      .EXT_host_query_reset = true,
      .EXT_image_drm_format_modifier = false,
      .EXT_image_robustness = true,
      .EXT_index_type_uint8 = true,
      .EXT_inline_uniform_block = true,
      .EXT_line_rasterization = true,
      .EXT_memory_budget = device->has_mem_available,
      .EXT_pci_bus_info = true,
      .EXT_pipeline_creation_cache_control = true,
      .EXT_pipeline_creation_feedback = true,
      .EXT_post_depth_coverage = device->info.gen >= 9,
      .EXT_private_data = true,
      .EXT_queue_family_foreign = ANDROID,
      .EXT_robustness2 = true,
      .EXT_sampler_filter_minmax = device->info.gen >= 9,
      .EXT_scalar_block_layout = true,
      .EXT_separate_stencil_usage = true,
      .EXT_shader_atomic_float = true,
      .EXT_shader_demote_to_helper_invocation = true,
      .EXT_shader_stencil_export = device->info.gen >= 9,
      .EXT_shader_subgroup_ballot = true,
      .EXT_shader_subgroup_vote = true,
      .EXT_shader_viewport_index_layer = true,
      .EXT_subgroup_size_control = true,
      .EXT_texel_buffer_alignment = true,
      .EXT_transform_feedback = true,
      .EXT_vertex_attribute_divisor = true,
      .EXT_ycbcr_image_arrays = true,
      .ANDROID_external_memory_android_hardware_buffer = (!ANDROID || ANDROID_API_LEVEL >= 28) && (ANDROID),
      .ANDROID_native_buffer = (!ANDROID || ANDROID_API_LEVEL >= 26) && (ANDROID),
      .GOOGLE_decorate_string = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (true),
      .GOOGLE_hlsl_functionality1 = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (true),
      .GOOGLE_user_type = (!ANDROID || ANDROID_API_LEVEL >= 9999) && (true),
      .INTEL_performance_query = device->perf && device->perf->i915_perf_version >= 3,
      .INTEL_shader_integer_functions2 = device->info.gen >= 8,
      .NV_compute_shader_derivatives = true,
   };
}
