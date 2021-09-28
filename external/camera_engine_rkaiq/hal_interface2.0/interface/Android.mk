#--------------------------------------------------------------------------
#  Copyright (C) 2020 Rockchip Electronics Co. Ltd. All rights reserved.

#Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions are met:
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#    * Neither the name of The Linux Foundation nor
#      the names of its contributors may be used to endorse or promote
#      products derived from this software without specific prior written
#      permission.

#THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
#NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
#CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
#OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
#WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
#OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
#ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#--------------------------------------------------------------------------

LOCAL_PATH:= $(call my-dir)
MY_LOCAL_PATH =${LOCAL_PATH}
include $(CLEAR_VARS)

LOCAL_SRC_FILES +=\
	rkisp_control_loop_impl.cpp \
	rkcamera_vendor_tags.cpp \
	rkaiq.cpp \
	AiqCameraHalAdapter.cpp \
	CameraWindow.cpp \
	settings_processor.cpp \
	Metadata2Str.cpp

#states
LOCAL_SRC_FILES += \
	ae_state_machine.cpp \
	af_state_machine.cpp \
	awb_state_machine.cpp

#message
LOCAL_SRC_FILES += \
	common/MessageThread.cpp


#LOCAL_CFLAGS += -DLINUX  -D_FILE_OFFSET_BITS=64 -DHAS_STDINT_H -DENABLE_ASSERT
LOCAL_CPPFLAGS += -std=c++11 -Wno-error -frtti
LOCAL_CPPFLAGS += -DLINUX
LOCAL_CPPFLAGS += $(PRJ_CPPFLAGS)
LOCAL_CPPFLAGS += -std=c++1y

#Namespace Declaration
LOCAL_CPPFLAGS += -DNAMESPACE_DECLARATION=namespace\ android\ {\namespace\ camera2
LOCAL_CPPFLAGS += -DNAMESPACE_DECLARATION_END=}
LOCAL_CPPFLAGS += -DUSING_DECLARED_NAMESPACE=using\ namespace\ android::camera2

ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 29)))
LOCAL_SHARED_LIBRARIES += libutils libcutils liblog
LOCAL_SHARED_LIBRARIES += \
	libcamera_metadata

ifeq ($(IS_HAVE_DRM),true)
LOCAL_SHARED_LIBRARIES += \
	libdrm
endif

LOCAL_SHARED_LIBRARIES += \
	librkaiq

#external/camera_engine_rkaiq
LOCAL_C_INCLUDES += \
	external/camera_engine_rkaiq \
	external/camera_engine_rkaiq/include/uAPI \
	external/camera_engine_rkaiq/include/uAPI \
	external/camera_engine_rkaiq/xcore \
	external/camera_engine_rkaiq/xcore/base \
	external/camera_engine_rkaiq/aiq_core \
	external/camera_engine_rkaiq/algos \
	external/camera_engine_rkaiq/hwi \
	external/camera_engine_rkaiq/iq_parser \
	external/camera_engine_rkaiq/uAPI \
	external/camera_engine_rkaiq/uAPI2 \
	external/camera_engine_rkaiq/common \
	external/camera_engine_rkaiq/include \
	external/camera_engine_rkaiq/include/iq_parser \
	external/camera_engine_rkaiq/include/uAPI \
	external/camera_engine_rkaiq/include/xcore \
	external/camera_engine_rkaiq/include/common \
	external/camera_engine_rkaiq/include/common/mediactl \
	external/camera_engine_rkaiq/include/xcore/base \
	external/camera_engine_rkaiq/include/algos \

#system and frameworks
LOCAL_C_INCLUDES += \
	system/media/camera/include \
	system/media/private/camera/include \
	system/core/libutils/include \
	system/core/include \
	frameworks/native/libs/binder/include \
	frameworks/av/include

#local
LOCAL_C_INCLUDES += \
	$(MY_LOCAL_PATH)/include/ \
	$(MY_LOCAL_PATH)/include_aiq/ \
	$(MY_LOCAL_PATH)/xcore/ \
	$(MY_LOCAL_PATH)/common/

LOCAL_PROPRIETARY_MODULE := true
LOCAL_STATIC_LIBRARIES += android.hardware.camera.common@1.0-helper

LOCAL_CFLAGS += -DANDROID_VERSION_ABOVE_8_X
LOCAL_CFLAGS += -DANDROID_PLATEFORM
LOCAL_CFLAGS += -DANDROID_OS

ifeq (rk356x, $(strip $(TARGET_BOARD_PLATFORM)))
LOCAL_CFLAGS += -DISP_HW_V21
endif

ifeq (rv1126, $(strip $(TARGET_BOARD_PLATFORM)))
LOCAL_CFLAGS += -DISP_HW_V20
endif

LOCAL_HEADER_LIBRARIES += \
	libhardware_headers \
	libbinder_headers \
	gl_headers \
	libutils_headers
endif

LOCAL_MODULE:= librkisp
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include_aiq

include $(BUILD_SHARED_LIBRARY)
