###############################################################################
# GmsConfigOverlayZeroTouch
LOCAL_PATH:= $(my-dir)

include $(CLEAR_VARS)
LOCAL_PACKAGE_NAME := GmsConfigOverlayZeroTouch
LOCAL_PRODUCT_MODULE := true
LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/res
LOCAL_SDK_VERSION := current
include $(BUILD_RRO_PACKAGE)
