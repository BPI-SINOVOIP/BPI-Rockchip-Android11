#
# Copyright (C) 2016-2020 ARM Limited.
#
# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

# HAL module implemenation, not prelinked.
# Binary located in hw/gralloc.<ro.product.board>.so
include $(CLEAR_VARS)
ifndef PLATFORM_SDK_VERSION
include $(BUILD_SYSTEM)/version_defaults.mk
endif

# Include makefile which exports Gralloc Major and Minor version numbers
include $(LOCAL_PATH)/../gralloc.version.mk

# Include platform specific makefiles
include $(if $(wildcard $(LOCAL_PATH)/Android.$(TARGET_BOARD_PLATFORM).mk), $(LOCAL_PATH)/Android.$(TARGET_BOARD_PLATFORM).mk,)

# Disallow building for unsupported Android versions
ifeq ($(shell expr $(PLATFORM_SDK_VERSION) \> 27), 0)
    $(error Gralloc supports only Android P and later versions.)
endif

PLATFORM_SDK_GREATER_THAN_28 := $(shell expr $(PLATFORM_SDK_VERSION) \> 28)

MALI_GRALLOC_GPU_LIBRARY_64_PATH1 := "\"/vendor/lib64/egl/\""
MALI_GRALLOC_GPU_LIBRARY_64_PATH2 := "\"/system/lib64/egl/\""
MALI_GRALLOC_DPU_LIBRARY_64_PATH := "\"/vendor/lib64/hw/\""
MALI_GRALLOC_DPU_AEU_LIBRARY_64_PATH := "\"/vendor/lib64/hw/\""
MALI_GRALLOC_GPU_LIBRARY_32_PATH1 := "\"/vendor/lib/egl/\""
MALI_GRALLOC_GPU_LIBRARY_32_PATH2 := "\"/system/lib/egl/\""
MALI_GRALLOC_DPU_LIBRARY_32_PATH := "\"/vendor/lib/hw/\""
MALI_GRALLOC_DPU_AEU_LIBRARY_32_PATH := "\"/vendor/lib/hw/\""

# 2.0+ specific.
# If set to the default value of zero then build the allocator. Otherwise build the mapper.
# The top-level make file builds both.
GRALLOC_MAPPER?=0

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
GRALLOC_INIT_AFBC?=0
# fbdev bitdepth to use
GRALLOC_FB_BPP?=32
# When enabled, forces display framebuffer format to BGRA_8888
GRALLOC_FB_SWAP_RED_BLUE?=1
# When enabled, forces format to BGRA_8888 for FB usage when HWC is in use
GRALLOC_HWC_FORCE_BGRA_8888?=0
# When enabled, disables AFBC for FB usage when HWC is in use
GRALLOC_HWC_FB_DISABLE_AFBC?=0
# Disables the framebuffer HAL device. When a hwc impl is available.
ifeq ($(PLATFORM_SDK_GREATER_THAN_28), 1)
    GRALLOC_DISABLE_FRAMEBUFFER_HAL?=1
    ifneq ($(GRALLOC_DISABLE_FRAMEBUFFER_HAL), 1)
        $(error Framebuffer HAL unsupported for Android 10 and above)
    endif
else
    GRALLOC_DISABLE_FRAMEBUFFER_HAL?=0
endif
# When enabled, buffers will never be allocated with AFBC
GRALLOC_ARM_NO_EXTERNAL_AFBC?=0
# Minimum buffer dimensions in pixels when buffer will use AFBC
GRALLOC_DISP_W?=0
GRALLOC_DISP_H?=0
# Vsync backend(not used)
GRALLOC_VSYNC_BACKEND?=default

ifeq ($(PLATFORM_SDK_GREATER_THAN_28), 1)
    GRALLOC_USE_LEGACY_CALCS_LOCK?=0
    ifeq ($(GRALLOC_USE_LEGACY_CALCS_LOCK), 1)
        $(error Legacy calculations are unsupported for Android 10 and above)
    endif
else
    GRALLOC_USE_LEGACY_CALCS_LOCK?=1
endif

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

GRALLOC_SRC_PATH := $(LOCAL_PATH)

LOCAL_C_INCLUDES := $(MALI_LOCAL_PATH) $(MALI_DDK_INCLUDES)

# General compilation flags
GRALLOC_SHARED_CFLAGS := -ldl -Werror -DLOG_TAG="\"gralloc$(GRALLOC_VERSION_MAJOR)\"" \
                         -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

