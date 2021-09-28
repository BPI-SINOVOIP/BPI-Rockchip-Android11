#
# Copyright 2019 The Android Open-Source Project
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
#
LOCAL_PATH := $(call my-dir)

commonSources := \
        GoldfishAVCDec.cpp  \
        MediaH264Decoder.cpp

$(call emugl-begin-shared-library,libstagefright_goldfish_avcdec$(GOLDFISH_OPENGL_LIB_SUFFIX))

LOCAL_SRC_FILES := $(commonSources)

LOCAL_CFLAGS += -DLOG_TAG=\"goldfish_avcdec\"
LOCAL_CFLAGS += -Wno-unused-private-field

$(call emugl-export,SHARED_LIBRARIES,libcutils libutils liblog)

LOCAL_HEADER_LIBRARIES := media_plugin_headers \
                          libmedia_headers \
                          libbinder_headers \
                          libhidlbase_impl_internal \
                          libbase
LOCAL_HEADER_LIBRARIES += libui_headers \
                          libnativewindow_headers \
                          libhardware_headers \
                          libarect_headers \
                          libarect_headers_for_ndk
LOCAL_SHARED_LIBRARIES :=       \
        libbinder               \
        libutils                \
        liblog                  \
        libcutils               \
        libui \
        android.hardware.media.omx@1.0 \
	    android.hardware.graphics.allocator@3.0 \
		android.hardware.graphics.mapper@3.0 \
        libstagefright_foundation

LOCAL_HEADER_LIBRARIES += libgralloc_cb.ranchu

$(call emugl-export,C_INCLUDES,$(LOCAL_PATH))
$(call emugl-import,libgoldfish_codecs_common)
$(call emugl-import,libstagefrighthw)
$(call emugl-end-module)
