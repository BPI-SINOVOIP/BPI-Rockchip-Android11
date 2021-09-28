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
        goldfish_media_utils.cpp

$(call emugl-begin-shared-library,libgoldfish_codecs_common$(GOLDFISH_OPENGL_LIB_SUFFIX))

LOCAL_SRC_FILES := $(commonSources)

LOCAL_CFLAGS += -DLOG_TAG=\"goldfish_codecs_common\"
LOCAL_CFLAGS += -Wno-unused-private-field

$(call emugl-export,SHARED_LIBRARIES,libcutils libutils liblog)

$(call emugl-export,C_INCLUDES,$(LOCAL_PATH))

ifeq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))
$(call emugl-import,libGoldfishAddressSpace$(GOLDFISH_OPENGL_LIB_SUFFIX))
else
$(call emugl-export,STATIC_LIBRARIES,libGoldfishAddressSpace)
endif

$(call emugl-end-module)
