###############################################################################
# copybit
ifeq ($(strip $(TARGET_ARCH)), arm)

include $(CLEAR_VARS)
LOCAL_MODULE := copybit.$(TARGET_BOARD_HARDWARE).so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_SRC_FILES := lib/$(TARGET_ARCH)/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

endif
