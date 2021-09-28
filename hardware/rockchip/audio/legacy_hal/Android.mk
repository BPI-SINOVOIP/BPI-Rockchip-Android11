# Copyright 2011 The Android Open Source Project
ifneq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)
#AUDIO_POLICY_TEST := true
#ENABLE_AUDIO_DUMP := true

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    AudioHardwareInterface.cpp\
    AudioHardware.cpp \
    audio_hw_hal.cpp\
    alsa_mixer.c\
    alsa_route.c\
    alsa_pcm.c

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS += -DWITH_A2DP
endif

ifeq ($(strip $(TARGET_BOARD_HARDWARE)), rk2928board)
  LOCAL_CFLAGS += -DTARGET_RK2928
endif

LOCAL_MODULE := audio.primary.$(TARGET_BOARD_HARDWARE)
ifneq (1,$(strip $(shell expr $(PLATFORM_VERSION) \>= 5.0)))
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
else
ifneq ($(strip $(TARGET_2ND_ARCH)), )
LOCAL_MULTILIB := both
endif
LOCAL_MODULE_RELATIVE_PATH := hw
endif
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := libmedia_helper \
	libspeex

LOCAL_C_INCLUDES := \
    $(call include-path-for, speex)
LOCAL_SHARED_LIBRARIES:= libc libcutils libutils libmedia libhardware_legacy libspeexresampler
include $(BUILD_SHARED_LIBRARY)
include $(CLEAR_VARS)
LOCAL_SRC_FILES:= amix.c alsa_mixer.c
LOCAL_CFLAGS += -DSUPPORT_USB
LOCAL_MODULE:= amix
LOCAL_SHARED_LIBRARIES := libc libcutils
LOCAL_MODULE_TAGS:= debug
include $(BUILD_EXECUTABLE)

#build the usb simple libaudio
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    AudioHardwareInterface.cpp\
    AudioHardware.cpp \
    audio_hw_hal.cpp\
    alsa_mixer.c\
    alsa_route.c\
    alsa_pcm.c

LOCAL_CFLAGS += -DSUPPORT_USB
LOCAL_MODULE := audio.alsa_usb.$(TARGET_BOARD_HARDWARE)
ifneq (1,$(strip $(shell expr $(PLATFORM_VERSION) \>= 5.0)))
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
else
ifneq ($(strip $(TARGET_2ND_ARCH)), )
LOCAL_MULTILIB := both
endif
LOCAL_MODULE_RELATIVE_PATH := hw
endif
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := libmedia_helper \
	libspeex
LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_C_INCLUDES := \
    $(call include-path-for, speex)
LOCAL_SHARED_LIBRARIES:= libc libcutils libutils libmedia libhardware_legacy libspeexresampler
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    AudioPolicyManagerBase.cpp \
    AudioPolicyCompatClient.cpp \
    audio_policy_hal.cpp

ifeq ($(AUDIO_POLICY_TEST),true)
  LOCAL_CFLAGS += -DAUDIO_POLICY_TEST
endif

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS += -DWITH_A2DP
endif

ifeq ($(strip $(TARGET_BOARD_HARDWARE)), rk2928board)
  LOCAL_CFLAGS += -DTARGET_RK2928
endif

LOCAL_STATIC_LIBRARIES := libmedia_helper
LOCAL_MODULE := libaudiopolicy_$(TARGET_BOARD_HARDWARE)
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

# The default audio policy, for now still implemented on top of legacy
# policy code
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    AudioPolicyManagerDefault.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libmedia \
    liblog

LOCAL_STATIC_LIBRARIES := \
    libmedia_helper

LOCAL_WHOLE_STATIC_LIBRARIES := \
    libaudiopolicy_$(TARGET_BOARD_HARDWARE)

include $(CLEAR_VARS)
LOCAL_MODULE := audio_policy.$(TARGET_BOARD_HARDWARE)
ifneq (1,$(strip $(shell expr $(PLATFORM_VERSION) \>= 5.0)))
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
else
ifneq ($(strip $(TARGET_2ND_ARCH)), )
LOCAL_MULTILIB := both
endif
LOCAL_MODULE_RELATIVE_PATH := hw
endif
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -Wno-unused-parameter
ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS += -DWITH_A2DP
endif

include $(BUILD_SHARED_LIBRARY)
endif

