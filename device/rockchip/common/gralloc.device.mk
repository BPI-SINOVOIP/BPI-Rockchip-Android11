# Copyright (C) 2020 Arm Limited.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Configuration that should be included by BoardConfig.mk to configure necessary Soong namespaces.

MALI_GRALLOC_API_TESTS?=0

#
# Static hardware defines
#
# These defines are used in case runtime detection does not find the
# user-space driver to read out hardware capabilities

# GPU support for AFBC 1.0
MALI_GPU_SUPPORT_AFBC_BASIC?=0
# GPU support for AFBC 1.1 block split
MALI_GPU_SUPPORT_AFBC_SPLITBLK?=0
# GPU support for AFBC 1.1 wide block
MALI_GPU_SUPPORT_AFBC_WIDEBLK?=0
# GPU support for AFBC 1.2 tiled headers
MALI_GPU_SUPPORT_AFBC_TILED_HEADERS?=0
# GPU support for writing AFBC YUV formats
MALI_GPU_SUPPORT_AFBC_YUV_WRITE?=0

# VPU version we support
MALI_VIDEO_VERSION?=0
# DPU version we support
MALI_DISPLAY_VERSION?=0

#
# Software behaviour defines
#

# The following defines are used to override default behaviour of which heap is selected for allocations.
# The default is to pick system heap.
# The following two defines enable either DMA heap or compound page heap for when the usage has
# GRALLOC_USAGE_HW_FB or GRALLOC_USAGE_HW_COMPOSER set and GRALLOC_USAGE_HW_VIDEO_ENCODER is not set.
# These defines should not be enabled at the same time.
GRALLOC_USE_ION_DMA_HEAP?=0
GRALLOC_USE_ION_COMPOUND_PAGE_HEAP?=0

# Properly initializes an empty AFBC buffer
GRALLOC_INIT_AFBC?=1
# When enabled, forces format to BGRA_8888 for FB usage when HWC is in use
GRALLOC_HWC_FORCE_BGRA_8888?=0
# When enabled, disables AFBC for FB usage when HWC is in use
GRALLOC_HWC_FB_DISABLE_AFBC?=0

# When enabled, buffers will never be allocated with AFBC
GRALLOC_ARM_NO_EXTERNAL_AFBC?=0

GRALLOC_USE_ION_DMABUF_SYNC?=1

ifeq ($(TARGET_BOARD_PLATFORM), juno)
ifeq ($(MALI_MMSS), 1)

# Use latest default MMSS build configuration if not already defined
ifeq ($(MALI_DISPLAY_VERSION), 0)
MALI_DISPLAY_VERSION = 650
endif
ifeq ($(MALI_VIDEO_VERSION), 0)
MALI_VIDEO_VERSION = 550
endif

GRALLOC_FB_SWAP_RED_BLUE = 0
GRALLOC_USE_ION_DMA_HEAP = 1
endif
endif

ifeq ($(TARGET_BOARD_PLATFORM), armboard_v7a)
ifeq ($(GRALLOC_MALI_DP),true)
    GRALLOC_FB_SWAP_RED_BLUE = 0
    GRALLOC_DISABLE_FRAMEBUFFER_HAL=1
    MALI_DISPLAY_VERSION = 550
    GRALLOC_USE_ION_DMA_HEAP=1
endif
endif

ifneq ($(MALI_DISPLAY_VERSION), 0)
# If Mali display is available, should disable framebuffer HAL
GRALLOC_DISABLE_FRAMEBUFFER_HAL := 1
# If Mali display is available, AFBC buffers should be initialised after allocation
GRALLOC_INIT_AFBC := 1
endif

# When enabled, sets camera capability bit
GRALLOC_CAMERA_WRITE_RAW16?=1

ifeq ($(GRALLOC_USE_ION_DMA_HEAP), 1)
ifeq ($(GRALLOC_USE_ION_COMPOUND_PAGE_HEAP), 1)
$(error "GRALLOC_USE_ION_DMA_HEAP and GRALLOC_USE_ION_COMPOUND_PAGE_HEAP can't be enabled at the same time")
endif
endif

MALI_VALID_DISPLAY_VERSIONS:= 0 500 550 650 71
MALI_VALID_VIDEO_VERSIONS:= 0 500 550 61

