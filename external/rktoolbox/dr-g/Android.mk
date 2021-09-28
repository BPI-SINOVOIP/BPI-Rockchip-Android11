
LOCAL_PATH:= $(call my-dir)

common_cflags := \
    -Werror -Wno-unused-parameter -Wno-unused-const-variable \

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := \
    libcutils

LOCAL_SRC_FILES := \
           main.c \

LOCAL_CFLAGS += $(common_cflags)
LOCAL_MODULE := rk_dr-g
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := dr-g
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_MODULE_PATH := $(TARGET_SYSTEM_OUT_BIN)
LOCAL_SRC_FILES := dr-g
include $(BUILD_PREBUILT)



