#
# Copyright (C) 2020 Arm Limited.
# SPDX-License-Identifier: Apache-2.0
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

LOCAL_MODULE := android.hardware.graphics.allocator@4.0-service
LOCAL_INIT_RC := android.hardware.graphics.allocator@4.0-service.rc
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SHARED_LIBRARIES := libhidlbase liblog libhidltransport libutils
LOCAL_SHARED_LIBRARIES += android.hardware.graphics.allocator@4.0

LOCAL_SRC_FILES := service.cpp

include $(BUILD_EXECUTABLE)