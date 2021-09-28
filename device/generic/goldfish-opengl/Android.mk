# This is the top-level build file for the Android HW OpenGL ES emulation
# in Android.
#
# You must define BUILD_EMULATOR_OPENGL to 'true' in your environment to
# build the following files.
#
# Also define BUILD_EMULATOR_OPENGL_DRIVER to 'true' to build the gralloc
# stuff as well.
#
# Top-level for all modules
GOLDFISH_OPENGL_PATH := $(call my-dir)

# There are two kinds of builds for goldfish-opengl:
# 1. The standard guest build, denoted by BUILD_EMULATOR_OPENGL
# 2. The host-side build, denoted by GOLDFISH_OPENGL_BUILD_FOR_HOST
#
# Variable controlling whether the build for goldfish-opengl
# libraries (including their Android.mk's) should be triggered.
GOLDFISH_OPENGL_SHOULD_BUILD := false

# In the host build, some libraries have name collisions with
# other libraries, so we have this variable here to control
# adding a suffix to the names of libraries. Should be blank
# for the guest build.
GOLDFISH_OPENGL_LIB_SUFFIX :=

# Directory containing common headers used by several modules
# This is always set to a module's LOCAL_C_INCLUDES
# See the definition of emugl-begin-module in common.mk
EMUGL_COMMON_INCLUDES := $(GOLDFISH_OPENGL_PATH)/host/include/libOpenglRender $(GOLDFISH_OPENGL_PATH)/system/include

# common cflags used by several modules
# This is always set to a module's LOCAL_CFLAGS
# See the definition of emugl-begin-module in common.mk
EMUGL_COMMON_CFLAGS := -DWITH_GLES2

# Whether or not to build the Vulkan library.
BUILD_EMULATOR_VULKAN := false

# Host build
ifeq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))

GOLDFISH_OPENGL_SHOULD_BUILD := true
GOLDFISH_OPENGL_LIB_SUFFIX := _host

BUILD_EMULATOR_VULKAN := true

# Set modern defaults for the codename, version, etc.
PLATFORM_VERSION_CODENAME:=Q
PLATFORM_SDK_VERSION:=29
IS_AT_LEAST_OPD1:=true

# The host-side Android framework implementation
HOST_EMUGL_PATH := $(GOLDFISH_OPENGL_PATH)/../../../external/qemu/android/android-emugl
EMUGL_COMMON_INCLUDES += $(HOST_EMUGL_PATH)/guest

EMUGL_COMMON_CFLAGS += \
    -DPLATFORM_SDK_VERSION=29 \
    -DGOLDFISH_HIDL_GRALLOC \
    -DEMULATOR_OPENGL_POST_O=1 \
    -DHOST_BUILD \
    -DANDROID \
    -DGL_GLEXT_PROTOTYPES \
    -fvisibility=default \
    -DPAGE_SIZE=4096 \
    -DGOLDFISH_VULKAN \
    -Wno-unused-parameter

endif # GOLDFISH_OPENGL_BUILD_FOR_HOST

ifeq (true,$(BUILD_EMULATOR_OPENGL)) # Guest build

GOLDFISH_OPENGL_SHOULD_BUILD := true

EMUGL_COMMON_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

ifeq (O, $(PLATFORM_VERSION_CODENAME))
EMUGL_COMMON_CFLAGS += -DGOLDFISH_HIDL_GRALLOC
endif

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -gt 25 && echo isApi26OrHigher),isApi26OrHigher)
EMUGL_COMMON_CFLAGS += -DGOLDFISH_HIDL_GRALLOC
endif

ifdef IS_AT_LEAST_OPD1
    EMUGL_COMMON_CFLAGS += -DEMULATOR_OPENGL_POST_O=1
endif

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -lt 18 && echo PreJellyBeanMr2),PreJellyBeanMr2)
    ifeq ($(ARCH_ARM_HAVE_TLS_REGISTER),true)
        EMUGL_COMMON_CFLAGS += -DHAVE_ARM_TLS_REGISTER
    endif
