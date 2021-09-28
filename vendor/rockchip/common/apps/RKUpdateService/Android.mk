###############################################################################
# RKUpdateService
LOCAL_PATH := $(call my-dir)

ifeq ($(strip $(TARGET_BOARD_HARDWARE)), rk30board)
include $(CLEAR_VARS)
LOCAL_MODULE := RKUpdateService
LOCAL_DEX_PREOPT := false
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_TAGS := optional
LOCAL_BUILT_MODULE_STEM := package.apk
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
#LOCAL_PRIVILEGED_MODULE :=
#LOCAL_CERTIFICATE := PRESIGNED
LOCAL_CERTIFICATE := platform
#LOCAL_OVERRIDES_PACKAGES := 
LOCAL_SRC_FILES := $(LOCAL_MODULE).apk
#LOCAL_REQUIRED_MODULES :=
ifeq ($(strip $(TARGET_ARCH)), arm)
LOCAL_PREBUILT_JNI_LIBS := \
    lib/arm/librockchip_update_jni.so
else ifeq ($(strip $(TARGET_ARCH)), arm64)
LOCAL_PREBUILT_JNI_LIBS := \
    lib/arm64/librockchip_update_jni.so
endif
include $(BUILD_PREBUILT)
endif

ifeq ($(strip $(TARGET_BOARD_HARDWARE)), sofiaboard)
include $(CLEAR_VARS)
LOCAL_MODULE := RKUpdateService
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_TAGS := optional
LOCAL_BUILT_MODULE_STEM := package.apk
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
#LOCAL_PRIVILEGED_MODULE :=
LOCAL_CERTIFICATE := PRESIGNED
#LOCAL_OVERRIDES_PACKAGES := 
LOCAL_SRC_FILES := $(LOCAL_MODULE)_sofia.apk
#LOCAL_REQUIRED_MODULES :=
#LOCAL_PREBUILT_JNI_LIBS := 
include $(BUILD_PREBUILT)
endif
