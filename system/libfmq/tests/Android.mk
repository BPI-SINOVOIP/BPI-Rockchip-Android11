#
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

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := fmq_test
LOCAL_MODULE_CLASS := NATIVE_TESTS
LOCAL_SRC_FILES := fmq_test
LOCAL_REQUIRED_MODULES :=                           \
    mq_test_client                                  \
    android.hardware.tests.msgq@1.0-service-test    \
    hidl_test_helper

LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nativetest64

ifneq ($(TARGET_2ND_ARCH),)
LOCAL_REQUIRED_MODULES += android.hardware.tests.msgq@1.0-service-test$(TARGET_2ND_ARCH_MODULE_SUFFIX)
LOCAL_REQUIRED_MODULES += mq_test_client$(TARGET_2ND_ARCH_MODULE_SUFFIX)
endif

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE := VtsFmqUnitTests
-include test/vts/tools/build/Android.host_config.mk