#Shared flags
GRALLOC_SHARED_CFLAGS += -DMALI_DISPLAY_VERSION=$(MALI_DISPLAY_VERSION)
GRALLOC_SHARED_CFLAGS += -DGRALLOC_VERSION_MAJOR=$(GRALLOC_VERSION_MAJOR)
GRALLOC_SHARED_CFLAGS += -DHIDL_ALLOCATOR_VERSION_SCALED=$(HIDL_ALLOCATOR_VERSION_SCALED)
GRALLOC_SHARED_CFLAGS += -DHIDL_MAPPER_VERSION_SCALED=$(HIDL_MAPPER_VERSION_SCALED)
GRALLOC_SHARED_CFLAGS += -DHIDL_COMMON_VERSION_SCALED=$(HIDL_COMMON_VERSION_SCALED)
GRALLOC_SHARED_CFLAGS += -DDISABLE_FRAMEBUFFER_HAL=$(GRALLOC_DISABLE_FRAMEBUFFER_HAL)
GRALLOC_SHARED_CFLAGS += -DGRALLOC_FB_BPP=$(GRALLOC_FB_BPP)
GRALLOC_SHARED_CFLAGS += -DGRALLOC_FB_SWAP_RED_BLUE=$(GRALLOC_FB_SWAP_RED_BLUE)
GRALLOC_SHARED_CFLAGS += -DGRALLOC_HWC_FORCE_BGRA_8888=$(GRALLOC_HWC_FORCE_BGRA_8888)
GRALLOC_SHARED_CFLAGS += -DGRALLOC_HWC_FB_DISABLE_AFBC=$(GRALLOC_HWC_FB_DISABLE_AFBC)
GRALLOC_SHARED_CFLAGS += -DGRALLOC_LIBRARY_BUILD=1
GRALLOC_SHARED_CFLAGS += -DGRALLOC_USE_LEGACY_CALCS=$(GRALLOC_USE_LEGACY_CALCS_LOCK)
GRALLOC_SHARED_CFLAGS += -DGRALLOC_USE_LEGACY_LOCK=$(GRALLOC_USE_LEGACY_CALCS_LOCK)
GRALLOC_SHARED_CFLAGS += -DGRALLOC_CAMERA_WRITE_RAW16=$(GRALLOC_CAMERA_WRITE_RAW16)

LOCAL_CFLAGS := $(GRALLOC_SHARED_CFLAGS)

LOCAL_SHARED_LIBRARIES := libhardware liblog libcutils libion libsync libutils

ifeq ($(GRALLOC_VERSION_MAJOR), 1)
    LOCAL_SHARED_LIBRARIES += libGLESv1_CM
else ifeq ($(GRALLOC_VERSION_MAJOR), 2)
    LOCAL_SHARED_LIBRARIES += libhidlbase libhidltransport
    ifeq ($(GRALLOC_MAPPER), 1)
        LOCAL_SHARED_LIBRARIES += android.hardware.graphics.mapper@2.0
        ifeq ($(HIDL_MAPPER_VERSION_SCALED), 210)
            LOCAL_SHARED_LIBRARIES += android.hardware.graphics.mapper@2.1
        endif
    else
        LOCAL_SHARED_LIBRARIES += android.hardware.graphics.allocator@2.0
        ifeq ($(HIDL_MAPPER_VERSION_SCALED), 210)
            LOCAL_SHARED_LIBRARIES += android.hardware.graphics.mapper@2.1
        endif
    endif
else ifeq ($(GRALLOC_VERSION_MAJOR), 3)
    LOCAL_SHARED_LIBRARIES += libhidlbase libhidltransport
    ifeq ($(GRALLOC_MAPPER), 1)
        LOCAL_SHARED_LIBRARIES += android.hardware.graphics.mapper@3.0
    else
        LOCAL_SHARED_LIBRARIES += android.hardware.graphics.allocator@3.0
    endif
else ifeq ($(GRALLOC_VERSION_MAJOR), 4)
    LOCAL_SHARED_LIBRARIES += libhidlbase libgralloctypes libdrm
    ifeq ($(GRALLOC_MAPPER), 1)
        LOCAL_SHARED_LIBRARIES += android.hardware.graphics.mapper@4.0
    else
        LOCAL_SHARED_LIBRARIES += android.hardware.graphics.allocator@4.0
    endif
endif

LOCAL_SHARED_LIBRARIES += libnativewindow
LOCAL_STATIC_LIBRARIES := libarect
LOCAL_HEADER_LIBRARIES := libnativebase_headers

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_PATH_32 := $(TARGET_OUT_VENDOR)/lib
LOCAL_MODULE_PATH_64 := $(TARGET_OUT_VENDOR)/lib64
ifeq ($(GRALLOC_VERSION_MAJOR), 1)
    ifeq ($(TARGET_BOARD_PLATFORM),)
        LOCAL_MODULE := gralloc.default
    else
        LOCAL_MODULE := gralloc.$(TARGET_BOARD_PLATFORM)
    endif
else ifeq ($(GRALLOC_VERSION_MAJOR), 2)
    ifeq ($(GRALLOC_MAPPER), 1)
        ifeq ($(HIDL_MAPPER_VERSION_SCALED), 200)
            LOCAL_MODULE := android.hardware.graphics.mapper@2.0-impl-arm
        else ifeq ($(HIDL_MAPPER_VERSION_SCALED), 210)
            LOCAL_MODULE := android.hardware.graphics.mapper@2.0-impl-2.1-arm
        endif
    else
        LOCAL_MODULE := android.hardware.graphics.allocator@2.0-impl-arm
    endif
