#
# Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
#
# Modification based on code covered by the Apache License, Version 2.0 (the "License").
# You may not use this software except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
# AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
# IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
# NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.
#
# IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

BOARD_USES_DRM_HWCOMPOSER2=false
BOARD_USES_DRM_HWCOMPOSER=false
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk356x)
BOARD_USES_DRM_HWCOMPOSER2=true
else
BOARD_USES_DRM_HWCOMPOSER=true
endif

ifeq ($(strip $(BOARD_USES_DRM_HWCOMPOSER)),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libdrm \
	libEGL \
	libGLESv2 \
	libhardware \
	liblog \
	libui \
	libutils \
	librga

LOCAL_STATIC_LIBRARIES := \
	libtinyxml2

LOCAL_C_INCLUDES := \
	external/tinyxml2 \
	external/libdrm \
	external/libdrm/include/drm \
	system/core/include/utils \
	hardware/rockchip/librga

LOCAL_SRC_FILES := \
	autolock.cpp \
	drmresources.cpp \
	drmcomposition.cpp \
	drmcompositor.cpp \
	drmcompositorworker.cpp \
	drmconnector.cpp \
	drmcrtc.cpp \
	drmdisplaycomposition.cpp \
	drmdisplaycompositor.cpp \
	drmencoder.cpp \
	drmeventlistener.cpp \
	drmmode.cpp \
	drmplane.cpp \
	drmproperty.cpp \
	glworker.cpp \
	hwcomposer.cpp \
	platform.cpp \
	platformdrmgeneric.cpp \
	platformnv.cpp \
	separate_rects.cpp \
	virtualcompositorworker.cpp \
	vsyncworker.cpp \
	worker.cpp \
	hwc_util.cpp \
	hwc_rockchip.cpp \
	hwc_debug.cpp

# API 30 -> Android 11.0
ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 30)))
LOCAL_CFLAGS += -DANDROID_R
endif

# API 29 -> Android 10.0
ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 29)))

#DRM driver version is 2.0,kernel version is 4.19
LOCAL_CFLAGS += -DDRM_DRIVER_VERSION=2 -DUSE_NO_ASPECT_RATIO=1

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-tDVx)
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

ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3368)
LOCAL_C_INCLUDES += \
    system/core/libion/original-kernel-headers
endif

else
LOCAL_C_INCLUDES += \
       hardware/rockchip/libgralloc

endif

# API 28 -> Android 9.0
ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 28)))

LOCAL_CFLAGS += -DANDROID_P

LOCAL_SHARED_LIBRARIES +=  \
	libsync_vendor

LOCAL_C_INCLUDES += \
	system/core \
	system/core/liblog/include \
	frameworks/native/include
else #Android 9.0

LOCAL_SHARED_LIBRARIES +=  \
	libsync

LOCAL_C_INCLUDES += \
	system/core/libsync \
	system/core/libsync/include

endif

ifeq ($(strip $(BOARD_DRM_HWCOMPOSER_BUFFER_IMPORTER)),nvidia-gralloc)
LOCAL_CPPFLAGS += -DUSE_NVIDIA_IMPORTER
else

# Disable afbc by default.
USE_AFBC_LAYER = 0
# RGA1: 0  RGA1_plus: 1  RGA2-Lite: 2  RGA2: 3  RGA2-Enhance: 4
RGA_VER = 3
RK_RGA_SCALE_AND_ROTATE = 1

RK_3D_VIDEO = 1

# When enter rotate video,we need improve cpu freq in some platforms.
RK_ROTATE_VIDEO_MODE = 0

# In performance mode,we get handle's parameters from gralloc_drm_handle_t instead of gralloc's perform.
# it will lead reduce compatibility. So we disable it by default.
RK_PER_MODE = 0
ifeq ($(strip $(RK_PER_MODE)), 1)
RK_PRINT_LAYER_NAME = 0
else
RK_PRINT_LAYER_NAME = 1
endif

RK_CTS_WORKROUND = 0

# vop Multi-Zone sort
RK_SORT_AREA_BY_XPOS = 1
# vop Multi-Zone can intersect in horizontal line.
RK_HOR_INTERSECT_LIMIT = 0

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-t720)
USE_AFBC_LAYER = 0
LOCAL_CPPFLAGS += -DMALI_PRODUCT_ID_T72X=1
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-t760)
# rk3288 vop cann't support AFBC.
ifneq ($(strip $(TARGET_BOARD_PLATFORM)),rk3288)
USE_AFBC_LAYER = 1
endif
LOCAL_CPPFLAGS += -DMALI_PRODUCT_ID_T76X=1
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-t860)
USE_AFBC_LAYER = 1
LOCAL_CPPFLAGS += -DMALI_PRODUCT_ID_T86X=1
endif

#ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk322x)
ifneq ($(filter rk3128h rk322x, $(strip $(TARGET_BOARD_PLATFORM))), )
RGA_VER = 2
LOCAL_CPPFLAGS += -DTARGET_BOARD_PLATFORM_RK322x  -DRK_DRM_GRALLOC=1 \
               -DMALI_AFBC_GRALLOC=1
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),tablet)
LOCAL_CPPFLAGS += -DRK322x_MID
endif
ifneq ($(filter box atv, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
LOCAL_CPPFLAGS += -DRK322x_BOX
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),phone)
LOCAL_CPPFLAGS += -DRK322x_PHONE
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),vr)
LOCAL_CPPFLAGS += -DRK322x_VR
endif
endif

ifeq ($(strip $(TARGET_PRODUCT)),iot_rk3229_evb)
RGA_VER = 2
RK_3D_VIDEO = 0
RK_PRINT_LAYER_NAME = 0
LOCAL_CPPFLAGS += -DTARGET_PRODUCT_IOT_RK3229_EVB
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3368)
RK_RGA_SCALE_AND_ROTATE = 0
RGA_VER = 2
LOCAL_CPPFLAGS += -DTARGET_BOARD_PLATFORM_RK3368
#USE_AFBC_LAYER = 1
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),tablet)
LOCAL_CPPFLAGS += -DRK3368_MID
endif
ifneq ($(filter box atv, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
LOCAL_CPPFLAGS += -DRK3368_BOX
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),phone)
LOCAL_CPPFLAGS += -DRK3368_PHONE
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),vr)
LOCAL_CPPFLAGS += -DRK3368_VR
endif
ifeq ($(strip $(PRODUCT_BUILD_MODULE)),px5car)
LOCAL_CPPFLAGS += -DUSE_PLANE_RESERVED
endif
endif


# 3399, 3399pro
ifneq ($(filter rk3399 rk3399pro, $(strip $(TARGET_BOARD_PLATFORM))), )
USE_AFBC_LAYER = 1
RGA_VER = 4
LOCAL_CPPFLAGS += -DTARGET_BOARD_PLATFORM_RK3399 \
               -DMALI_AFBC_GRALLOC=1
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),tablet)
LOCAL_CPPFLAGS += -DRK3399_MID
endif
ifneq ($(filter box atv, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
LOCAL_CPPFLAGS += -DRK3399_BOX
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),phone)
LOCAL_CPPFLAGS += -DRK3399_PHONE
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),vr)
LOCAL_CPPFLAGS += -DRK3399_VR
endif

# Gralloc 4.0
ifeq ($(TARGET_RK_GRALLOC_VERSION),4)
LOCAL_CFLAGS += -DUSE_GRALLOC_4=1
LOCAL_C_INCLUDES += \
        hardware/rockchip/libgralloc/midgard/src \
        hardware/libhardware/include
LOCAL_SRC_FILES += \
        drmgralloc4.cpp
LOCAL_SHARED_LIBRARIES += \
        libhidlbase \
        libgralloctypes \
        android.hardware.graphics.mapper@4.0
LOCAL_HEADER_LIBRARIES += \
        libgralloc_headers
else
LOCAL_CPPFLAGS += -DRK_DRM_GRALLOC=1
endif

endif


# 3366
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3366)
RGA_VER = 2
LOCAL_CPPFLAGS += -DTARGET_BOARD_PLATFORM_RK3366 -DRK_DRM_GRALLOC=1 \
               -DMALI_AFBC_GRALLOC=1
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),tablet)
LOCAL_CPPFLAGS += -DRK3366_MID
endif
ifneq ($(filter box atv, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
LOCAL_CPPFLAGS += -DRK3366_BOX
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),phone)
LOCAL_CPPFLAGS += -DRK3366_PHONE
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),vr)
LOCAL_CPPFLAGS += -DRK3366_VR
endif
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3288)
RGA_VER = 3
RK_CTS_WORKROUND = 0
LOCAL_CPPFLAGS += -DTARGET_BOARD_PLATFORM_RK3288 \
               -DMALI_AFBC_GRALLOC=1
