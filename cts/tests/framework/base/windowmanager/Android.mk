#
# Copyright (C) 2015 The Android Open Source Project
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

LOCAL_MODULE_TAGS := tests optional

LOCAL_SRC_FILES := \
    $(call all-java-files-under, src) \
    $(call all-java-files-under, alertwindowservice/src) \
    $(call all-named-files-under,Components.java, *) \
    ../../../../apps/CtsVerifier/src/com/android/cts/verifier/vr/MockVrListenerService.java \

LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/res

LOCAL_ASSET_DIR := $(LOCAL_PATH)/intent_tests

LOCAL_PACKAGE_NAME := CtsWindowManagerDeviceTestCases

LOCAL_JAVA_LIBRARIES := android.test.runner.stubs

LOCAL_STATIC_JAVA_LIBRARIES := \
    compatibility-device-util-axt \
    androidx.test.ext.junit \
    androidx.test.rules \
    hamcrest-library \
    platform-test-annotations \
    cts-wm-util \
    CtsSurfaceValidatorLib \
    CtsMockInputMethodLib \
    metrics-helper-lib \

LOCAL_COMPATIBILITY_SUITE := cts vts10 general-tests sts

LOCAL_SDK_VERSION := test_current

include $(BUILD_CTS_PACKAGE)

include $(call all-makefiles-under,$(LOCAL_PATH))
