LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
    drmservice.c

LOCAL_MULTILIB := 32

LOCAL_C_INCLUDES += bionic \
$(call include-path-for, libhardware_legacy)/hardware_legacy

LOCAL_MODULE := rockchip.drmservice
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_INIT_RC := init.rockchip.drmservice.rc
LOCAL_STATIC_LIBRARIES := libcutils 
LOCAL_SHARED_LIBRARIES := libhardware_legacy libnetutils liblog
LOCAL_CPPFLAGS := \
    -Werror \
    -Wunused-variable \
    -Wmacro-redefined

# API 28 -> Android 9.0
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 29)))
LOCAL_CFLAGS += -DENABLE_CMDLINE_VERIFY
endif

# Verify sn with pattern "^[A-Za-z0-9]+$"
ifeq ($(BUILD_WITH_GOOGLE_MARKET), true)
LOCAL_CFLAGS += -DENABLE_SN_VERIFY
endif

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
    test/vendor_storage_test.c

# Prevent ioctl errors in vendor_storage with 32-bit
LOCAL_MULTILIB := 32
LOCAL_MODULE := vendor_storage_test
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_CPPFLAGS := \
    -Werror \
    -Wmacro-redefined
include $(BUILD_EXECUTABLE)
