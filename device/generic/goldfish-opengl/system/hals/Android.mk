#
# Copyright 2015 The Android Open-Source Project
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

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := android.hardware.graphics.allocator@3.0-service
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES := allocator3.cpp
LOCAL_INIT_RC := android.hardware.graphics.allocator@3.0-service.rc

LOCAL_SHARED_LIBRARIES += \
    android.hardware.graphics.allocator@3.0 \
    android.hardware.graphics.mapper@3.0 \
    libOpenglSystemCommon \
    libOpenglCodecCommon$(GOLDFISH_OPENGL_LIB_SUFFIX) \
    libbase \
    libcutils \
    libhidlbase \
    liblog \
    libutils

LOCAL_STATIC_LIBRARIES += libqemupipe.ranchu libGoldfishAddressSpace$(GOLDFISH_OPENGL_LIB_SUFFIX)
LOCAL_HEADER_LIBRARIES += libgralloc_cb.ranchu

LOCAL_C_INCLUDES += \
    device/generic/goldfish-opengl/system/include \
    device/generic/goldfish-opengl/system/OpenglSystemCommon \
    device/generic/goldfish-opengl/shared/GoldfishAddressSpace/include \
    device/generic/goldfish-opengl/shared/OpenglCodecCommon \
    device/generic/goldfish-opengl/host/include/libOpenglRender \
    device/generic/goldfish-opengl/system/renderControl_enc \

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE := android.hardware.graphics.mapper@3.0-impl-ranchu
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES := mapper3.cpp

#     android.hardware.graphics.allocator@3.0 \

LOCAL_SHARED_LIBRARIES += \
    android.hardware.graphics.mapper@3.0 \
    libOpenglSystemCommon \
    libOpenglCodecCommon$(GOLDFISH_OPENGL_LIB_SUFFIX) \
    libbase \
    libcutils \
    libhidlbase \
    liblog \
    libutils \
    libsync

LOCAL_STATIC_LIBRARIES += libqemupipe.ranchu libGoldfishAddressSpace$(GOLDFISH_OPENGL_LIB_SUFFIX)
LOCAL_HEADER_LIBRARIES += libgralloc_cb.ranchu

LOCAL_C_INCLUDES += \
    device/generic/goldfish-opengl/system/include \
    device/generic/goldfish-opengl/system/OpenglSystemCommon \
    device/generic/goldfish-opengl/shared/GoldfishAddressSpace/include \
    device/generic/goldfish-opengl/shared/OpenglCodecCommon \
    device/generic/goldfish-opengl/host/include/libOpenglRender \
    device/generic/goldfish-opengl/system/renderControl_enc \

include $(BUILD_SHARED_LIBRARY)

