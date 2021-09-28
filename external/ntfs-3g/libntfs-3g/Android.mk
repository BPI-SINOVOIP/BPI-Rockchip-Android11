LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	acls.c \
	attrib.c \
	attrlist.c \
	bitmap.c \
	bootsect.c \
	cache.c \
	collate.c \
	compat.c \
	compress.c \
	debug.c \
	device.c \
	dir.c \
	ea.c \
	efs.c \
	ioctl.c \
	index.c \
	inode.c \
	lcnalloc.c \
	logfile.c \
	logging.c \
	mft.c \
	misc.c \
	mst.c \
	object_id.c \
	reparse.c \
	runlist.c \
	realpath.c \
	security.c \
	unistr.c \
	volume.c \
	xattrs.c \
	unix_io.c

LOCAL_MODULE := libntfs-3g
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
	libc \
	libutils	\
	liblog 

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/.. \
	$(LOCAL_PATH)/../src \
	$(LOCAL_PATH)/../include/ntfs-3g

LOCAL_CFLAGS := -O2 -g -W -Wall \
	-DHAVE_CONFIG_H \
	-D__USE_FILE_OFFSET64
	
LOCAL_PRELINK_MODULE := false
LOCAL_CFLAGS += -Wno-error
include $(BUILD_SHARED_LIBRARY)

