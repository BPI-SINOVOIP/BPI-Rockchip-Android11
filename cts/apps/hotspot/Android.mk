LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE_PATH := $(TARGET_OUT_DATA_APPS)

LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_PACKAGE_NAME := hotspot

LOCAL_SDK_VERSION := current

LOCAL_STATIC_JAVA_LIBRARIES := android-support-v4

LOCAL_COMPATIBILITY_SUITE := cts sts vts10

include $(BUILD_CTS_PACKAGE)
