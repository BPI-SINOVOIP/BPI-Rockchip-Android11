# Copyright (C) 2017 The Android Open Source Project
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

nnapi_cts_dir := $(call my-dir)

# Build the actual CTS module with the static lib above.
# This is necessary for the build system to pickup the AndroidTest.xml.
LOCAL_PATH:= $(nnapi_cts_dir)

include $(CLEAR_VARS)
LOCAL_MODULE := CtsNNAPITestCases
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nativetest
LOCAL_MULTILIB := both
LOCAL_MODULE_STEM_32 := $(LOCAL_MODULE)32
LOCAL_MODULE_STEM_64 := $(LOCAL_MODULE)64

LOCAL_WHOLE_STATIC_LIBRARIES := CtsNNAPITests_static

LOCAL_SHARED_LIBRARIES := libandroid liblog libneuralnetworks
LOCAL_STATIC_LIBRARIES := libbase_ndk libgtest_ndk_c++ libgmock_ndk
LOCAL_CTS_TEST_PACKAGE := android.neuralnetworks

# Tag this module as a cts test artifact
LOCAL_COMPATIBILITY_SUITE := cts vts10 mts general-tests

LOCAL_SDK_VERSION := current
LOCAL_NDK_STL_VARIANT := c++_static

include $(BUILD_CTS_EXECUTABLE)

include $(nnapi_cts_dir)/benchmark/Android.mk
include $(nnapi_cts_dir)/tflite_delegate/Android.mk