LOCAL_CPPFLAGS += -DRK_MULTI_AREAS_FORMAT_LIMIT
RK_SORT_AREA_BY_XPOS = 0
RK_HOR_INTERSECT_LIMIT = 1
RK_ROTATE_VIDEO_MODE = 1
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),tablet)
LOCAL_CPPFLAGS += -DRK3288_MID
endif
ifneq ($(filter box atv, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
LOCAL_CPPFLAGS += -DRK3288_BOX
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),phone)
LOCAL_CPPFLAGS += -DRK3288_PHONE
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),vr)
LOCAL_CPPFLAGS += -DRK3288_VR
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),stbvr)
LOCAL_CPPFLAGS += -DRK3288_VR
endif

# Gralloc 4.0
ifeq ($(TARGET_RK_GRALLOC_VERSION),4)
LOCAL_CFLAGS += -DUSE_GRALLOC_4=1
LOCAL_C_INCLUDES += \
        hardware/rockchip/libgralloc/midgard/src \
        hardware/libhardware/include
LOCAL_SRC_FILES += \
        drmgralloc4.cpp
LOCAL_SHARED_LIBRARIES += \
        libhidlbase \
        libgralloctypes \
        android.hardware.graphics.mapper@4.0
LOCAL_HEADER_LIBRARIES += \
        libgralloc_headers
else
+LOCAL_CPPFLAGS += -DRK_DRM_GRALLOC=1
endif
endif


ifneq ($(filter rk3328 rk3228h, $(TARGET_BOARD_PLATFORM)),)
RK_VER = 2
LOCAL_CPPFLAGS += -DTARGET_BOARD_PLATFORM_RK3328  -DRK_DRM_GRALLOC=1 \
               -DMALI_AFBC_GRALLOC=1
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),tablet)
LOCAL_CPPFLAGS += -DRK3328_MID
endif
ifneq ($(filter box atv, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
LOCAL_CPPFLAGS += -DRK3328_BOX
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),phone)
LOCAL_CPPFLAGS += -DRK3328_PHONE
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),vr)
LOCAL_CPPFLAGS += -DRK3328_VR
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),stbvr)
LOCAL_CPPFLAGS += -DRK3328_VR
endif
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3126c)
# disable scale when using rga1/rga1_plus.
RK_RGA_SCALE_AND_ROTATE = 0
RGA_VER = 1
RK_CTS_WORKROUND = 1
LOCAL_CPPFLAGS += -DTARGET_BOARD_PLATFORM_RK3126C  -DRK_DRM_GRALLOC=1 \
               -DMALI_AFBC_GRALLOC=1
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),tablet)
LOCAL_CPPFLAGS += -DRK3126C_MID
endif
ifneq ($(filter box atv, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
LOCAL_CPPFLAGS += -DRK3126C_BOX
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),phone)
LOCAL_CPPFLAGS += -DRK3126C_PHONE
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),vr)
LOCAL_CPPFLAGS += -DRK3126C_VR
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),stbvr)
LOCAL_CPPFLAGS += -DRK3126C_VR
endif
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3326)
USE_AFBC_LAYER = 1
RGA_VER = 2
LOCAL_CPPFLAGS += -DTARGET_BOARD_PLATFORM_RK3326 \
               -DMALI_AFBC_GRALLOC=1
LOCAL_CPPFLAGS += -DRK_MULTI_AREAS_FORMAT_LIMIT
RK_SORT_AREA_BY_XPOS = 0
RK_HOR_INTERSECT_LIMIT = 1

# Gralloc 4.0
ifeq ($(TARGET_RK_GRALLOC_VERSION),4)
LOCAL_CFLAGS += -DUSE_GRALLOC_4=1

LOCAL_C_INCLUDES += \
        hardware/rockchip/libgralloc/bifrost/src \
        hardware/libhardware/include

LOCAL_SRC_FILES += \
        drmgralloc4.cpp

LOCAL_SHARED_LIBRARIES += \
        libhidlbase \
        libgralloctypes \
        android.hardware.graphics.mapper@4.0

LOCAL_HEADER_LIBRARIES += \
        libgralloc_headers
