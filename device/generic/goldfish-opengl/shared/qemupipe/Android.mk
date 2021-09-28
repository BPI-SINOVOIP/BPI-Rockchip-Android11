ifeq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))

LOCAL_PATH := $(call my-dir)

$(call emugl-begin-static-library,libqemupipe$(GOLDFISH_OPENGL_LIB_SUFFIX))

LOCAL_SRC_FILES := \
    qemu_pipe_common.cpp \
    qemu_pipe_host.cpp

$(call emugl-export,SHARED_LIBRARIES,android-emu-shared)
$(call emugl-export,C_INCLUDES,$(LOCAL_PATH)/include-types)
$(call emugl-export,C_INCLUDES,$(LOCAL_PATH)/include)
$(call emugl-end-module)

endif
