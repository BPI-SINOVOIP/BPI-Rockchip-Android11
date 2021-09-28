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

LOCAL_SRC_FILES := \
	Rkvpu_OMX_VdecControl.c \
	Rkvpu_OMX_Vdec.c \
	library_register.c

LOCAL_MODULE := libomxvpu_dec
LOCAL_ARM_MODE := arm
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

LOCAL_C_INCLUDES := $(ROCKCHIP_OMX_INC)/khronos \
	$(ROCKCHIP_OMX_INC)/rockchip \
	$(ROCKCHIP_OMX_TOP)/osal \
	$(ROCKCHIP_OMX_TOP)/core \
	$(ROCKCHIP_OMX_COMPONENT)/common \
	$(ROCKCHIP_OMX_COMPONENT)/video/dec \
        $(TOP)/hardware/rockchip/librkvpu \
	$(TOP)/hardware/rockchip/librkvpu/common/include \
	$(TOP)/hardware/rockchip/librga \

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
	 libgralloc_priv_omx \
	 libvpu \
	 librga

ifeq ($(BOARD_WIDEVINE_OEMCRYPTO_LEVEL), 1)
LOCAL_CFLAGS += -DHAVE_L1_SVP_MODE=ON
endif

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
ifeq ($(BOARD_VERSION_LOW),true)
LOCAL_CFLAGS += -DLOW_VRESION
endif

ifeq ($(OMX_USE_DRM), true)
LOCAL_SHARED_LIBRARIES += librga
endif

ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 26)))
LOCAL_CFLAGS += -DAVS80
endif

ifeq ($(filter %false, $(BOARD_SUPPORT_HEVC)), )
LOCAL_CFLAGS += -DSUPPORT_HEVC=1
endif

ifeq ($(filter %false, $(BOARD_SUPPORT_VP6)), )
LOCAL_CFLAGS += -DSUPPORT_VP6=1
endif

ifeq ($(filter %false, $(BOARD_SUPPORT_VP9)), )
LOCAL_CFLAGS += -DSUPPORT_VP9=1
endif

LOCAL_CFLAGS += -Werror

include $(BUILD_SHARED_LIBRARY)
