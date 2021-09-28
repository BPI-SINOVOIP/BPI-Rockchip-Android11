###############################################################################
# RKDeviceTest
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := RKDeviceTest
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_TAGS := optional
LOCAL_BUILT_MODULE_STEM := package.apk
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
#LOCAL_PRIVILEGED_MODULE :=
LOCAL_CERTIFICATE := PRESIGNED
#LOCAL_OVERRIDES_PACKAGES := 
LOCAL_SRC_FILES := $(LOCAL_MODULE).apk
#LOCAL_REQUIRED_MODULES :=
ifeq ($(strip $(TARGET_ARCH)), arm)
LOCAL_PREBUILT_JNI_LIBS := \
    lib/arm/libstlport.so
else ifeq ($(strip $(TARGET_ARCH)), arm64)
LOCAL_PREBUILT_JNI_LIBS := \
    lib/arm64/libstlport.so
endif
include $(BUILD_PREBUILT)
