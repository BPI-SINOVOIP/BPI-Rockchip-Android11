LOCAL_PATH := $(call my-dir)

#########################
# Build the mkntfs binary

include $(CLEAR_VARS)
LOCAL_SRC_FILES :=  \
	attrdef.c \
	boot.c \
	sd.c \
	mkntfs.c \
	utils.c 

LOCAL_MODULE := mkntfs
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -Wno-error
LOCAL_SHARED_LIBRARIES := libntfs-3g

LOCAL_C_INCLUDES :=  \
	$(LOCAL_PATH)/../include/ntfs-3g \
	$(LOCAL_PATH)/..

LOCAL_CFLAGS := -O2 -W -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DHAVE_CONFIG_H -D__USE_FILE_OFFSET64

include $(BUILD_EXECUTABLE)

#########################
# Build the ntfsfix binary

include $(CLEAR_VARS)
LOCAL_SRC_FILES :=  \
	ntfsfix.c \
	utils.c

LOCAL_MODULE := ntfsfix
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := libntfs-3g

LOCAL_C_INCLUDES :=  \
	$(LOCAL_PATH)/../include/ntfs-3g \
	$(LOCAL_PATH)/..
LOCAL_CFLAGS += -Wno-error
LOCAL_CFLAGS := -O2 -g -W -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DHAVE_CONFIG_H -D__USE_FILE_OFFSET64

include $(BUILD_EXECUTABLE)

