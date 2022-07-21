#--------------------------------------------------------------------------
#  Copyright (C) 2014 Fuzhou Rockchip Electronics Co. Ltd. All rights reserved.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  #  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
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

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(PLATFORM_VERSION),4.4.4) 
BOARD_VERSION_LOW := true
endif

ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 26)))
LOCAL_CFLAGS += -DAVS80
endif

ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 29)))
LOCAL_CFLAGS += -DAVS100
endif

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
	Rockchip_OSAL_Event.c \
	Rockchip_OSAL_Queue.c \
	Rockchip_OSAL_ETC.c \
	Rockchip_OSAL_Mutex.c \
	Rockchip_OSAL_Thread.c \
	Rockchip_OSAL_Memory.c \
	Rockchip_OSAL_Semaphore.c \
	Rockchip_OSAL_Library.c \
	Rockchip_OSAL_Log.c\
	Rockchip_OSAL_RGA_Process.c \
	Rockchip_OSAL_Android.cpp \
	Rockchip_OSAL_SharedMemory.c \
	Rockchip_OSAL_Env.c \
        Rockchip_OSAL_ColorUtils.cpp

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := libRkOMX_OSAL
LOCAL_PROPRIETARY_MODULE := true

LOCAL_SHARED_LIBRARIES := libhardware liblog libvpu libui
LOCAL_STATIC_LIBRARIES := libutils libcutils

ifeq ($(BOARD_VERSION_LOW),true)
LOCAL_CFLAGS += -DLOW_VRESION
endif

ifeq ($(BOARD_CONFIG_3GR),true)
      LOCAL_CFLAGS += -DROCKCHIP_GPU_LIB_ENABLE
endif

ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 24)))
LOCAL_CFLAGS += -DUSE_ANW
endif

LOCAL_C_INCLUDES := $(ROCKCHIP_OMX_INC)/khronos \
	$(ROCKCHIP_OMX_INC)/rockchip \
	$(ROCKCHIP_OMX_INC)/linux \
	$(ROCKCHIP_OMX_TOP)/osal \
	$(ROCKCHIP_OMX_COMPONENT)/common \
	$(ROCKCHIP_OMX_COMPONENT)/video/dec \
	$(ROCKCHIP_OMX_COMPONENT)/video/enc \
	$(TOP)/frameworks/native/include/media/hardware \
	$(TOP)/frameworks/native/include/media/openmax \
	$(TOP)/frameworks/native/libs/arect/include \
	$(TOP)/frameworks/native/libs/nativebase/include \
	$(TOP)/system/core/base/include/ \
	$(TOP)/hardware/rockchip/librkvpu \
	$(TOP)/hardware/rockchip/librkvpu/omx_get_gralloc_private \
	$(TOP)/system/core/libcutils


ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 26)))
LOCAL_C_INCLUDES += \
	$(TOP)/frameworks/native/headers/media_plugin
endif

ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 30)))
LOCAL_C_INCLUDES += \
    $(TOP)/system/memory/libion/kernel-headers \
    $(TOP)/system/memory/libion/include
endif

ifeq ($(OMX_USE_DRM), true)
	LOCAL_CFLAGS += -DUSE_DRM
	LOCAL_C_INCLUDES +=  $(TOP)/hardware/rockchip/librga/include \
			     $(TOP)/external/libdrm/include/drm/
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM)), rk356x)
	LOCAL_CFLAGS += -DSUPPORT_AFBC
endif

LOCAL_CFLAGS += -Werror

include $(BUILD_STATIC_LIBRARY)
