#
# CHRE Audio Concurrency Test Nanoapp Makefile
#

# Environment Checks ###########################################################
ifeq ($(CHRE_PREFIX),)
  ifneq ($(ANDROID_BUILD_TOP),)
    CHRE_PREFIX = $(ANDROID_BUILD_TOP)/system/chre
  else
    $(error "You must run 'lunch' to setup ANDROID_BUILD_TOP, or explicitly \
    define the CHRE_PREFIX environment variable to point to the CHRE root \
    directory.")
  endif
endif

# Nanoapp Configuration ########################################################

NANOAPP_NAME = chre_audio_concurrency_test
NANOAPP_ID = 0x476f6f6754000004
NANOAPP_NAME_STRING = \"Chre\ Audio\ Concurrency\ Test\"
NANOAPP_VERSION = 0x00000002

NANOAPP_PATH = $(CHRE_PREFIX)/apps/test/common/chre_audio_concurrency_test
TEST_SHARED_PATH = $(CHRE_PREFIX)/apps/test/common/shared

# Protobuf Sources #############################################################

NANOPB_EXTENSION = nanopb

NANOPB_SRCS += $(NANOAPP_PATH)/../proto/chre_audio_concurrency_test.proto
NANOPB_SRCS += $(NANOAPP_PATH)/../proto/chre_test_common.proto
NANOPB_PROTO_PATH = $(NANOAPP_PATH)/../proto
NANOPB_INCLUDES = $(NANOPB_PROTO_PATH)
NANOPB_FLAGS += --proto_path=$(NANOPB_PROTO_PATH)

# Source Code ##################################################################

COMMON_SRCS += $(NANOAPP_PATH)/src/chre_audio_concurrency_test.cc
COMMON_SRCS += $(NANOAPP_PATH)/src/chre_audio_concurrency_test_manager.cc
COMMON_SRCS += $(TEST_SHARED_PATH)/src/send_message.cc
COMMON_SRCS += $(CHRE_PREFIX)/util/nanoapp/callbacks.cc

# Compiler Flags ###############################################################

# Defines
COMMON_CFLAGS += -DNANOAPP_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_INFO

# Includes
COMMON_CFLAGS += -I$(NANOAPP_PATH)/inc
COMMON_CFLAGS += -I$(TEST_SHARED_PATH)/inc

# Makefile Includes ############################################################

include $(CHRE_PREFIX)/build/nanoapp/app.mk
