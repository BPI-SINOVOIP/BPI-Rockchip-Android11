LOCAL_PATH := $(call my-dir)
# TODO:  Find a better way to separate build configs for ADP vs non-ADP devices
ifneq ($(BOARD_IS_AUTOMOTIVE),true)
  ifneq ($(filter msm8x27 msm8226,$(TARGET_BOARD_PLATFORM)),)
    include $(call all-named-subdir-makefiles,msm8960)
  else ifneq ($(filter msm8994,$(TARGET_BOARD_PLATFORM)),)
    include $(call all-named-subdir-makefiles,msm8992)
  else ifneq ($(filter msm8909,$(TARGET_BOARD_PLATFORM)),)
    ifeq ($(TARGET_SUPPORTS_QCOM_3100),true)
      include $(call all-named-subdir-makefiles,msm8909w_3100)
    else
      include $(call all-named-subdir-makefiles,msm8909)
    endif
  else ifneq ($(wildcard $(LOCAL_PATH)/$(TARGET_BOARD_PLATFORM)),)
    include $(call all-named-subdir-makefiles,$(TARGET_BOARD_PLATFORM))
  endif
endif
