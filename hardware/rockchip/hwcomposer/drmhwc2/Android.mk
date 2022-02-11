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

LOCAL_PATH := $(call my-dir)

BOARD_USES_DRM_HWCOMPOSER2=false
BOARD_USES_DRM_HWCOMPOSER=false
# API 31 -> Android 12.0
ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 31)))
# RK356x RK3588 use DrmHwc2
ifneq ($(filter rk356x rk3588, $(strip $(TARGET_BOARD_PLATFORM))), )
ifeq ($(strip $(BUILD_WITH_RK_EBOOK)),true)
        BOARD_USES_DRM_HWCOMPOSER2=false
else  # BUILD_WITH_RK_EBOOK
        BOARD_USES_DRM_HWCOMPOSER2=true
endif # BUILD_WITH_RK_EBOOK
else
        BOARD_USES_DRM_HWCOMPOSER2=false
endif
endif

# API 30 -> Android 11.0
ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 30)))
ifneq ($(filter rk356x rk3588, $(strip $(TARGET_BOARD_PLATFORM))), )
ifeq ($(strip $(BUILD_WITH_RK_EBOOK)),true)
        BOARD_USES_DRM_HWCOMPOSER2=false
else  # BUILD_WITH_RK_EBOOK
        BOARD_USES_DRM_HWCOMPOSER2=true
endif # BUILD_WITH_RK_EBOOK
else
        BOARD_USES_DRM_HWCOMPOSER2=false
endif
endif

ifeq ($(strip $(BOARD_USES_DRM_HWCOMPOSER2)),true)

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libdrm \
	libhardware \
	liblog \
	libui \
	libutils \
	libsync_vendor \
        libtinyxml2 \
        libbaseparameter

LOCAL_STATIC_LIBRARIES := \
  libdrmhwcutils

LOCAL_C_INCLUDES := \
  external/libdrm \
  external/libdrm/include/drm \
  system/core \
  system/core/libsync/include \
  external/tinyxml2 \
  hardware/rockchip/libbaseparameter

LOCAL_SRC_FILES := \
  drmhwctwo.cpp \
  compositor/drmdisplaycomposition.cpp \
  compositor/drmdisplaycompositor.cpp \
  drm/drmconnector.cpp \
  drm/drmcrtc.cpp \
  drm/drmdevice.cpp \
  drm/drmencoder.cpp \
  drm/drmeventlistener.cpp \
  drm/drmmode.cpp \
  drm/drmplane.cpp \
  drm/drmproperty.cpp \
  drm/drmcompositorworker.cpp \
  drm/resourcemanager.cpp \
  drm/vsyncworker.cpp \
  drm/invalidateworker.cpp \
  platform/platform.cpp \
  utils/autolock.cpp \
  platform/platformdrmgeneric.cpp \
  rockchip/utils/drmdebug.cpp \
  rockchip/drmlayer.cpp \
  rockchip/drmtype.cpp \
  rockchip/drmgralloc.cpp \
  rockchip/drmbaseparameter.cpp \
  rockchip/platform/drmvop3399.cpp \
  rockchip/platform/drmvop356x.cpp \
  rockchip/platform/drmvop3588.cpp

LOCAL_CPPFLAGS += \
  -DHWC2_USE_CPP11 \
  -DHWC2_INCLUDE_STRINGIFICATION \
  -DRK_DRM_GRALLOC \
  -DUSE_HWC2 \
  -DMALI_AFBC_GRALLOC \
  -DUSE_DRM_GENERIC_IMPORTER \
  -Wno-unreachable-code-loop-increment \
  -DUSE_NO_ASPECT_RATIO

ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 31)))
LOCAL_CFLAGS += -DANDROID_S
LOCAL_HEADER_LIBRARIES += \
  libhardware_rockchip_headers
endif

# API 30 -> Android 11.0
ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 30)))

LOCAL_C_INCLUDES += \
    hardware/rockchip/hwcomposer/drmhwc2/include

LOCAL_CPPFLAGS += -DANDROID_R

ifeq ($(TARGET_RK_GRALLOC_VERSION),4) # Gralloc 4.0

LOCAL_CPPFLAGS += -DUSE_GRALLOC_4=1

LOCAL_SHARED_LIBRARIES += \
    libhidlbase \
    libgralloctypes \
    android.hardware.graphics.mapper@4.0

LOCAL_SRC_FILES += \
    rockchip/drmgralloc4.cpp

LOCAL_HEADER_LIBRARIES += \
    libgralloc_headers

endif # Gralloc 4.0
else  # Android 11

LOCAL_C_INCLUDES += \
  hardware/rockchip/hwcomposer/include

endif

# API 29 -> Android 10.0
ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 29)))
LOCAL_CPPFLAGS += -DANDROID_Q
ifneq (,$(filter mali-tDVx mali-G52, $(TARGET_BOARD_PLATFORM_GPU)))
LOCAL_C_INCLUDES += \
  hardware/rockchip/libgralloc/bifrost \
  hardware/rockchip/libgralloc/bifrost/src
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
endif

MAJOR_VERSION := "RK_GRAPHICS_VER=commit-id:$(shell cd $(LOCAL_PATH) && git log  -1 --oneline | awk '{print $$1}')"
LOCAL_CPPFLAGS += -DRK_GRAPHICS_VER=\"$(MAJOR_VERSION)\"

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_HARDWARE)

# API 26 -> Android 8.0
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 26)))
LOCAL_PROPRIETARY_MODULE := true
endif

LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -Wno-unused-function -Wno-unused-private-field -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := $(TARGET_SHLIB_SUFFIX)
include $(BUILD_SHARED_LIBRARY)

endif
ifeq ($(strip $(BOARD_USES_DRM_HWCOMPOSER2)),true)
include $(call all-makefiles-under,$(LOCAL_PATH))
endif
