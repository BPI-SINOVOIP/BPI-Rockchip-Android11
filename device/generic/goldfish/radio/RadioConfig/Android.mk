LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRIVILEGED_MODULE := true
LOCAL_SRC_FILES := $(call all-subdir-java-files)
LOCAL_PACKAGE_NAME := EmulatorRadioConfig
LOCAL_PRIVATE_PLATFORM_APIS := true
LOCAL_CERTIFICATE := platform
LOCAL_SYSTEM_EXT_MODULE := true
LOCAL_REQUIRED_MODULES := privapp_whitelist_com.android.emulator.radio.config
LOCAL_PROGUARD_ENABLED := disabled

include $(BUILD_PACKAGE)
