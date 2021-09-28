ifneq ($(BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE),)

ifeq ($(TARGET_SUPPORTS_ANDROID_WEAR),true)
    LW_FEATURE_SET := true
endif

LOCAL_PATH := $(call my-dir)

include $(call all-makefiles-under,$(LOCAL_PATH))
endif
