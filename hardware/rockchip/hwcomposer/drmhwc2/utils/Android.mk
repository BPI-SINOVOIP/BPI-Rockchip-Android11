LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    worker.cpp

# API 30 -> Android 11.0
ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 30)))

LOCAL_C_INCLUDES += \
  hardware/rockchip/hwcomposer/drmhwc2/include
LOCAL_CPPFLAGS += -DANDROID_R

else
LOCAL_C_INCLUDES += \
  hardware/rockchip/hwcomposer/include

LOCAL_CPPFLAGS += -DANDROID_Q

endif

LOCAL_CPPFLAGS := \
    -Wall \
    -Werror

# API 26 -> Android 8.0
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 26)))
LOCAL_PROPRIETARY_MODULE := true
endif

LOCAL_MODULE := libdrmhwcutils

include $(BUILD_STATIC_LIBRARY)
