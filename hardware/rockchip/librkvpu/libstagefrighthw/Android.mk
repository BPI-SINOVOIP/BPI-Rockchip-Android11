#
# Copyright (C) 2009 The Android Open Source Project
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
#

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    RKOMXPlugin.cpp

#enable log
#LOCAL_CFLAGS += -DLOG_NDEBUG=0

USE_ROCKCHIP_OMX:=true

ifeq ($(USE_ROCKCHIP_OMX),true)
    LOCAL_CFLAGS += -DUSE_ROCKCHIP_OMX
endif

ifeq ($(USE_MEDIASDK),true)
    LOCAL_CFLAGS += -DUSE_MEDIASDK
endif

ifeq ($(USE_INTEL_MDP),true)
    LOCAL_CFLAGS += -DUSE_INTEL_MDP
endif

LOCAL_C_INCLUDES:= \
        $(call include-path-for, frameworks-native)/media/hardware \
        $(call include-path-for, frameworks-native)/media/openmax

LOCAL_SHARED_LIBRARIES :=       \
        libbinder               \
        libutils                \
        libcutils               \
        libui                   \
        libdl                   \
        libstagefright_foundation \
	liblog                    \

LOCAL_MODULE := libstagefrighthw
LOCAL_PROPRIETARY_MODULE := true
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_OWNER := rockchip,intel

include $(BUILD_SHARED_LIBRARY)