endif
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -lt 16 && echo PreJellyBean),PreJellyBean)
    EMUGL_COMMON_CFLAGS += -DALOG_ASSERT=LOG_ASSERT
    EMUGL_COMMON_CFLAGS += -DALOGE=LOGE
    EMUGL_COMMON_CFLAGS += -DALOGW=LOGW
    EMUGL_COMMON_CFLAGS += -DALOGD=LOGD
    EMUGL_COMMON_CFLAGS += -DALOGV=LOGV
endif

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -gt 27 && echo isApi28OrHigher),isApi28OrHigher)
    BUILD_EMULATOR_VULKAN := true
    EMUGL_COMMON_CFLAGS += -DGOLDFISH_VULKAN
endif

# Include common definitions used by all the modules included later
# in this build file. This contains the definition of all useful
# emugl-xxxx functions.
#
include $(GOLDFISH_OPENGL_PATH)/common.mk

endif # BUILD_EMULATOR_OPENGL (guest build)

ifeq (true,$(GOLDFISH_OPENGL_SHOULD_BUILD))

# Uncomment the following line if you want to enable debug traces
# in the GLES emulation libraries.
# EMUGL_COMMON_CFLAGS += -DEMUGL_DEBUG=1

# IMPORTANT: ORDER IS CRUCIAL HERE
#
# For the import/export feature to work properly, you must include
# modules below in correct order. That is, if module B depends on
# module A, then it must be included after module A below.
#
# This ensures that anything exported by module A will be correctly
# be imported by module B when it is declared.
#
# Note that the build system will complain if you try to import a
# module that hasn't been declared yet anyway.
#
include $(GOLDFISH_OPENGL_PATH)/shared/qemupipe/Android.mk
include $(GOLDFISH_OPENGL_PATH)/shared/gralloc_cb/Android.mk
include $(GOLDFISH_OPENGL_PATH)/shared/GoldfishAddressSpace/Android.mk
include $(GOLDFISH_OPENGL_PATH)/shared/OpenglCodecCommon/Android.mk

# Encoder shared libraries
include $(GOLDFISH_OPENGL_PATH)/system/GLESv1_enc/Android.mk
include $(GOLDFISH_OPENGL_PATH)/system/GLESv2_enc/Android.mk
include $(GOLDFISH_OPENGL_PATH)/system/renderControl_enc/Android.mk

ifeq (true,$(BUILD_EMULATOR_VULKAN)) # Vulkan libs
    include $(GOLDFISH_OPENGL_PATH)/android-emu/Android.mk
    include $(GOLDFISH_OPENGL_PATH)/system/vulkan_enc/Android.mk
endif

include $(GOLDFISH_OPENGL_PATH)/system/OpenglSystemCommon/Android.mk

# System shared libraries
include $(GOLDFISH_OPENGL_PATH)/system/GLESv1/Android.mk
include $(GOLDFISH_OPENGL_PATH)/system/GLESv2/Android.mk

include $(GOLDFISH_OPENGL_PATH)/system/gralloc/Android.mk

ifneq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))
include $(GOLDFISH_OPENGL_PATH)/system/hals/Android.mk
endif

include $(GOLDFISH_OPENGL_PATH)/system/egl/Android.mk

ifeq (true,$(BUILD_EMULATOR_VULKAN)) # Vulkan libs
    include $(GOLDFISH_OPENGL_PATH)/system/vulkan/Android.mk
endif

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -gt 28 -o $(IS_AT_LEAST_QPR1) = true && echo isApi29OrHigher),isApi29OrHigher)
    # HWC2 enabled after P
    include $(GOLDFISH_OPENGL_PATH)/system/hwc2/Android.mk
    # hardware codecs enabled after P
    include $(GOLDFISH_OPENGL_PATH)/system/codecs/omx/common/Android.mk
    include $(GOLDFISH_OPENGL_PATH)/system/codecs/omx/plugin/Android.mk
    include $(GOLDFISH_OPENGL_PATH)/system/codecs/omx/avcdec/Android.mk
    include $(GOLDFISH_OPENGL_PATH)/system/codecs/omx/vpxdec/Android.mk
endif

endif
