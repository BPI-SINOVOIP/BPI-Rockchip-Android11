LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

$(info $(shell ($(LOCAL_PATH)/genversion.sh)))

include $(CLEAR_VARS)

LOCAL_ARM_MODE := thumb
# used for testing
# LOCAL_CFLAGS += -g -O0

LOCAL_CPPFLAGS := \
	-std=c++11 \
	-fno-threadsafe-statics \

LOCAL_SRC_FILES := \
	src/Utils.cpp \
	src/QList.cpp \
	src/MpiDebug.cpp \
	src/BitReader.cpp \
	src/JpegParser.cpp \
	src/ExifBuilder.cpp \
	src/RKExifWrapper.cpp \
	src/MpiJpegEncoder.cpp \
	src/MpiJpegDecoder.cpp \

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libmpp \
	librga \

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/inc/mpp_inc/ \
	$(LOCAL_PATH)/inc \
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

include $(call all-makefiles-under,$(LOCAL_PATH))
