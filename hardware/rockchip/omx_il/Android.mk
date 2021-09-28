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
BOARD_USE_ANB := true
BOARD_USE_STOREMETADATA := true

ifeq ($(filter sofia%, $(TARGET_BOARD_PLATFORM)), )
	BOARD_CONFIG_3GR := false
else
	BOARD_CONFIG_3GR := true
endif

ifeq ($(strip $(BOARD_USE_DRM)), true)
	OMX_USE_DRM := true
else
	OMX_USE_DRM := false
endif

include $(CLEAR_VARS)

$(info $(shell ($(LOCAL_PATH)/compile_setup.sh $(LOCAL_PATH))))

ROCKCHIP_OMX_TOP := $(LOCAL_PATH)

ROCKCHIP_OMX_INC := $(ROCKCHIP_OMX_TOP)/include/
ROCKCHIP_OMX_COMPONENT := $(ROCKCHIP_OMX_TOP)/component

include $(ROCKCHIP_OMX_TOP)/osal/Android.mk
include $(ROCKCHIP_OMX_TOP)/core/Android.mk

include $(ROCKCHIP_OMX_COMPONENT)/common/Android.mk
include $(ROCKCHIP_OMX_COMPONENT)/video/dec/Android.mk
include $(ROCKCHIP_OMX_COMPONENT)/video/enc/Android.mk


