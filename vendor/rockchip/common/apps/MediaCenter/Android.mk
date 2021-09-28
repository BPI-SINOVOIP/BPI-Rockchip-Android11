###############################################################################
# MediaCenter
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := MediaCenter
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_TAGS := optional
LOCAL_BUILT_MODULE_STEM := package.apk
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_PRIVILEGED_MODULE :=true
LOCAL_CERTIFICATE := platform
LOCAL_OVERRIDES_PACKAGES := Launcher3 
LOCAL_SRC_FILES := $(LOCAL_MODULE).apk
ifeq ($(strip $(TARGET_ARCH)), arm)
LOCAL_PREBUILT_JNI_LIBS := \
    lib/arm/libmediacenter-jni.so
LOCAL_PREBUILT_JNI_LIBS += \
    lib/arm/libImageUtil.so
else ifeq ($(strip $(TARGET_ARCH)), arm64)
LOCAL_PREBUILT_JNI_LIBS := \
    lib/arm64/libmediacenter-jni.so
LOCAL_PREBUILT_JNI_LIBS += \
    lib/arm64/libImageUtil.so
endif
include $(BUILD_PREBUILT)
#LOCAL_REQUIRED_MODULES :=
#LOCAL_MULTILIB := 32
#include $(BUILD_PREBUILT)
