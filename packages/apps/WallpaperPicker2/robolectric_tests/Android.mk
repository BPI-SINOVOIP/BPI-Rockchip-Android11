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

#############################################
# WallPaperPicker2 Robolectric test target. #
#############################################
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := WallPaperPicker2RoboTests
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_STATIC_JAVA_LIBRARIES := \
    androidx.test.runner \
    androidx.test.rules \
    mockito-robolectric-prebuilt \
    truth-prebuilt
LOCAL_JAVA_LIBRARIES := \
    platform-robolectric-3.6.2-prebuilt

LOCAL_JAVA_RESOURCE_DIRS := config

LOCAL_INSTRUMENTATION_FOR := WallpaperPicker2
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_JAVA_LIBRARY)

############################################
# Target to run the previous target.       #
############################################
include $(CLEAR_VARS)

LOCAL_MODULE := RunWallPaperPicker2RoboTests
LOCAL_SDK_VERSION := current
LOCAL_JAVA_LIBRARIES := \
    WallPaperPicker2RoboTests

LOCAL_TEST_PACKAGE := WallpaperPicker2

LOCAL_INSTRUMENT_SOURCE_DIRS := packages/apps/WallpaperPicker2/src \

LOCAL_ROBOTEST_TIMEOUT := 36000

include prebuilts/misc/common/robolectric/3.6.2/run_robotests.mk
