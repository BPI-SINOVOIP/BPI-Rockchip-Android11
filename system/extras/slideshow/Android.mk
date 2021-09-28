# Copyright 2015 The Android Open Source Project

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := slideshow.cpp
LOCAL_MODULE := slideshow
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -D__STDC_LIMIT_MACROS -Werror
LOCAL_SHARED_LIBRARIES := \
    libminui \
    libpng \
    libbase \
    libz \
    libutils \
    libcutils \
    liblog \
    libm \
    libc
include $(BUILD_EXECUTABLE)
