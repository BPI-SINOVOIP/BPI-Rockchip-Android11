# Copyright (C) 2016 The Android Open Source Project
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

LOCAL_SRC_FILES := \
    $(call all-java-files-under, src) \
    ../../../../../frameworks/opt/car/services/src/com/android/internal/car/ICarServiceHelper.aidl

LOCAL_RESOURCE_DIR += $(LOCAL_PATH)/res \
    packages/services/Car/service/res

LOCAL_AAPT_FLAGS += --extra-packages com.android.car --auto-add-overlay

LOCAL_PACKAGE_NAME := CarServiceUnitTest
LOCAL_PRIVATE_PLATFORM_APIS := true

LOCAL_CERTIFICATE := platform

LOCAL_MODULE_TAGS := tests

# When built explicitly put it in the data partition
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA_APPS)

LOCAL_PROGUARD_ENABLED := disabled

LOCAL_INSTRUMENTATION_FOR := CarService

LOCAL_JAVA_LIBRARIES := \
    android.car \
    android.car.userlib \
    android.car.watchdoglib \
    android.test.runner \
    android.test.base \
    android.test.mock \
    EncryptionRunner

LOCAL_STATIC_JAVA_LIBRARIES := \
    android.car.testapi \
    android.car.test.utils \
    androidx.test.core \
    androidx.test.ext.junit \
    androidx.test.rules \
    car-frameworks-service \
    car-service-test-static-lib \
    com.android.car.test.utils \
    frameworks-base-testutils \
    mockito-target-extended \
    testng \
    truth-prebuilt

LOCAL_COMPATIBILITY_SUITE := general-tests

# mockito-target-inline dependency
LOCAL_JNI_SHARED_LIBRARIES := \
    libdexmakerjvmtiagent \
    libstaticjvmtiagent \

include $(BUILD_PACKAGE)
