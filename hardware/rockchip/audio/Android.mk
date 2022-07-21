#legacy audio hal is just for debuging, and we use tinyalsa in all rk product
#use AUDIO_FORCE_LEGACY to choose which you need.

MY_LOCAL_PATH := $(call my-dir)

AUDIO_FORCE_LEGACY=false

ifeq ($(strip $(AUDIO_FORCE_LEGACY)), true)
    include $(MY_LOCAL_PATH)/legacy_hal/Android.mk
else
    include $(MY_LOCAL_PATH)/tinyalsa_hal/Android.mk
ifeq ($(strip $(BOARD_SUPPORT_MULTIAUDIO)), true)
    include $(MY_LOCAL_PATH)/hdmi_hal/Android.mk
    include $(MY_LOCAL_PATH)/hdmi_1_hal/Android.mk
    include $(MY_LOCAL_PATH)/spdif_hal/Android.mk
    include $(MY_LOCAL_PATH)/spdif_1_hal/Android.mk
endif
endif

