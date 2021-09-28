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
#

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_RESOURCE_DIR += $(LOCAL_PATH)/res \
    packages/services/Car/service/res

LOCAL_AAPT_FLAGS += --extra-packages com.android.car --auto-add-overlay

LOCAL_PACKAGE_NAME := CarServiceTest
LOCAL_PRIVATE_PLATFORM_APIS := true

# for system|priviledged permission.
LOCAL_CERTIFICATE := platform

LOCAL_MODULE_TAGS := tests

# When built explicitly put it in the data partition
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA_APPS)

LOCAL_PROGUARD_ENABLED := disabled

LOCAL_STATIC_JAVA_LIBRARIES := junit
# testng imported to use assertThrows, we can remove it once it's ported to JUnit's.
LOCAL_STATIC_JAVA_LIBRARIES += \
    android.car.test.utils \
    androidx.test.ext.junit \
    androidx.test.rules \
    android.hardware.automotive.vehicle-V2.0-java \
    car-frameworks-service \
    car-service-test-static-lib \
    car-systemtest \
    com.android.car.test.utils \
    mockito-target-extended \
    testng \
    truth-prebuilt \
    vehicle-hal-support-lib-for-test \
    compatibility-device-util-axt

LOCAL_JAVA_LIBRARIES := \
    android.car \
    android.car.userlib \
    android.car.watchdoglib \
    android.test.runner \
    android.test.base

# mockito-target-inline dependency
LOCAL_JNI_SHARED_LIBRARIES := \
    libdexmakerjvmtiagent \
    libstaticjvmtiagent

LOCAL_COMPATIBILITY_SUITE := general-tests

include $(BUILD_PACKAGE)
