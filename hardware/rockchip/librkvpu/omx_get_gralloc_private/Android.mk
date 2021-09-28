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

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    gralloc_priv_omx.cpp
LOCAL_SHARED_LIBRARIES := \
    liblog \
    libutils

LOCAL_MODULE := libgralloc_priv_omx
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS += -Wno-unused-parameter -Wno-unused-variable
LOCAL_HEADER_LIBRARIES += \
	liblog_headers

LOCAL_C_INCLUDES := \
	hardware/libhardware/include \
	system/core/liblog/include

ifeq ($(TARGET_RK_GRALLOC_VERSION),4)
LOCAL_SRC_FILES += \
    platform_gralloc4.cpp
LOCAL_SHARED_LIBRARIES += \
    libsync \
    libhidlbase \
    libgralloctypes \
    android.hardware.graphics.mapper@4.0
LOCAL_C_INCLUDES += \
	frameworks/native/include \
	system/core/libsync \
	system/core/libsync/include\
	external/libdrm/include/drm
LOCAL_CFLAGS += -DUSE_GRALLOC_4
else
ifeq ($(strip $(BOARD_USE_DRM)), true)
ifneq ($(filter rk3399 rk3366 rk3288 rk356x rk3128h rk322x rk3126c rk3328 rk3326 rk3399pro rk3228h, $(strip $(TARGET_BOARD_PLATFORM))), )
    LOCAL_CFLAGS += -DUSE_DRM -DRK_DRM_GRALLOC=1 -DMALI_AFBC_GRALLOC=1
ifeq ($(TARGET_USES_HWC2),true)
    LOCAL_CFLAGS += -DUSE_HWC2
endif
endif
endif
endif

# API 29 -> Android 10.0
ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 29)))

ifneq (,$(filter mali-tDVx mali-G52, $(TARGET_BOARD_PLATFORM_GPU)))
LOCAL_C_INCLUDES += \
	hardware/rockchip/libgralloc/bifrost
endif

ifneq (,$(filter mali-t860 mali-t760, $(TARGET_BOARD_PLATFORM_GPU)))
LOCAL_C_INCLUDES += \
	hardware/rockchip/libgralloc/midgard
endif

ifneq (,$(filter mali400 mali450, $(TARGET_BOARD_PLATFORM_GPU)))
LOCAL_C_INCLUDES += \
	hardware/rockchip/libgralloc/utgard
endif
else
LOCAL_C_INCLUDES += \
        hardware/rockchip/libgralloc

endif

ifeq ($(strip $(GRAPHIC_MEMORY_PROVIDER)),dma_buf)
	LOCAL_CFLAGS += -DUSE_DMA_BUF
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-t720)
LOCAL_CFLAGS += -DMALI_PRODUCT_ID_T72X=1
LOCAL_CFLAGS += -DMALI_AFBC_GRALLOC=0
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-t760)
LOCAL_CFLAGS += -DMALI_PRODUCT_ID_T76X=1
LOCAL_CFLAGS += -DMALI_AFBC_GRALLOC=1
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-t860)
LOCAL_CFLAGS += -DMALI_PRODUCT_ID_T86X=1
LOCAL_CFLAGS += -DMALI_AFBC_GRALLOC=1
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)),G6110)
LOCAL_CFLAGS += -DGPU_G6110
LOCAL_C_INCLUDES += \
    system/memory/libion/original-kernel-headers
endif


include $(BUILD_SHARED_LIBRARY)
