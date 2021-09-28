#
# Copyright (C) 2014 The Android Open Source Project
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

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)
LOCAL_PACKAGE_NAME := CtsSplitAppFeature
LOCAL_SDK_VERSION := current
LOCAL_MIN_SDK_VERSION := 4
LOCAL_PACKAGE_SPLITS := v7

LOCAL_ASSET_DIR := $(LOCAL_PATH)/assets

LOCAL_CERTIFICATE := cts/hostsidetests/appsecurity/certs/cts-testkey1
LOCAL_AAPT_FLAGS := --version-code 100 --version-name OneHundred --replace-version

LOCAL_MODULE_TAGS := tests

# tag this module as a cts test artifact
LOCAL_COMPATIBILITY_SUITE := cts vts10 general-tests

LOCAL_USE_AAPT2 := true
LOCAL_APK_LIBRARIES := CtsSplitApp
LOCAL_RES_LIBRARIES := $(LOCAL_APK_LIBRARIES)
LOCAL_AAPT_FLAGS += --package-id 0x80 --rename-manifest-package com.android.cts.splitapp

include $(BUILD_CTS_SUPPORT_PACKAGE)


#################################################
# Define a variant requiring a split for install

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests
LOCAL_STATIC_JAVA_LIBRARIES := androidx.test.rules

LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_MANIFEST_FILE := needsplit/AndroidManifest.xml

LOCAL_PACKAGE_NAME := CtsNeedSplitFeature
LOCAL_SDK_VERSION := current
LOCAL_MIN_SDK_VERSION := 4

LOCAL_COMPATIBILITY_SUITE := cts vts10 general-tests

LOCAL_ASSET_DIR := $(LOCAL_PATH)/assets

LOCAL_CERTIFICATE := cts/hostsidetests/appsecurity/certs/cts-testkey1
LOCAL_APK_LIBRARIES := CtsSplitApp
LOCAL_RES_LIBRARIES := $(LOCAL_APK_LIBRARIES)
LOCAL_JAVA_LIBRARIES := android.test.runner.stubs android.test.base.stubs
LOCAL_AAPT_FLAGS := --version-code 100 --version-name OneHundred --replace-version
LOCAL_AAPT_FLAGS += --package-id 0x80 --rename-manifest-package com.android.cts.splitapp

LOCAL_USE_AAPT2 := true
LOCAL_PROGUARD_ENABLED := disabled
LOCAL_DEX_PREOPT := false

include $(BUILD_CTS_SUPPORT_PACKAGE)
