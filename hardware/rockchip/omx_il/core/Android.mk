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

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
	Rockchip_OMX_Component_Register.c \
	Rockchip_OMX_Core.c

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := libOMX_Core
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CFLAGS += -Werror

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := libRkOMX_OSAL libRkOMX_Basecomponent
LOCAL_SHARED_LIBRARIES := libc libdl libcutils libutils liblog \
	libRkOMX_Resourcemanager

LOCAL_C_INCLUDES := $(ROCKCHIP_OMX_INC)/khronos \
	$(ROCKCHIP_OMX_INC)/rockchip \
	$(ROCKCHIP_OMX_TOP)/osal \
	$(ROCKCHIP_OMX_TOP)/component/common \
	$(TOP)/hardware/rk29/librkvpu \

include $(BUILD_SHARED_LIBRARY)
