###############################################################################
# PinyinIME
LOCAL_PATH := $(call my-dir)

ifeq ($(strip $(TARGET_BOARD_HARDWARE)), rk30board)
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), box)
include $(CLEAR_VARS)
LOCAL_MULTILIB := 32
LOCAL_MODULE := PinyinIME
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_TAGS := optional
LOCAL_BUILT_MODULE_STEM := package.apk
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_CERTIFICATE := shared
LOCAL_SRC_FILES := $(LOCAL_MODULE).apk
LOCAL_PREBUILT_JNI_LIBS := \
    lib/arm/libjni_pinyinime.so
include $(BUILD_PREBUILT)
endif
endif
