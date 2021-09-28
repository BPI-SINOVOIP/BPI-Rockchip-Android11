###############################################################################
# StressTest
LOCAL_PATH := $(call my-dir)

ifeq ($(strip $(TARGET_BOARD_HARDWARE)), rk30board)

include $(CLEAR_VARS)
LOCAL_MODULE := StressTest
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_TAGS := optional
LOCAL_BUILT_MODULE_STEM := package.apk
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_PRIVILEGED_MODULE := true
LOCAL_CERTIFICATE := platform
#LOCAL_OVERRIDES_PACKAGES := 

ifeq ($(strip $(TARGET_BOARD_HARDWARE)), rk30board)
  ifneq ($(filter atv box, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
	LOCAL_SRC_FILES := $(LOCAL_MODULE)_box.apk
  else
	LOCAL_SRC_FILES := $(LOCAL_MODULE).apk
  endif
else
ifeq ($(strip $(TARGET_BOARD_HARDWARE)), sofiaboard)
  LOCAL_SRC_FILES := $(LOCAL_MODULE)_sofia.apk
endif
endif

LOCAL_REQUIRED_MODULES := \
    getbootmode.sh
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := getbootmode.sh
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)
endif

