LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_USE_AAPT2 := true

LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/res

LOCAL_STATIC_ANDROID_LIBRARIES := \
    androidx.core_core \
    androidx.appcompat_appcompat

# Build all java files in the src subdirectory
LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_SDK_VERSION := current

LOCAL_PRODUCT_MODULE := true

# Name of the APK to build
LOCAL_PACKAGE_NAME := SampleLocationAttribution

# Tell it to build an APK
include $(BUILD_PACKAGE)
