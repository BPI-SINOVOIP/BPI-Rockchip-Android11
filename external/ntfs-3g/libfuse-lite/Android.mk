LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	fuse.c 			\
	fuse_kern_chan.c 	\
	fuse_loop.c 		\
	fuse_lowlevel.c 	\
	fuse_opt.c 		\
	fuse_session.c 		\
	fuse_signals.c 		\
	fusermount.c		\
	helper.c 		\
	mount.c 		\
	mount_util.c 		

LOCAL_MODULE := libfuse-lite
LOCAL_MODULE_TAGS := optional

LOCAL_SYSTEM_SHARED_LIBRARIES := \
	libc

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/.. \
	$(LOCAL_PATH)/../include/fuse-lite

LOCAL_CFLAGS := -O2 -g -W -Wall \
	-D_LARGEFILE_SOURCE \
	-D_FILE_OFFSET_BITS=64 \
	-DHAVE_CONFIG_H \
	-D__USE_FILE_OFFSET64
	
LOCAL_CFLAGS += -Wno-error
LOCAL_PRELINK_MODULE := false

include $(BUILD_STATIC_LIBRARY)

