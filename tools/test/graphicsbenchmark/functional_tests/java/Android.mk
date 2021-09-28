# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PACKAGE_NAME := GameCoreJavaTestCases
LOCAL_SDK_VERSION := 26  # Oreo
LOCAL_STATIC_JAVA_LIBRARIES := \
    androidx.test.rules \
    repackaged.android.test.base \
    truth-prebuilt \
    GameCoreHelper
LOCAL_JNI_SHARED_LIBRARIES := \
    libgamecore_sample \
    libgamecore_java_tests_jni \
    libagq
LOCAL_MODULE_TAGS := tests
LOCAL_COMPATIBILITY_SUITE := device-tests
LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_NDK_STL_VARIANT := c++_shared

include $(BUILD_PACKAGE)

# Build all sub-directories
include $(call all-makefiles-under,$(LOCAL_PATH))
