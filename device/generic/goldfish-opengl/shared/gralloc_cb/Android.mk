ifeq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))

LOCAL_PATH := $(call my-dir)

$(call emugl-begin-static-library,libgralloc_cb$(GOLDFISH_OPENGL_LIB_SUFFIX))
LOCAL_SRC_FILES := empty.cpp
$(call emugl-export,C_INCLUDES,$(LOCAL_PATH)/include)
$(call emugl-end-module)

endif
