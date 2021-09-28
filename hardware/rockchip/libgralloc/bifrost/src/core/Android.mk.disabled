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

# Common parameters
MULTIARCH_MODULE := libgralloc_core
MULTIARCH_MODULE_OWNER := arm
MULTIARCH_PROPRIETARY_MODULE := true
MULTIARCH_MODULE_TAGS := optional

MULTIARCH_C_INCLUDES := $(GRALLOC_SRC_PATH)

MULTIARCH_SRC_FILES := \
    mali_gralloc_bufferaccess.cpp \
    mali_gralloc_bufferallocation.cpp \
    mali_gralloc_formats.cpp \
    mali_gralloc_reference.cpp \
    mali_gralloc_debug.cpp \
    format_info.cpp

ifeq ($(GRALLOC_USE_LEGACY_CALCS_LOCK), 1)
MULTIARCH_SRC_FILES += \
    legacy/buffer_alloc.cpp \
    legacy/buffer_access.cpp
endif

MULTIARCH_SHARED_LIBRARIES := liblog libcutils libutils

MULTIARCH_CFLAGS := $(GRALLOC_SHARED_CFLAGS)
MULTIARCH_CFLAGS += -DGRALLOC_DISP_W=$(GRALLOC_DISP_W)
MULTIARCH_CFLAGS += -DGRALLOC_DISP_H=$(GRALLOC_DISP_H)

# Target build
LOCAL_MODULE := $(MULTIARCH_MODULE)
LOCAL_MODULE_OWNER := $(MULTIARCH_MODULE_OWNER)
LOCAL_PROPRIETARY_MODULE := $(MULTIARCH_PROPRIETARY_MODULE)
LOCAL_MODULE_TAGS := $(MULTIARCH_MODULE_TAGS)
LOCAL_SRC_FILES := $(MULTIARCH_SRC_FILES)
LOCAL_C_INCLUDES := $(MULTIARCH_C_INCLUDES)
LOCAL_SHARED_LIBRARIES := $(MULTIARCH_SHARED_LIBRARIES)
LOCAL_CFLAGS := $(MULTIARCH_CFLAGS)

# Target builds do not include libhardware headers by default
LOCAL_SHARED_LIBRARIES += libhardware

ifeq ($(HIDL_COMMON_VERSION_SCALED), 100)
    LOCAL_SHARED_LIBRARIES += android.hardware.graphics.common@1.0
else ifeq ($(HIDL_COMMON_VERSION_SCALED), 110)
    LOCAL_SHARED_LIBRARIES += android.hardware.graphics.common@1.1
else ifeq ($(HIDL_COMMON_VERSION_SCALED), 120)
    LOCAL_SHARED_LIBRARIES += android.hardware.graphics.common@1.2
endif

LOCAL_MULTILIB := both
include $(BUILD_STATIC_LIBRARY)

# Host build only on Android 10 and later due to misisng libutils on P.
ifeq ($(PLATFORM_SDK_GREATER_THAN_28), 1)
include $(CLEAR_VARS)
LOCAL_MODULE := $(MULTIARCH_MODULE)
LOCAL_MODULE_OWNER := $(MULTIARCH_MODULE_OWNER)
LOCAL_PROPRIETARY_MODULE := $(MULTIARCH_PROPRIETARY_MODULE)
LOCAL_MODULE_TAGS := $(MULTIARCH_MODULE_TAGS)
LOCAL_SRC_FILES := $(MULTIARCH_SRC_FILES)
LOCAL_C_INCLUDES := $(MULTIARCH_C_INCLUDES)
LOCAL_SHARED_LIBRARIES := $(MULTIARCH_SHARED_LIBRARIES)
LOCAL_CFLAGS := $(MULTIARCH_CFLAGS)

# Define to workaround missing HIDL headers.
LOCAL_CFLAGS += -DGRALLOC_HOST_BUILD=1

include $(BUILD_HOST_STATIC_LIBRARY)
endif