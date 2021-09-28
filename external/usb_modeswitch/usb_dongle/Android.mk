LOCAL_PATH:= $(call my-dir)

common_src_files := \
	NetlinkManager.cpp \
	NetlinkHandler.cpp \
	MiscManager.cpp \
	Misc.cpp \
	G3Dev.cpp

common_shared_libraries := \
	libsysutils \
	libbinder \
	libcutils \
	liblog \
	libselinux \
	libutils \
	libbase \
	libhwbinder

common_cflags := -Werror -Wall -Wno-missing-field-initializers -Wno-unused-variable -Wno-unused-parameter

common_local_tidy_flags := -warnings-as-errors=clang-analyzer-security*,cert-*
common_local_tidy_checks := -*,clang-analyzer-security*,cert-*,-cert-err34-c,-cert-err58-cpp
common_local_tidy_checks += ,-cert-env33-c

include $(CLEAR_VARS)

LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MODULE := usb_dongle
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CLANG := true
LOCAL_TIDY := true
LOCAL_TIDY_FLAGS := $(common_local_tidy_flags)
LOCAL_TIDY_CHECKS := $(common_local_tidy_checks)
LOCAL_SRC_FILES := \
	main.cpp \
	$(common_src_files)

LOCAL_INIT_RC := usb_dongle.rc

LOCAL_CFLAGS := $(common_cflags)
LOCAL_CFLAGS += -Werror=format
LOCAL_CFLAGS += -DUSE_USB_MODE_SWITCH

LOCAL_SHARED_LIBRARIES := $(common_shared_libraries)

include $(BUILD_EXECUTABLE)
