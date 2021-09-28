#
# TensorFlow Lite for Microcontroller demo app Makefile
#

# Environment Checks ###########################################################

ifeq ($(ANDROID_BUILD_TOP),)
$(error "You must run build/envsetup.sh and lunch first")
endif

# Nanoapp Configuration ########################################################

NANOAPP_NAME = tflm_demo
NANOAPP_ID = 0x476f6f676c002008
NANOAPP_VERSION = 0x00000001 # Version scheme: 0xMMmmpppp

NANOAPP_NAME_STRING = \"TFLM\ Demo\"

# Source Directories ###########################################################

NANOAPP_PATH = src

# Common Compiler Flags ########################################################

# Include paths
COMMON_CFLAGS += -I$(ANDROID_BUILD_TOP)/system/chre/util/include
COMMON_CFLAGS += -I./src

# Defines
COMMON_CFLAGS += -DLOG_LEVEL=LOG_LEVEL_INFO
COMMON_CFLAGS += -DNANOAPP_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_INFO
COMMON_CFLAGS += -DCHRE_NANOAPP_DISABLE_BACKCOMPAT
COMMON_CFLAGS += -DNDEBUG

# If OPT_LEVEL is unset, defaulting to 3.
ifeq ($(OPT_LEVEL),)
	OPT_LEVEL = 3
endif

# Common Source Files ##########################################################

COMMON_SRCS += $(NANOAPP_PATH)/main.cc
COMMON_SRCS += $(NANOAPP_PATH)/model.cc
COMMON_SRCS += $(NANOAPP_PATH)/sine_model_data.cc

# TensorFlow Lite for Micro ####################################################

TFLM_PATH = $(ANDROID_BUILD_TOP)/system/chre/external/tflm/latest
USE_TFLM = true

# Makefile Includes ############################################################

include $(ANDROID_BUILD_TOP)/system/chre/build/nanoapp/app.mk
