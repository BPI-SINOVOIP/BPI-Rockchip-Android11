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

ifeq ($(strip $(BUILD_WITH_RK_EBOOK)),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libcfa
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES_arm := libcfa/lib/libcfa.so
LOCAL_32_BIT_ONLY := true
include $(BUILD_PREBUILT)

ifeq (${TARGET_ARCH},arm64)
include $(CLEAR_VARS)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libcfa
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES_arm64 := libcfa/lib64/libcfa.so
include $(BUILD_PREBUILT)
endif

include $(CLEAR_VARS)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libeink
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES_arm := libregal/lib/libeink.so
LOCAL_32_BIT_ONLY := true
include $(BUILD_PREBUILT)

ifeq (${TARGET_ARCH},arm64)
include $(CLEAR_VARS)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libeink
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES_arm64 := libregal/lib64/libeink.so
include $(BUILD_PREBUILT)
endif

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libhardware \
	liblog \
	libsync_vendor \
	libui \
	libutils \
        librga \
        libjpeg \
        libpng \
        libskia \
        libcfa \
        libeink

LOCAL_STATIC_LIBRARIES := \
	libtinyxml2


LOCAL_C_INCLUDES := \
        hardware/rockchip/libgralloc \
	external/tinyxml2 \
	external/libdrm \
	external/libdrm/include/drm \
	system/core/include/utils \
	system/core \
	system/core/libsync/include \
	hardware/rockchip/librga \
        frameworks/native/include \

LOCAL_SRC_FILES := \
	autolock.cpp \
	hwcomposer.cpp \
	separate_rects.cpp \
	vsyncworker.cpp \
	worker.cpp \
        hwc_util.cpp \
        hwc_rockchip.cpp \
        hwc_debug.cpp \
        einkcompositorworker.cpp

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
LOCAL_C_INCLUDES += \
        hardware/rockchip/libgralloc/bifrost/src \
        hardware/rockchip/libgralloc/bifrost \
        hardware/libhardware/include
endif

MAJOR_VERSION := "RK_GRAPHICS_VER=commit-id:$(shell cd $(LOCAL_PATH) && git log  -1 --oneline | awk '{print $$1}')"
LOCAL_CPPFLAGS += -DRK_GRAPHICS_VER=\"$(MAJOR_VERSION)\" -DRK356X

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_HARDWARE)


LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS += -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := $(TARGET_SHLIB_SUFFIX)
include $(BUILD_SHARED_LIBRARY)
endif

