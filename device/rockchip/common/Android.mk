LOCAL_PATH := $(call my-dir)

$(shell mkdir -p $(TARGET_ROOT_OUT)/mnt/vendor/metadata)

include $(call all-makefiles-under,$(LOCAL_PATH))
