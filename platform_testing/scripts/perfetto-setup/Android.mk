#
# Copyright (C) 2019 The Android Open Source Project
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
include $(CLEAR_VARS)
LOCAL_MODULE := trace_config_detailed.textproto
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/local/tmp
LOCAL_PREBUILT_MODULE_FILE := prebuilts/tools/linux-x86_64/perfetto/configs/trace_config_detailed.textproto
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := long_trace_config.textproto
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/local/tmp
LOCAL_PREBUILT_MODULE_FILE := prebuilts/tools/linux-x86_64/perfetto/configs/long_trace_config.textproto
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := trace_config.textproto
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/local/tmp
LOCAL_PREBUILT_MODULE_FILE := prebuilts/tools/linux-x86_64/perfetto/configs/trace_config.textproto
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := trace_config_experimental.textproto
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/local/tmp
LOCAL_PREBUILT_MODULE_FILE := prebuilts/tools/linux-x86_64/perfetto/configs/trace_config_experimental.textproto
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := perfetto_trace_processor_shell
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/local/tmp
LOCAL_CHECK_ELF_FILES := false
LOCAL_PREBUILT_MODULE_FILE := prebuilts/tools/linux-x86_64/perfetto/trace_processor_shell
include $(BUILD_PREBUILT)


