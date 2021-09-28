LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	abc.c \
	misc.c \
	mail.c \
	hotplug.c

LOCAL_MODULE := abc
LOCAL_PROPRIETARY_MODULE := true
LOCAL_STATIC_LIBRARIES := libcutils 
LOCAL_SHARED_LIBRARIES := liblog libselinux
 # libext2_blkid
LOCAL_CFLAGS += -Wno-error \
    -Wunused-variable \
    -Wmacro-redefined

include $(BUILD_EXECUTABLE)