else
LOCAL_CPPFLAGS += -DRK_DRM_GRALLOC=1
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),tablet)
LOCAL_CPPFLAGS += -DRK3326_MID
endif
ifneq ($(filter box atv, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
LOCAL_CPPFLAGS += -DRK3326_BOX
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),phone)
LOCAL_CPPFLAGS += -DRK3326_PHONE
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),vr)
LOCAL_CPPFLAGS += -DRK3326_VR
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),stbvr)
LOCAL_CPPFLAGS += -DRK3326_VR
endif
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),tablet)
LOCAL_CFLAGS += -DRK_MID
else
ifneq ($(filter box atv, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
LOCAL_CFLAGS += -DRK_BOX
# Disable afbc in box platform.
USE_AFBC_LAYER = 0
else
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),phone)
LOCAL_CFLAGS += -DRK_PHONE
else
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),vr)
LOCAL_CFLAGS += -DRK_VIR
else
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)),stbvr)
LOCAL_CFLAGS += -DRK_VIR
endif #stbvr
endif #vr
endif #phone
endif #box
endif #tablet

ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3126c)
RK_INVALID_REFRESH = 0
else
RK_INVALID_REFRESH = 1
endif

ifeq ($(TARGET_USES_HWC2),true)
    LOCAL_CFLAGS += -DUSE_HWC2
endif

# DUAL_VIEW_MODE enable
ifeq ($(strip $(BOARD_MULTISCREEN_SPLICING)),true)
LOCAL_CPPFLAGS += -DDUAL_VIEW_MODE=1
else
LOCAL_CPPFLAGS += -DDUAL_VIEW_MODE=0
endif

#USE_PLANE_RESERVED enable
#LOCAL_CPPFLAGS += -DUSE_PLANE_RESERVED

# RK_RGA_PREPARE_ASYNC and RK_RGA_COMPSITE_SYNC are exclusive.
# 	RK_RGA_PREPARE_ASYNC: use async rga in hwc_prepare.
#	RK_RGA_COMPSITE_SYNC: use sync rga in composite thread.
LOCAL_CPPFLAGS += -DUSE_SQUASH=0 -DRK_RGA_TEST=0 -DRK_VR=0 -DRK_STEREO=0 -DUSE_GL_WORKER=0
LOCAL_CPPFLAGS += -DUSE_DRM_GENERIC_IMPORTER \
               -DUSE_MULTI_AREAS=1 -DRK_RGA_PREPARE_ASYNC=1 -DRK_RGA_COMPSITE_SYNC=0 \
	       -DUSE_AFBC_LAYER=$(USE_AFBC_LAYER) -DRK_SKIP_SUB=1 \
	       -DRK_VIDEO_UI_OPT=1 -DRK_VIDEO_SKIP_LINE=1 \
	       -DRK_INVALID_REFRESH=$(RK_INVALID_REFRESH) -DRK_HDR_PERF_MODE=0 \
               -DRK_3D_VIDEO=$(RK_3D_VIDEO) -DRK_PRINT_LAYER_NAME=$(RK_PRINT_LAYER_NAME) \
	       -DRK_SORT_AREA_BY_XPOS=$(RK_SORT_AREA_BY_XPOS) -DRK_HOR_INTERSECT_LIMIT=$(RK_HOR_INTERSECT_LIMIT) \
	       -DRK_RGA_SCALE_AND_ROTATE=$(RK_RGA_SCALE_AND_ROTATE) \
               -DRGA_VER=$(RGA_VER) -DRK_PER_MODE=$(RK_PER_MODE) -DRK_ROTATE_VIDEO_MODE=$(RK_ROTATE_VIDEO_MODE) \
               -DRK_CTS_WORKROUND=$(RK_CTS_WORKROUND)
MAJOR_VERSION := "RK_GRAPHICS_VER=commit-id:$(shell cd $(LOCAL_PATH) && git log  -1 --oneline | awk '{print $$1}')"
LOCAL_CPPFLAGS += -DRK_GRAPHICS_VER=\"$(MAJOR_VERSION)\"
endif

ifeq ($(strip $(TARGET_PRODUCT)),iot_rk3229_evb)
LOCAL_MODULE := hwcomposer.rk3229
else
LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_HARDWARE)
endif

# API 26 -> Android 8.0
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 26)))
LOCAL_PROPRIETARY_MODULE := true
endif
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -Wno-unused-function -Wno-unused-parameter
LOCAL_CFLAGS += -Wno-unused-variable
LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := $(TARGET_SHLIB_SUFFIX)
include $(BUILD_SHARED_LIBRARY)

endif
