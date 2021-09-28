# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SDK_VERSION := 26

LOCAL_SRC_FILES := \
    $(call all-java-files-under, src) \

LOCAL_RESOURCE_DIR := \
    $(LOCAL_PATH)/res \

LOCAL_MULTILIB := both
LOCAL_PACKAGE_NAME := C2E2ETest
LOCAL_JNI_SHARED_LIBRARIES := libcodectest
LOCAL_MODULE_TAGS := tests

c2_e2e_test_apk := $(call intermediates-dir-for,APPS,C2E2ETest)/package.apk
$(call dist-for-goals,dist_files,$(c2_e2e_test_apk):C2E2ETest.apk)

include $(BUILD_PACKAGE)

include $(call all-makefiles-under,$(LOCAL_PATH))
