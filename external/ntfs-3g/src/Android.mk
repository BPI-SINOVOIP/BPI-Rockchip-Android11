LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	ntfs-3g_common.c \
	ntfs-3g.c
 
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/.. \
	$(LOCAL_PATH)/../src \
	$(LOCAL_PATH)/../include/fuse-lite \
	$(LOCAL_PATH)/../include/ntfs-3g
 
LOCAL_CFLAGS := -O2 -g -W -Wall \
	-D_LARGEFILE_SOURCE \
	-D_FILE_OFFSET_BITS=64 \
	-DHAVE_CONFIG_H \
	-D__USE_FILE_OFFSET64

LOCAL_CFLAGS += -Wno-error 
LOCAL_MODULE := ntfs-3g
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := libc libutils libntfs-3g
LOCAL_STATIC_LIBRARIES := libfuse-lite 
 
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	ntfs-3g_common.c \
	lowntfs-3g.c
 
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/.. \
	$(LOCAL_PATH)/../src \
	$(LOCAL_PATH)/../include/fuse-lite \
	$(LOCAL_PATH)/../include/ntfs-3g
 
LOCAL_CFLAGS := -O2 -g -W -Wall \
	-D_LARGEFILE_SOURCE \
	-D_FILE_OFFSET_BITS=64 \
	-DHAVE_CONFIG_H \
	-D__USE_FILE_OFFSET64

LOCAL_CFLAGS += -Wno-error 
LOCAL_MODULE := lowntfs-3g
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := libc libutils libntfs-3g
LOCAL_STATIC_LIBRARIES := libfuse-lite 
 
#include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	usermap.c
 
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/.. \
	$(LOCAL_PATH)/../src \
	$(LOCAL_PATH)/../include/fuse-lite \
	$(LOCAL_PATH)/../include/ntfs-3g
 
LOCAL_CFLAGS := -O2 -g -W -Wall \
	-D_LARGEFILE_SOURCE \
	-D_FILE_OFFSET_BITS=64 \
	-DHAVE_CONFIG_H \
	-D__USE_FILE_OFFSET64
 
LOCAL_MODULE := usermap
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -Wno-error
LOCAL_SHARED_LIBRARIES := libc libutils libntfs-3g
LOCAL_STATIC_LIBRARIES := libfuse-lite 
 
#include $(BUILD_EXECUTABLE)


