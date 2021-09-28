ifneq ($(filter rk3368 rk3399 rk3288 rk3366 rk3126c rk3328 rk3326 rk3128h rk322x rk3399pro, $(strip $(TARGET_BOARD_PLATFORM))), )
CONFIG_FILE_ANDROID = $(shell pwd)/productConfigs.mk
CONFIG_FILE_LINUX = $(call my-dir)/productConfigs.mk
ifeq ($(CONFIG_FILE_ANDROID), $(wildcard $(CONFIG_FILE_ANDROID)))
include $(CONFIG_FILE_ANDROID)
else ifeq ($(CONFIG_FILE_LINUX), $(wildcard $(CONFIG_FILE_LINUX)))
include $(CONFIG_FILE_LINUX)
endif

ifeq ($(IS_ANDROID_OS),true)
include $(call all-subdir-makefiles)
else
include $(call allSubdirMakefiles)
endif
else
#isp2.0 could not compile this
$(info TARGET_BOARD_PLATFORM=$(TARGET_BOARD_PLATFORM))
endif