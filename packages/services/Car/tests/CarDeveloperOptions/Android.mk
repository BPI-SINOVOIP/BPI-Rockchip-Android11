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
#

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        $(call all-logtags-files-under, src)

LOCAL_MODULE := carsettings-logtags

include $(BUILD_STATIC_JAVA_LIBRARY)

# Build the Settings APK
include $(CLEAR_VARS)

LOCAL_PACKAGE_NAME := CarDeveloperOptions
LOCAL_PRIVATE_PLATFORM_APIS := true
LOCAL_CERTIFICATE := platform
LOCAL_SYSTEM_EXT_MODULE := true
LOCAL_PRIVILEGED_MODULE := true
LOCAL_REQUIRED_MODULES := privapp_whitelist_com.android.car.developeroptions
LOCAL_MODULE_TAGS := optional
LOCAL_USE_AAPT2 := true

LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_STATIC_ANDROID_LIBRARIES := \
    androidx-constraintlayout_constraintlayout \
    androidx.slice_slice-builders \
    androidx.slice_slice-core \
    androidx.slice_slice-view \
    androidx.core_core \
    androidx.appcompat_appcompat \
    androidx.cardview_cardview \
    androidx.preference_preference \
    androidx.recyclerview_recyclerview \
    com.google.android.material_material \
    setupcompat \
    setupdesign

LOCAL_JAVA_LIBRARIES := \
    telephony-common \
    android.car.settings \
    ims-common

LOCAL_STATIC_JAVA_LIBRARIES := \
    androidx-constraintlayout_constraintlayout-solver \
    androidx.lifecycle_lifecycle-runtime \
    androidx.lifecycle_lifecycle-extensions \
    guava \
    jsr305 \
    carsettings-contextual-card-protos-lite \
    carsettings-log-bridge-protos-lite \
    carsettings-logtags \
    statslog-settings \
    zxing-core-1.7

LOCAL_PROGUARD_FLAG_FILES := proguard.flags

ifneq ($(INCREMENTAL_BUILDS),)
    LOCAL_PROGUARD_ENABLED := disabled
    LOCAL_JACK_ENABLED := incremental
    LOCAL_JACK_FLAGS := --multi-dex native
endif

include frameworks/base/packages/SettingsLib/common.mk
include frameworks/base/packages/SettingsLib/search/common.mk

include $(BUILD_PACKAGE)

# ====  prebuilt library  ========================
include $(CLEAR_VARS)

include $(BUILD_MULTI_PREBUILT)

# Use the following include to make our test apk.
ifeq (,$(ONE_SHOT_MAKEFILE))
include $(call all-makefiles-under,$(LOCAL_PATH))
endif
