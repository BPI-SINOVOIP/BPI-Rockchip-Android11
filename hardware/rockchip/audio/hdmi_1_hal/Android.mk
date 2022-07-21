# Copyright (C) 2012 The Android Open Source Project
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

include $(CLEAR_VARS)

LOCAL_MODULE := audio.hdmi_1.$(TARGET_BOARD_HARDWARE)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := \
	bitstream/audio_iec958.c \
	bitstream/audio_bitstream.c \
	bitstream/audio_bitstream_manager.c \
	audio_hw.c \
	alsa_route.c \
	alsa_mixer.c \
	voice_preprocess.c \
	audio_hw_hdmi.c \
	denoise/rkdenoise.c

LOCAL_C_INCLUDES += \
	$(call include-path-for, audio-utils) \
	$(call include-path-for, audio-route) \
	$(call include-path-for, speex)


LOCAL_HEADER_LIBRARIES += libhardware_headers
LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_CFLAGS += -DLIBTINYALSA_ENABLE_VNDK_EXT
ifneq ($(filter box atv, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
LOCAL_CFLAGS += -DBOX_HAL
endif
ifeq ($(strip $(BOARD_USE_DRM)),true)
LOCAL_CFLAGS += -DUSE_DRM
endif
ifeq ($(strip $(BOARD_USE_AUDIO_3A)),true)
LOCAL_CFLAGS += -DAUDIO_3A
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3368)
LOCAL_CFLAGS += -DRK3368
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3228h)
        LOCAL_CFLAGS += -DRK3228
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3328)
        LOCAL_CFLAGS += -DRK3228
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3228)
        LOCAL_CFLAGS += -DRK3228
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk322x)
	LOCAL_CFLAGS += -DRK3228
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3128h)
        LOCAL_CFLAGS += -DRK3228
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3399)
    LOCAL_CFLAGS += -DRK3399
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3399pro)
    LOCAL_CFLAGS += -DRK3399
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3128)
    LOCAL_CFLAGS += -DRK3128
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk3588)
    LOCAL_CFLAGS += -DRK3588
    LOCAL_CFLAGS += -DIEC958_FORAMT
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), laptop)
LOCAL_CFLAGS += -DRK3399_LAPTOP
LOCAL_CFLAGS += -DBT_AP_SCO
endif
ifeq ($(AUD_VOICE_CONFIG),voice_support)
LOCAL_CFLAGS += -DVOICE_SUPPORT
endif
LOCAL_CFLAGS += -Wno-error

LOCAL_SHARED_LIBRARIES := liblog libcutils libaudioutils libaudioroute libhardware_legacy libspeexresampler

#API 31 -> Android 12.0, Android 12.0 link libtinyalsa_iec958
ifneq (1, $(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 31)))
LOCAL_SHARED_LIBRARIES += libtinyalsa_iec958
else
LOCAL_SHARED_LIBRARIES += libtinyalsa
endif

LOCAL_STATIC_LIBRARIES := libspeex
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
