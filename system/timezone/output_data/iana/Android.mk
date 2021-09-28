# -*- mode: makefile -*-
# Copyright (C) 2018 The Android Open Source Project
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

LOCAL_PATH:= $(call my-dir)

ifeq ($(WITH_HOST_DALVIK),true)

# Host simulations of APEX modules for use by hostdex rules.

# Simulate the time zone data module.
include $(CLEAR_VARS)
LOCAL_MODULE := tzdata_host_tzdata_apex
LOCAL_IS_HOST_MODULE := true
LOCAL_SRC_FILES := tzdata
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_STEM := $(LOCAL_SRC_FILES)
LOCAL_MODULE_PATH := $(HOST_OUT)/com.android.tzdata/etc/tz
include $(BUILD_PREBUILT)

endif
