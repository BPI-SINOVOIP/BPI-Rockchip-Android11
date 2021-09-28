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

TOP_LOCAL_PATH := $(call my-dir)

# Blueprint builds disable Android.mk use
MALI_GRALLOC_API_TESTS?=0

ifdef GRALLOC_USE_GRALLOC1_API
    ifdef GRALLOC_API_VERSION
        $(warning GRALLOC_API_VERSION flag is not compatible with GRALLOC_USE_GRALLOC1_API)
    endif
endif

include $(TOP_LOCAL_PATH)/gralloc.version.mk

#Build allocator for version >= 2.x and gralloc libhardware HAL for all previous versions.
GRALLOC_MAPPER := 0
ifeq ($(shell expr $(GRALLOC_VERSION_MAJOR) \>= 2), 1)
    $(info Build Gralloc allocator for $(GRALLOC_API_VERSION))
else
    $(info Build Gralloc 1.x libhardware HAL)
    $(warning Building for unsupported Gralloc version - Gralloc 1.x)
endif
include $(TOP_LOCAL_PATH)/src/Android.mk

ifeq ($(shell expr $(GRALLOC_VERSION_MAJOR) \>= 2), 1)
   GRALLOC_MAPPER := 1
   $(info Build Gralloc mapper for $(GRALLOC_API_VERSION))
   include $(TOP_LOCAL_PATH)/src/Android.mk
endif

# Build service
ifeq ($(GRALLOC_VERSION_MAJOR), 3)
   $(info Build 3.0 IAllocator service.)
   include $(TOP_LOCAL_PATH)/service/3.x/Android.mk
else ifeq ($(GRALLOC_VERSION_MAJOR), 4)
   $(info Build 4.0 IAllocator service.)
   include $(TOP_LOCAL_PATH)/service/4.x/Android.mk
endif

# Build gralloc api tests.
ifeq ($(MALI_GRALLOC_API_TESTS), 1)
   $(info Build gralloc API tests.)
   include $(TOP_LOCAL_PATH)/tests/Android.mk
endif
