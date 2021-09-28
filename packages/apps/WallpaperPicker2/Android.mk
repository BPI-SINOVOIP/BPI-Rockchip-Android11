#
# Copyright (C) 2017 The Android Open Source Project
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
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := wallpaper2-glide-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../prebuilts/maven_repo/bumptech/com/github/bumptech/glide/glide/SNAPSHOT/glide-SNAPSHOT$(COMMON_JAVA_PACKAGE_SUFFIX)
LOCAL_UNINSTALLABLE_MODULE := true
LOCAL_JETIFIER_ENABLED := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := wallpaper2-disklrucache-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../prebuilts/maven_repo/bumptech/com/github/bumptech/glide/disklrucache/SNAPSHOT/disklrucache-SNAPSHOT$(COMMON_JAVA_PACKAGE_SUFFIX)
LOCAL_UNINSTALLABLE_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := wallpaper2-gifdecoder-target
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := ../../../prebuilts/maven_repo/bumptech/com/github/bumptech/glide/gifdecoder/SNAPSHOT/gifdecoder-SNAPSHOT$(COMMON_JAVA_PACKAGE_SUFFIX)
LOCAL_UNINSTALLABLE_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_USE_AAPT2 := true
LOCAL_AAPT2_ONLY := true
LOCAL_MODULE_TAGS := optional

LOCAL_STATIC_ANDROID_LIBRARIES := android-support-exifinterface
LOCAL_SRC_FILES := $(call all-java-files-under, ../../../external/subsampling-scale-image-view/library/src)
LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/../../../external/subsampling-scale-image-view/library/src/main/res

LOCAL_PROGUARD_ENABLED := disabled

LOCAL_SDK_VERSION := current
LOCAL_MIN_SDK_VERSION := 26
LOCAL_MODULE := wallpaper-subsampling-scale-image-view
LOCAL_MANIFEST_FILE := ../../../external/subsampling-scale-image-view/library/src/main/AndroidManifest.xml

include $(BUILD_STATIC_JAVA_LIBRARY)


#
# Build rule for WallpaperPicker2 dependencies lib.
#
include $(CLEAR_VARS)
LOCAL_USE_AAPT2 := true
LOCAL_MODULE_TAGS := optional

LOCAL_STATIC_ANDROID_LIBRARIES := \
    androidx.appcompat_appcompat \
    androidx.cardview_cardview \
    androidx.recyclerview_recyclerview \
    androidx.slice_slice-view \
    androidx-constraintlayout_constraintlayout \
    com.google.android.material_material \
    androidx.exifinterface_exifinterface \
    wallpaper-subsampling-scale-image-view

LOCAL_STATIC_JAVA_LIBRARIES := \
    wallpaper2-glide-target \
    wallpaper2-disklrucache-target \
    wallpaper2-gifdecoder-target \
    volley \
    libbackup \
    SystemUISharedLib

LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/res

LOCAL_PROGUARD_ENABLED := disabled
LOCAL_MANIFEST_FILE := AndroidManifest.xml

ifneq (,$(wildcard frameworks/base))
    LOCAL_PRIVATE_PLATFORM_APIS := true
else
    LOCAL_SDK_VERSION := current
endif
LOCAL_MODULE := WallpaperPicker2CommonDepsLib
LOCAL_PRIVILEGED_MODULE := true

include $(BUILD_STATIC_JAVA_LIBRARY)

#
# Build app code.
#
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_USE_AAPT2 := true

LOCAL_STATIC_ANDROID_LIBRARIES := WallpaperPicker2CommonDepsLib

LOCAL_SRC_FILES := $(call all-java-files-under, src) \
    $(call all-java-files-under, src_override)

LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/res

LOCAL_PROGUARD_FLAG_FILES := proguard.flags
LOCAL_PROGUARD_ENABLED := disabled

LOCAL_PRIVILEGED_MODULE := true

ifneq (,$(wildcard frameworks/base))
  LOCAL_PRIVATE_PLATFORM_APIS := true
else
  LOCAL_SDK_VERSION := system_current
  LOCAL_STATIC_JAVA_LIBRARIES += libSharedWallpaper
endif

LOCAL_PACKAGE_NAME := WallpaperPicker2
LOCAL_JETIFIER_ENABLED := true

include $(BUILD_PACKAGE)

include $(call all-makefiles-under,$(LOCAL_PATH))
