# Copyright (C) 2014 The Android Open Source Project
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

ifeq ($(USE_CUSTOM_AUDIO_POLICY), 1)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    ATVAudioPolicyManager.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    liblog \
    libutils \
    libmedia_helper \
    libbinder \
    libaudiofoundation \
    libaudiopolicymanagerdefault

LOCAL_STATIC_LIBRARIES := \
    libaudiopolicycomponents

LOCAL_C_INCLUDES += \
    $(call include-path-for, audio-utils) \
    $(TOPDIR)frameworks/av/services/audiopolicy

LOCAL_HEADER_LIBRARIES := \
    libaudiopolicycommon \
    libaudiopolicyengine_interface_headers \
    libaudiopolicymanager_interface_headers

LOCAL_CFLAGS := -Wall -Werror

LOCAL_MODULE:= libaudiopolicymanager

include $(BUILD_SHARED_LIBRARY)

endif #ifeq ($(USE_CUSTOM_AUDIO_POLICY), 1)
