#--------------------------------------------------------------------------
#  Copyright (C) 2018 Fuzhou Rockchip Electronics Co. Ltd. All rights reserved.

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

include $(CLEAR_VARS)
LOCAL_MODULE := librkisp
ifeq (1,$(strip $(shell expr $(PLATFORM_VERSION) \>= 8.0)))
    LOCAL_PROPRIETARY_MODULE := true
endif
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_MODULE_SUFFIX := .so
ifneq ($(strip $(TARGET_2ND_ARCH)), )
LOCAL_MULTILIB := both
LOCAL_SRC_FILES_$(TARGET_ARCH) := lib/$(TARGET_ARCH)/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := lib/$(TARGET_2ND_ARCH)/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
else
LOCAL_SRC_FILES := lib/$(TARGET_ARCH)/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_RELATIVE_PATH := rkisp/ae
LOCAL_MODULE := librkisp_ae
ifeq (1,$(strip $(shell expr $(PLATFORM_VERSION) \>= 8.0)))
     LOCAL_PROPRIETARY_MODULE := true
endif
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_MODULE_SUFFIX := .so
ifneq ($(strip $(TARGET_2ND_ARCH)), )
LOCAL_MULTILIB := both
LOCAL_SRC_FILES_$(TARGET_ARCH) := lib/$(TARGET_ARCH)/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := lib/$(TARGET_2ND_ARCH)/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
else
LOCAL_SRC_FILES := lib/$(TARGET_ARCH)/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_RELATIVE_PATH := rkisp/awb
LOCAL_MODULE := librkisp_awb
ifeq (1,$(strip $(shell expr $(PLATFORM_VERSION) \>= 8.0)))
    LOCAL_PROPRIETARY_MODULE := true
endif
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_MODULE_SUFFIX := .so
ifneq ($(strip $(TARGET_2ND_ARCH)), )
LOCAL_MULTILIB := both
LOCAL_SRC_FILES_$(TARGET_ARCH) := lib/$(TARGET_ARCH)/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := lib/$(TARGET_2ND_ARCH)/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
else
LOCAL_SRC_FILES := lib/$(TARGET_ARCH)/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_RELATIVE_PATH := rkisp/af
LOCAL_MODULE := librkisp_af
ifeq (1,$(strip $(shell expr $(PLATFORM_VERSION) \>= 8.0)))
    LOCAL_PROPRIETARY_MODULE := true
endif
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_MODULE_SUFFIX := .so
ifneq ($(strip $(TARGET_2ND_ARCH)), )
LOCAL_MULTILIB := both
LOCAL_SRC_FILES_$(TARGET_ARCH) := lib/$(TARGET_ARCH)/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := lib/$(TARGET_2ND_ARCH)/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
else
LOCAL_SRC_FILES := lib/$(TARGET_ARCH)/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
endif
include $(BUILD_PREBUILT)
