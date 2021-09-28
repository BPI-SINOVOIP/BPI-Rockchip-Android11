
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += bootable/recovery
LOCAL_SRC_FILES := recovery_ui.cpp

# should match TARGET_RECOVERY_UI_LIB set in BoardConfig.mk
LOCAL_MODULE := librecovery_ui_$(TARGET_PRODUCT)

include $(BUILD_STATIC_LIBRARY)

