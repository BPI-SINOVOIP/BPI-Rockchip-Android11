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

my_dir := $(call my-dir)

LOCAL_PATH:= external/tensorflow/
include $(CLEAR_VARS)
LOCAL_MODULE := CtsTfliteNnapiDelegateTests_static
LOCAL_SRC_FILES := \
    tensorflow/lite/delegates/nnapi/nnapi_delegate_test.cc \
    tensorflow/lite/kernels/test_util.cc \
    tensorflow/core/platform/default/logging.cc \
    tensorflow/lite/kernels/acceleration_test_util.cc \
    tensorflow/lite/kernels/acceleration_test_util_internal.cc \
    tensorflow/lite/delegates/nnapi/acceleration_test_list.cc \
    tensorflow/lite/delegates/nnapi/acceleration_test_util.cc
LOCAL_CPP_EXTENSION := .cc

LOCAL_C_INCLUDES += external/flatbuffers/include
LOCAL_C_INCLUDES += external/tensorflow

LOCAL_CFLAGS :=  \
    -DPLATFORM_POSIX_ANDROID \
    -Wall \
    -Werror \
    -Wextra \
    -Wno-extern-c-compat \
    -Wno-sign-compare \
    -Wno-unused-parameter \
    -Wno-unused-private-field \

LOCAL_SHARED_LIBRARIES := libandroid liblog libneuralnetworks
LOCAL_STATIC_LIBRARIES := libgtest_ndk_c++ libgmock_ndk libtflite_static
LOCAL_HEADER_LIBRARIES := libeigen gemmlowp_headers
LOCAL_SDK_VERSION := current
LOCAL_NDK_STL_VARIANT := c++_static
include $(BUILD_STATIC_LIBRARY)


# Build the actual CTS module with the static lib above.
# This is necessary for the build system to pickup the AndroidTest.xml.
LOCAL_PATH:= $(my_dir)

include $(CLEAR_VARS)
LOCAL_MODULE := CtsTfliteNnapiDelegateTestCases
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nativetest
LOCAL_MULTILIB := both
LOCAL_MODULE_STEM_32 := $(LOCAL_MODULE)32
LOCAL_MODULE_STEM_64 := $(LOCAL_MODULE)64

LOCAL_WHOLE_STATIC_LIBRARIES := CtsTfliteNnapiDelegateTests_static

LOCAL_SHARED_LIBRARIES := libandroid liblog libneuralnetworks
LOCAL_STATIC_LIBRARIES := libgtest_ndk_c++ libtflite_static
LOCAL_CTS_TEST_PACKAGE := android.neuralnetworks

# Tag this module as a cts test artifact
LOCAL_COMPATIBILITY_SUITE := cts vts10 mts general-tests

LOCAL_SDK_VERSION := current
LOCAL_NDK_STL_VARIANT := c++_static

include $(BUILD_CTS_EXECUTABLE)
