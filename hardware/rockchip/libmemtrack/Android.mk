# Copyright (C) 2013 The Android Open Source Project
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

ifneq ($(strip $(TARGET_BOARD_PLATFORM)), rk3368)
LOCAL_PATH := $(call my-dir)

# HAL module implemenation stored in
# <POWERS_HARDWARE_MODULE_ID>.<ro.hardware>.so
include $(CLEAR_VARS)

LOCAL_SRC_FILES := memtrack.cpp
ifneq ($(filter rk3128h rk3328 rk322x, $(strip $(TARGET_BOARD_PLATFORM))), )
LOCAL_SRC_FILES += rk322x.cpp
else
ifneq ($(filter rk3326, $(strip $(TARGET_BOARD_PLATFORM))), )
LOCAL_SRC_FILES += rk3326.cpp
else
ifneq ($(filter rk3399 rk3399pro, $(strip $(TARGET_BOARD_PLATFORM))), )
LOCAL_SRC_FILES += rk3399.cpp
else
LOCAL_SRC_FILES += rk_common.cpp
endif
endif
endif
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_HEADER_LIBRARIES := libhardware_headers
LOCAL_SHARED_LIBRARIES :=  liblog
LOCAL_MODULE := memtrack.$(TARGET_BOARD_PLATFORM)
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)
endif
