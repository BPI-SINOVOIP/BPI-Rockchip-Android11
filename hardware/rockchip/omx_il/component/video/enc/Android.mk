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

LOCAL_SRC_FILES := \
	Rkvpu_OMX_VencControl.c \
	Rkvpu_OMX_Venc.c \
	library_register.c

LOCAL_MODULE := libomxvpu_enc
LOCAL_ARM_MODE := arm
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

LOCAL_C_INCLUDES := $(ROCKCHIP_OMX_INC)/khronos \
	$(ROCKCHIP_OMX_INC)/rockchip \
	$(ROCKCHIP_OMX_TOP)/osal \
	$(ROCKCHIP_OMX_TOP)/core \
	$(ROCKCHIP_OMX_COMPONENT)/common \
	$(ROCKCHIP_OMX_COMPONENT)/video/enc \
	$(TOP)/hardware/rockchip/librkvpu \
	$(TOP)/hardware/rockchip/librkvpu/omx_get_gralloc_private	

LOCAL_STATIC_LIBRARIES := libRkOMX_OSAL \
	  	libRkOMX_Basecomponent 
	
LOCAL_SHARED_LIBRARIES := libc \
	 libdl \
	 libcutils \
	 libutils \
	 liblog \
	 libui \
	 libRkOMX_Resourcemanager \
	 libhardware \
	 libvpu \
         libgralloc_priv_omx 
	
ifeq ($(BOARD_USE_ANB), true)
LOCAL_CFLAGS += -DUSE_ANB
endif

ifeq ($(BOARD_USE_STOREMETADATA), true)
LOCAL_CFLAGS += -DUSE_STOREMETADATA
endif

ifeq ($(BOARD_CONFIG_3GR),true)
LOCAL_CFLAGS += -DSOFIA_3GR \
    -DROCKCHIP_GPU_LIB_ENABLE
endif

ifeq ($(OMX_USE_DRM), true)
LOCAL_SHARED_LIBRARIES += librga
endif

ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 26)))
LOCAL_CFLAGS += -DAVS80
endif

ifneq ($(filter %true, $(BOARD_SUPPORT_HEVC_ENC)), )
LOCAL_CFLAGS += -DSUPPORT_HEVC_ENC=1
endif

ifneq ($(filter %true, $(BOARD_SUPPORT_VP8_ENC)), )
LOCAL_CFLAGS += -DSUPPORT_VP8_ENC=1
endif

LOCAL_CFLAGS += -Werror

include $(BUILD_SHARED_LIBRARY)
