LOCAL_PATH:= $(call my-dir)

#
# SECTION 1: build test for jpeg decoder
#

include $(CLEAR_VARS)

LOCAL_MODULE := jpegd_test_bak

LOCAL_SRC_FILES := \
	jpeg_dec_test.cpp \

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libhwjpeg_bak \

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../inc/mpp_inc/ \
	$(LOCAL_PATH)/../inc \

ifeq (1, $(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 29)))
LOCAL_C_INCLUDES += \
	$(TOP)/system/core/libutils/include \
else
endif

LOCAL_PROPRIETARY_MODULE := true

LOCAL_MULTILIB := 32

include $(BUILD_EXECUTABLE)

#
# SECTION 2: build test for jpeg encoder
#

include $(CLEAR_VARS)

LOCAL_MODULE := jpege_test_bak

LOCAL_SRC_FILES := \
	jpeg_enc_test.cpp \

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libhwjpeg_bak \

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../inc/mpp_inc/ \
	$(LOCAL_PATH)/../inc \

ifeq (1, $(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 29)))
LOCAL_C_INCLUDES += \
	$(TOP)/system/core/libutils/include \
else
endif

LOCAL_PROPRIETARY_MODULE := true

LOCAL_MULTILIB := 32

include $(BUILD_EXECUTABLE)
