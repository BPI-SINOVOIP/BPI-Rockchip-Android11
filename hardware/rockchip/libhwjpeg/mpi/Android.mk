LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_ARM_MODE := thumb
# used for testing
# LOCAL_CFLAGS += -g -O0

LOCAL_CFLAGS += \
	-fPIC \
	-Wno-unused-parameter \
	-U_FORTIFY_SOURCE \
	-D_FORTIFY_SOURCE=1 \
	-DSKIA_IMPLEMENTATION=1 \
	-Wno-clobbered \
	-fexceptions \

LOCAL_CPPFLAGS := \
	-std=c++11 \
	-fno-threadsafe-statics \

LOCAL_SRC_FILES := \
	Utils.cpp \
	QList.cpp \
	MpiDebug.cpp \
	BitReader.cpp \
	JpegParser.cpp \
	ExifBuilder.cpp \
	RKEncoderWraper.cpp \
	MpiJpegEncoder.cpp \
	MpiJpegDecoder.cpp \

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libmpp \
	librga \

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../inc/mpp_inc/ \
	$(LOCAL_PATH)/../inc \
	$(TOP)/hardware/rockchip/librga \

ifeq (1, $(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 29)))
LOCAL_SHARED_LIBRARIES += \
	libutils \

LOCAL_C_INCLUDES += \
	$(TOP)/system/core/libutils/include	\
	$(TOP)/hardware/libhardware/include \
else
endif

LOCAL_MULTILIB := 32
LOCAL_MODULE := libhwjpeg_bak
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
