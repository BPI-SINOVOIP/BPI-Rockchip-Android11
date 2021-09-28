# TODO:  Find a better way to separate build configs for ADP vs non-ADP devices
ifneq ($(TARGET_BOARD_AUTO),true)
  ifneq ($(filter msm8960 msm8974,$(TARGET_BOARD_PLATFORM)),)
    LOCAL_PATH := $(call my-dir)

    # HAL module implemenation stored in
    # hw/<POWERS_HARDWARE_MODULE_ID>.<ro.hardware>.so
    include $(CLEAR_VARS)


    LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
    LOCAL_SHARED_LIBRARIES := liblog libcutils libdl
    LOCAL_SRC_FILES := \
                       power.c \

    LOCAL_MODULE:= power.$(TARGET_BOARD_PLATFORM)
    LOCAL_MODULE_TAGS := optional
    LOCAL_CFLAGS += -Wno-unused-parameter
    include $(BUILD_SHARED_LIBRARY)
  endif
endif
