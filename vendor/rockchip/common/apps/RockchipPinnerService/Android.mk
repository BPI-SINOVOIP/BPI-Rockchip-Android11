###############################################################################
# PinnerService
LOCAL_PATH:= $(my-dir)

include $(CLEAR_VARS)
LOCAL_PACKAGE_NAME := RockchipPinnerService
LOCAL_PRODUCT_MODULE := true
LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/$(TARGET_ARCH)/res
LOCAL_SDK_VERSION := current
include $(BUILD_RRO_PACKAGE)
