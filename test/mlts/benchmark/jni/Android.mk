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
#

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libnnbenchmark_jni
LOCAL_SRC_FILES := benchmark_jni.cpp run_tflite.cpp
LOCAL_C_INCLUDES := external/flatbuffers/include external/tensorflow
LOCAL_SHARED_LIBRARIES := libandroid liblog
LOCAL_STATIC_LIBRARIES := libtflite_static
LOCAL_CFLAGS := -Wno-sign-compare -Wno-unused-parameter
LOCAL_SDK_VERSION := 28
LOCAL_NDK_STL_VARIANT := c++_static

include $(BUILD_SHARED_LIBRARY)
include $(call all-makefiles-under,$(LOCAL_PATH))
