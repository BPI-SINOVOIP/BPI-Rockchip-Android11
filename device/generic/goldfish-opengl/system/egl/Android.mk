ifneq (false,$(BUILD_EMULATOR_OPENGL_DRIVER))

LOCAL_PATH := $(call my-dir)

$(call emugl-begin-shared-library,libEGL_emulation)
$(call emugl-import,libOpenglSystemCommon)
$(call emugl-set-shared-library-subpath,egl)

ifeq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))
$(call emugl-import,libqemupipe$(GOLDFISH_OPENGL_LIB_SUFFIX))
else
$(call emugl-export,STATIC_LIBRARIES,libqemupipe.ranchu)
endif

LOCAL_CFLAGS += -DLOG_TAG=\"EGL_emulation\" -DEGL_EGLEXT_PROTOTYPES -DWITH_GLES2
LOCAL_CFLAGS += -Wno-gnu-designator

LOCAL_SRC_FILES := \
    eglDisplay.cpp \
    egl.cpp \
    ClientAPIExts.cpp

ifneq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))

LOCAL_SHARED_LIBRARIES += libdl
endif

ifneq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))
ifdef IS_AT_LEAST_OPM1
LOCAL_HEADER_LIBRARIES += libui_headers
endif

ifdef IS_AT_LEAST_OPD1
LOCAL_HEADER_LIBRARIES += libnativebase_headers
LOCAL_STATIC_LIBRARIES += libarect
LOCAL_SHARED_LIBRARIES += libnativewindow
endif

endif # !GOLDFISH_OPENGL_BUILD_FOR_HOST

$(call emugl-end-module)

#### egl.cfg ####

# Ensure that this file is only copied to emulator-specific builds.
# Other builds are device-specific and will provide their own
# version of this file to point to the appropriate HW EGL libraries.
#
ifneq (,$(filter aosp_arm aosp_x86 aosp_mips full full_x86 full_mips sdk sdk_x86 sdk_mips google_sdk google_sdk_x86 google_sdk_mips,$(TARGET_PRODUCT)))
include $(CLEAR_VARS)

LOCAL_MODULE := egl.cfg
LOCAL_SRC_FILES := $(LOCAL_MODULE)

LOCAL_MODULE_PATH := $(TARGET_OUT)/lib/egl
LOCAL_MODULE_CLASS := ETC

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -lt 19 && echo PreKitkat),PreKitkat)
    LOCAL_MODULE_TAGS := debug
endif

include $(BUILD_PREBUILT)
endif # TARGET_PRODUCT in 'full full_x86 full_mips sdk sdk_x86 sdk_mips google_sdk google_sdk_x86 google_sdk_mips')

endif # BUILD_EMULATOR_OPENGL_DRIVER != false