ifeq ($(filter $(MALI_DISPLAY_VERSION),$(MALI_VALID_DISPLAY_VERSIONS)),)
    $(error Display version $(MALI_DISPLAY_VERSION) is not valid. Valid versions are $(MALI_VALID_DISPLAY_VERSIONS))
endif

ifeq ($(filter $(MALI_VIDEO_VERSION),$(MALI_VALID_VIDEO_VERSIONS)),)
    $(error Video version $(MALI_VIDEO_VERSION) is not valid. Valid versions are $(MALI_VALID_VIDEO_VERSIONS))
endif

# GRALLOC_API_VERSION?=v300
GRALLOC_API_VERSION?=4.x

# Setup configuration in Soong namespace
SOONG_CONFIG_NAMESPACES += arm_gralloc
SOONG_CONFIG_arm_gralloc := \
	mali_gpu_support_afbc_basic \
	mali_gpu_support_afbc_splitblk \
	mali_gpu_support_afbc_wideblk \
	mali_gpu_support_afbc_tiled_headers \
	mali_gpu_support_afbc_yuv_write \
	mali_video_version \
	mali_display_version \
	gralloc_use_ion_dma_heap \
	gralloc_use_ion_compound_page_heap \
	gralloc_init_afbc \
	gralloc_hwc_force_bgra_8888 \
	gralloc_hwc_fb_disable_afbc \
	gralloc_arm_no_external_afbc \
	gralloc_use_ion_dmabuf_sync \
	gralloc_camera_write_raw16 \
	mali_gralloc_api_tests \
	gralloc_api_version

SOONG_CONFIG_arm_gralloc_mali_gpu_support_afbc_basic := $(MALI_GPU_SUPPORT_AFBC_BASIC)
SOONG_CONFIG_arm_gralloc_mali_gpu_support_afbc_splitblk := $(MALI_GPU_SUPPORT_AFBC_SPLITBLK)
SOONG_CONFIG_arm_gralloc_mali_gpu_support_afbc_wideblk := $(MALI_GPU_SUPPORT_AFBC_WIDEBLK)
SOONG_CONFIG_arm_gralloc_mali_gpu_support_afbc_tiled_headers := $(MALI_GPU_SUPPORT_AFBC_TILED_HEADERS)
SOONG_CONFIG_arm_gralloc_mali_gpu_support_afbc_yuv_write := $(MALI_GPU_SUPPORT_AFBC_YUV_WRITE)
SOONG_CONFIG_arm_gralloc_mali_video_version := v$(MALI_VIDEO_VERSION)
SOONG_CONFIG_arm_gralloc_mali_display_version := v$(MALI_DISPLAY_VERSION)
SOONG_CONFIG_arm_gralloc_gralloc_use_ion_dma_heap := $(GRALLOC_USE_ION_DMA_HEAP)
SOONG_CONFIG_arm_gralloc_gralloc_use_ion_compound_page_heap := $(GRALLOC_USE_ION_COMPOUND_PAGE_HEAP)
SOONG_CONFIG_arm_gralloc_gralloc_init_afbc := $(GRALLOC_INIT_AFBC)
SOONG_CONFIG_arm_gralloc_gralloc_hwc_force_bgra_8888 := $(GRALLOC_HWC_FORCE_BGRA_8888)
SOONG_CONFIG_arm_gralloc_gralloc_hwc_fb_disable_afbc := $(GRALLOC_HWC_FB_DISABLE_AFBC)
SOONG_CONFIG_arm_gralloc_gralloc_arm_no_external_afbc := $(GRALLOC_ARM_NO_EXTERNAL_AFBC)
SOONG_CONFIG_arm_gralloc_gralloc_use_ion_dmabuf_sync := $(GRALLOC_USE_ION_DMABUF_SYNC)
SOONG_CONFIG_arm_gralloc_gralloc_camera_write_raw16 := $(GRALLOC_CAMERA_WRITE_RAW16)
SOONG_CONFIG_arm_gralloc_mali_gralloc_api_tests := $(MALI_GRALLOC_API_TESTS)
SOONG_CONFIG_arm_gralloc_gralloc_api_version := $(GRALLOC_API_VERSION)
