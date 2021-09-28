LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libcld80211
LOCAL_CLANG := true
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES += $(LOCAL_PATH) \
	external/libnl/include
LOCAL_SHARED_LIBRARIES := libcutils libnl liblog
LOCAL_SRC_FILES := cld80211_lib.c
LOCAL_CFLAGS += -Wall -Werror -Wno-unused-parameter
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
LOCAL_VENDOR_MODULE := true
include $(BUILD_SHARED_LIBRARY)

