#
# Copyright (C) 2020 ARM Limited. All rights reserved.
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
include $(CLEAR_VARS)

LOCAL_MODULE := libgralloc_allocator
LOCAL_MODULE_OWNER := arm
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_MULTILIB := both

LOCAL_C_INCLUDES := $(GRALLOC_SRC_PATH)

LOCAL_SRC_FILES := mali_gralloc_ion.cpp mali_gralloc_shared_memory.cpp

LOCAL_SHARED_LIBRARIES := libhardware liblog libcutils libion libsync libutils

# General compilation flags
LOCAL_CFLAGS := $(GRALLOC_SHARED_CFLAGS)

LOCAL_CFLAGS += -DGRALLOC_USE_ION_DMA_HEAP=$(GRALLOC_USE_ION_DMA_HEAP)
LOCAL_CFLAGS += -DGRALLOC_USE_ION_COMPOUND_PAGE_HEAP=$(GRALLOC_USE_ION_COMPOUND_PAGE_HEAP)
LOCAL_CFLAGS += -DGRALLOC_INIT_AFBC=$(GRALLOC_INIT_AFBC)
LOCAL_CFLAGS += -DGRALLOC_USE_ION_DMABUF_SYNC=$(GRALLOC_USE_ION_DMABUF_SYNC)

LOCAL_SHARED_LIBRARIES += libnativewindow
LOCAL_HEADER_LIBRARIES := libnativebase_headers
LOCAL_STATIC_LIBRARIES := libarect

ifeq ($(HIDL_COMMON_VERSION_SCALED), 100)
    LOCAL_SHARED_LIBRARIES += android.hardware.graphics.common@1.0
else ifeq ($(HIDL_COMMON_VERSION_SCALED), 110)
    LOCAL_SHARED_LIBRARIES += android.hardware.graphics.common@1.1
else ifeq ($(HIDL_COMMON_VERSION_SCALED), 120)
    LOCAL_SHARED_LIBRARIES += android.hardware.graphics.common@1.2
endif

include $(BUILD_STATIC_LIBRARY)
