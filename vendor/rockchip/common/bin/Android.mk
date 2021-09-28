###############################################################################
# All static executable files
# We only have arm version now
###############################################################################
LOCAL_PATH := $(call my-dir)
#ifeq ($(strip $(TARGET_ARCH)), arm)

###############################################################################
ifneq (,$(filter user userdebug eng,$(TARGET_BUILD_VARIANT)))
# busybox
include $(CLEAR_VARS)
LOCAL_MODULE := busybox
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_SRC_FILES := $(TARGET_ARCH)/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)
endif

###############################################################################
# mkdosfs
include $(CLEAR_VARS)
LOCAL_MODULE := mkdosfs
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_SRC_FILES := $(TARGET_ARCH)/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

###############################################################################
# sdtool
include $(CLEAR_VARS)
LOCAL_MODULE := sdtool
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_SRC_FILES := $(TARGET_ARCH)/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

#endif
