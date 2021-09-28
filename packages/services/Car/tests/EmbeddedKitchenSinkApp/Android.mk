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

#disble build in PDK, missing ui-lib breaks build
ifneq ($(TARGET_BUILD_PDK),true)

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_USE_AAPT2 := true

LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/res

LOCAL_PACKAGE_NAME := EmbeddedKitchenSinkApp
LOCAL_PRIVATE_PLATFORM_APIS := true

LOCAL_MODULE_TAGS := optional

LOCAL_PRIVILEGED_MODULE := true

LOCAL_CERTIFICATE := platform

LOCAL_PROGUARD_ENABLED := disabled

LOCAL_DEX_PREOPT := false

LOCAL_STATIC_ANDROID_LIBRARIES += \
    car-service-test-static-lib \
    com.google.android.material_material \
    androidx.appcompat_appcompat \
    car-ui-lib

LOCAL_STATIC_JAVA_LIBRARIES += \
    android.hidl.base-V1.0-java \
    android.hardware.automotive.vehicle-V2.0-java \
    vehicle-hal-support-lib-for-test \
    com.android.car.keventreader-client \
    guava \
    android.car.cluster.navigation \
    car-experimental-api-static-lib

LOCAL_JAVA_LIBRARIES += android.car

LOCAL_REQUIRED_MODULES := privapp_whitelist_com.google.android.car.kitchensink

include $(BUILD_PACKAGE)

include $(CLEAR_VARS)

include $(BUILD_MULTI_PREBUILT)

include $(CLEAR_VARS)

endif #TARGET_BUILD_PDK