else ifeq ($(GRALLOC_VERSION_MAJOR), 3)
    ifeq ($(GRALLOC_MAPPER), 1)
        LOCAL_MODULE := android.hardware.graphics.mapper@3.0-impl-arm
    else
        LOCAL_MODULE := android.hardware.graphics.allocator@3.0-impl-arm
    endif
else ifeq ($(GRALLOC_VERSION_MAJOR), 4)
    ifeq ($(GRALLOC_MAPPER), 1)
        LOCAL_MODULE := android.hardware.graphics.mapper@4.0-impl-arm
    else
        LOCAL_MODULE := android.hardware.graphics.allocator@4.0-impl-arm
    endif
endif

LOCAL_MODULE_OWNER := arm
LOCAL_PROPRIETARY_MODULE := true

LOCAL_MODULE_TAGS := optional
LOCAL_MULTILIB := both

# In some build configurations there are circular dependencies
LOCAL_GROUP_STATIC_LIBRARIES := true
LOCAL_STATIC_LIBRARIES += libgralloc_core libgralloc_allocator libgralloc_capabilities

ifeq ($(GRALLOC_VERSION_MAJOR), 1)
    LOCAL_SRC_FILES += 1.x/mali_gralloc_module.cpp \
                       fbdev/mali_gralloc_framebuffer.cpp \
                       1.x/framebuffer_device.cpp \
                       1.x/gralloc_vsync_${GRALLOC_VSYNC_BACKEND}.cpp \
                       1.x/mali_gralloc_public_interface.cpp
else ifeq ($(GRALLOC_VERSION_MAJOR), 2)
    ifeq ($(GRALLOC_MAPPER), 1)
        LOCAL_SRC_FILES += 2.x/GrallocMapper.cpp \
                           hidl_common/Mapper.cpp \
                           hidl_common/RegisteredHandlePool.cpp
    else
        LOCAL_SRC_FILES += 2.x/GrallocAllocator.cpp \
                           fbdev/mali_gralloc_framebuffer.cpp \
                           hidl_common/Allocator.cpp
    endif

    LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := \
        android.hardware.graphics.allocator@2.0 \
        libhidlbase \
        libhidltransport

    ifeq ($(HIDL_MAPPER_VERSION_SCALED), 200)
        LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += android.hardware.graphics.mapper@2.0
        LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += android.hardware.graphics.common@1.0
    else ifeq ($(HIDL_MAPPER_VERSION_SCALED), 210)
        LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += android.hardware.graphics.mapper@2.1
        LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += android.hardware.graphics.common@1.1
    endif

    ifeq ($(HIDL_COMMON_VERSION_SCALED), 100)
        LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += android.hardware.graphics.common@1.0
    else ifeq ($(HIDL_COMMON_VERSION_SCALED), 110)
        LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += android.hardware.graphics.common@1.1
    endif
else ifeq ($(GRALLOC_VERSION_MAJOR), 3)
    LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := \
        libhidlbase \
        libhidltransport

    ifeq ($(GRALLOC_MAPPER), 1)
        LOCAL_SRC_FILES += 3.x/GrallocMapper.cpp \
                           hidl_common/Mapper.cpp \
                           hidl_common/RegisteredHandlePool.cpp
    else
        LOCAL_SRC_FILES += 3.x/GrallocAllocator.cpp \
                           hidl_common/Allocator.cpp
        LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += android.hardware.graphics.allocator@3.0
        LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += android.hardware.graphics.mapper@3.0
        LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += android.hardware.graphics.common@1.2
    endif
else ifeq ($(GRALLOC_VERSION_MAJOR), 4)
    LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := \
        libhidlbase
    ifeq ($(GRALLOC_MAPPER), 1)
        LOCAL_SRC_FILES += 4.x/GrallocMapper.cpp \
                           hidl_common/Mapper.cpp \
                           hidl_common/RegisteredHandlePool.cpp \
                           hidl_common/MapperMetadata.cpp
        LOCAL_STATIC_LIBRARIES += libgralloc_drmutils
        LOCAL_SHARED_LIBRARIES += arm.graphics-ndk_platform
    else
        LOCAL_SRC_FILES += 4.x/GrallocAllocator.cpp \
                           hidl_common/Allocator.cpp
        LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += android.hardware.graphics.allocator@4.0
        LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += android.hardware.graphics.mapper@4.0
        LOCAL_EXPORT_SHARED_LIBRARY_HEADERS += android.hardware.graphics.common@1.2
    endif
endif

LOCAL_MODULE_OWNER := arm

include $(BUILD_SHARED_LIBRARY)

ifeq ($(GRALLOC_MAPPER), 0)
include $(GRALLOC_SRC_PATH)/allocator/Android.mk
include $(GRALLOC_SRC_PATH)/core/Android.mk
include $(GRALLOC_SRC_PATH)/capabilities/Android.mk
endif
