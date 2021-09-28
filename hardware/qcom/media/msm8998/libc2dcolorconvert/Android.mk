LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        C2DColorConverter.cpp

LOCAL_CFLAGS := -Wall -Werror

# Allow implicit fallthrough in C2DColorConverter.cpp:554 until it is fixed.
LOCAL_CFLAGS += -Wno-implicit-fallthrough

LOCAL_C_INCLUDES := \
    $(TARGET_OUT_HEADERS)/adreno
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/qcom/display
ifeq ($(TARGET_COMPILE_WITH_MSM_KERNEL),true)
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
endif

LOCAL_SHARED_LIBRARIES := liblog libdl libutils

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libc2dcolorconvert

LOCAL_VENDOR_MODULE := true

ifeq ($(TARGET_COMPILE_WITH_MSM_KERNEL),true)
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
endif

LOCAL_HEADER_LIBRARIES += libhardware_headers
include $(BUILD_SHARED_LIBRARY)
