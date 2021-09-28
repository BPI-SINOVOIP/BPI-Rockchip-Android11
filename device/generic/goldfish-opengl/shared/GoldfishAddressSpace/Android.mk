ifeq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))

LOCAL_PATH := $(call my-dir)

$(call emugl-begin-static-library,libGoldfishAddressSpace$(GOLDFISH_OPENGL_LIB_SUFFIX))

LOCAL_SRC_FILES := goldfish_address_space.cpp

LOCAL_CFLAGS += -DLOG_TAG=\"goldfish-address-space\"

$(call emugl-export,SHARED_LIBRARIES,liblog android-emu-shared)
$(call emugl-export,C_INCLUDES,$(LOCAL_PATH)/include)
$(call emugl-end-module)

endif
