COPYRIGHT = """\
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
"""

import os.path
import sys

VULKAN_UTIL = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../vulkan/util'))
sys.path.append(VULKAN_UTIL)

from vk_extensions import *

API_PATCH_VERSION = 145

# Supported API versions.  Each one is the maximum patch version for the given
# version.  Version come in increasing order and each version is available if
# it's provided "enable" condition is true and all previous versions are
# available.
API_VERSIONS = [
    ApiVersion('1.0',   True),
    ApiVersion('1.1',   True),
    ApiVersion('1.2',   '!ANDROID'),
]

MAX_API_VERSION = None # Computed later

# On Android, we disable all surface and swapchain extensions. Android's Vulkan
# loader implements VK_KHR_surface and VK_KHR_swapchain, and applications
# cannot access the driver's implementation. Moreoever, if the driver exposes
# the those extension strings, then tests dEQP-VK.api.info.instance.extensions
# and dEQP-VK.api.info.device fail due to the duplicated strings.
EXTENSIONS = [
    Extension('VK_KHR_8bit_storage',                      1, 'device->info.gen >= 8'),
    Extension('VK_KHR_16bit_storage',                     1, 'device->info.gen >= 8'),
    Extension('VK_KHR_bind_memory2',                      1, True),
    Extension('VK_KHR_buffer_device_address',             1, 'device->has_a64_buffer_access'),
    Extension('VK_KHR_copy_commands2',                    1, True),
    Extension('VK_KHR_create_renderpass2',                1, True),
    Extension('VK_KHR_dedicated_allocation',              3, True),
    Extension('VK_KHR_depth_stencil_resolve',             1, True),
    Extension('VK_KHR_descriptor_update_template',        1, True),
    Extension('VK_KHR_device_group',                      4, True),
    Extension('VK_KHR_device_group_creation',             1, True),
    Extension('VK_KHR_display',                          23, 'VK_USE_PLATFORM_DISPLAY_KHR'),
    Extension('VK_KHR_draw_indirect_count',               1, True),
    Extension('VK_KHR_driver_properties',                 1, True),
    Extension('VK_KHR_external_fence',                    1,
              'device->has_syncobj_wait'),
    Extension('VK_KHR_external_fence_capabilities',       1, True),
    Extension('VK_KHR_external_fence_fd',                 1,
              'device->has_syncobj_wait'),
    Extension('VK_KHR_external_memory',                   1, True),
    Extension('VK_KHR_external_memory_capabilities',      1, True),
    Extension('VK_KHR_external_memory_fd',                1, True),
    Extension('VK_KHR_external_semaphore',                1, True),
    Extension('VK_KHR_external_semaphore_capabilities',   1, True),
    Extension('VK_KHR_external_semaphore_fd',             1, True),
    Extension('VK_KHR_get_display_properties2',           1, 'VK_USE_PLATFORM_DISPLAY_KHR'),
    Extension('VK_KHR_get_memory_requirements2',          1, True),
    Extension('VK_KHR_get_physical_device_properties2',   1, True),
    Extension('VK_KHR_get_surface_capabilities2',         1, 'ANV_HAS_SURFACE'),
    Extension('VK_KHR_image_format_list',                 1, True),
    Extension('VK_KHR_imageless_framebuffer',             1, True),
    Extension('VK_KHR_incremental_present',               1, 'ANV_HAS_SURFACE'),
    Extension('VK_KHR_maintenance1',                      2, True),
    Extension('VK_KHR_maintenance2',                      1, True),
    Extension('VK_KHR_maintenance3',                      1, True),
    Extension('VK_KHR_multiview',                         1, True),
    Extension('VK_KHR_performance_query',                 1,
              'device->use_softpin && device->perf && ' +
              'device->perf->i915_perf_version >= 3 && ' +
              'device->use_call_secondary'),
    Extension('VK_KHR_pipeline_executable_properties',    1, True),
    Extension('VK_KHR_push_descriptor',                   2, True),
    Extension('VK_KHR_relaxed_block_layout',              1, True),
    Extension('VK_KHR_sampler_mirror_clamp_to_edge',      3, True),
    Extension('VK_KHR_sampler_ycbcr_conversion',         14, True),
    Extension('VK_KHR_separate_depth_stencil_layouts',    1, True),
    Extension('VK_KHR_shader_atomic_int64',               1,
              'device->info.gen >= 9 && device->use_softpin'),
    Extension('VK_KHR_shader_clock',                      1, True),
    Extension('VK_KHR_shader_draw_parameters',            1, True),
    Extension('VK_KHR_shader_float16_int8',               1, 'device->info.gen >= 8'),
    Extension('VK_KHR_shader_float_controls',             4, 'device->info.gen >= 8'),
    Extension('VK_KHR_shader_non_semantic_info',          1, True),
    Extension('VK_KHR_shader_subgroup_extended_types',    1, 'device->info.gen >= 8'),
    Extension('VK_KHR_shader_terminate_invocation',       1, True),
    Extension('VK_KHR_spirv_1_4',                         1, True),
    Extension('VK_KHR_storage_buffer_storage_class',      1, True),
    Extension('VK_KHR_surface',                          25, 'ANV_HAS_SURFACE'),
    Extension('VK_KHR_surface_protected_capabilities',    1, 'ANV_HAS_SURFACE'),
    Extension('VK_KHR_swapchain',                        70, 'ANV_HAS_SURFACE'),
    Extension('VK_KHR_swapchain_mutable_format',          1, 'ANV_HAS_SURFACE'),
    Extension('VK_KHR_timeline_semaphore',                2, True),
    Extension('VK_KHR_uniform_buffer_standard_layout',    1, True),
    Extension('VK_KHR_variable_pointers',                 1, True),
    Extension('VK_KHR_vulkan_memory_model',               3, True),
    Extension('VK_KHR_wayland_surface',                   6, 'VK_USE_PLATFORM_WAYLAND_KHR'),
    Extension('VK_KHR_xcb_surface',                       6, 'VK_USE_PLATFORM_XCB_KHR'),
    Extension('VK_KHR_xlib_surface',                      6, 'VK_USE_PLATFORM_XLIB_KHR'),
    Extension('VK_EXT_4444_formats',                      1, True),
    Extension('VK_EXT_acquire_xlib_display',              1, 'VK_USE_PLATFORM_XLIB_XRANDR_EXT'),
    Extension('VK_EXT_buffer_device_address',             2, 'device->has_a64_buffer_access'),
    Extension('VK_EXT_calibrated_timestamps',             1, 'device->has_reg_timestamp'),
    Extension('VK_EXT_conditional_rendering',             2, 'device->info.gen >= 8 || device->info.is_haswell'),
    Extension('VK_EXT_custom_border_color',               12, 'device->info.gen >= 8'),
    Extension('VK_EXT_debug_report',                      8, True),
    Extension('VK_EXT_depth_clip_enable',                 1, True),
    Extension('VK_EXT_descriptor_indexing',               2,
              'device->has_a64_buffer_access && device->has_bindless_images'),
    Extension('VK_EXT_direct_mode_display',               1, 'VK_USE_PLATFORM_DISPLAY_KHR'),
    Extension('VK_EXT_display_control',                   1, 'VK_USE_PLATFORM_DISPLAY_KHR'),
    Extension('VK_EXT_display_surface_counter',           1, 'VK_USE_PLATFORM_DISPLAY_KHR'),
    Extension('VK_EXT_extended_dynamic_state',            1, True),
    Extension('VK_EXT_external_memory_dma_buf',           1, True),
    Extension('VK_EXT_external_memory_host',              1, True),
    Extension('VK_EXT_fragment_shader_interlock',         1, 'device->info.gen >= 9'),
    Extension('VK_EXT_global_priority',                   2,
              'device->has_context_priority'),
    Extension('VK_EXT_host_query_reset',                  1, True),
    Extension('VK_EXT_image_drm_format_modifier',         1, False),
    Extension('VK_EXT_image_robustness',                  1, True),
    Extension('VK_EXT_index_type_uint8',                  1, True),
    Extension('VK_EXT_inline_uniform_block',              1, True),
    Extension('VK_EXT_line_rasterization',                1, True),
    Extension('VK_EXT_memory_budget',                     1, 'device->has_mem_available'),
    Extension('VK_EXT_pci_bus_info',                      2, True),
    Extension('VK_EXT_pipeline_creation_cache_control',   3, True),
    Extension('VK_EXT_pipeline_creation_feedback',        1, True),
    Extension('VK_EXT_post_depth_coverage',               1, 'device->info.gen >= 9'),
    Extension('VK_EXT_private_data',                      1, True),
    Extension('VK_EXT_queue_family_foreign',              1, 'ANDROID'),
    Extension('VK_EXT_robustness2',                       1, True),
    Extension('VK_EXT_sampler_filter_minmax',             2, 'device->info.gen >= 9'),
    Extension('VK_EXT_scalar_block_layout',               1, True),
    Extension('VK_EXT_separate_stencil_usage',            1, True),
    Extension('VK_EXT_shader_atomic_float',               1, True),
    Extension('VK_EXT_shader_demote_to_helper_invocation', 1, True),
    Extension('VK_EXT_shader_stencil_export',             1, 'device->info.gen >= 9'),
    Extension('VK_EXT_shader_subgroup_ballot',            1, True),
    Extension('VK_EXT_shader_subgroup_vote',              1, True),
    Extension('VK_EXT_shader_viewport_index_layer',       1, True),
    Extension('VK_EXT_subgroup_size_control',             2, True),
    Extension('VK_EXT_texel_buffer_alignment',            1, True),
    Extension('VK_EXT_transform_feedback',                1, True),
    Extension('VK_EXT_vertex_attribute_divisor',          3, True),
    Extension('VK_EXT_ycbcr_image_arrays',                1, True),
    Extension('VK_ANDROID_external_memory_android_hardware_buffer', 3, 'ANDROID'),
    Extension('VK_ANDROID_native_buffer',                 7, 'ANDROID'),
    Extension('VK_GOOGLE_decorate_string',                1, True),
    Extension('VK_GOOGLE_hlsl_functionality1',            1, True),
    Extension('VK_GOOGLE_user_type',                      1, True),
    Extension('VK_INTEL_performance_query',               2, 'device->perf && device->perf->i915_perf_version >= 3'),
    Extension('VK_INTEL_shader_integer_functions2',       1, 'device->info.gen >= 8'),
    Extension('VK_NV_compute_shader_derivatives',         1, True),
]

# Sort the extension list the way we expect: KHR, then EXT, then vendors
# alphabetically. For digits, read them as a whole number sort that.
# eg.: VK_KHR_8bit_storage < VK_KHR_16bit_storage < VK_EXT_acquire_xlib_display
def extension_order(ext):
    order = []
    for substring in re.split('(KHR|EXT|[0-9]+)', ext.name):
        if substring == 'KHR':
            order.append(1)
        if substring == 'EXT':
            order.append(2)
        elif substring.isdigit():
            order.append(int(substring))
        else:
            order.append(substring)
    return order
for i in range(len(EXTENSIONS) - 1):
    if extension_order(EXTENSIONS[i + 1]) < extension_order(EXTENSIONS[i]):
        print(EXTENSIONS[i + 1].name + ' should come before ' + EXTENSIONS[i].name)
        exit(1)

MAX_API_VERSION = VkVersion('0.0.0')
for version in API_VERSIONS:
    version.version = VkVersion(version.version)
    version.version.patch = API_PATCH_VERSION
    assert version.version > MAX_API_VERSION
    MAX_API_VERSION = version.version
