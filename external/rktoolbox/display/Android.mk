
LOCAL_PATH:= $(call my-dir)

common_cflags := \
    -Werror -Wno-unused-parameter -Wno-unused-const-variable \

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := \
    libcutils

LOCAL_SRC_FILES := \
           display.c \
           handle.c

LOCAL_CFLAGS += $(common_cflags)
LOCAL_MODULE := rk_display
include $(BUILD_EXECUTABLE)
