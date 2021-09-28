ifneq (false,$(BUILD_EMULATOR_OPENGL_DRIVER))

LOCAL_PATH := $(call my-dir)

define gralloc_recipe
$$(call emugl-begin-shared-library,gralloc.$(1))
$$(call emugl-import,libGLESv1_enc lib_renderControl_enc libOpenglSystemCommon)
$$(call emugl-set-shared-library-subpath,hw)

LOCAL_CFLAGS += -DLOG_TAG=\"gralloc_$(1)\"
LOCAL_CFLAGS += -Wno-missing-field-initializers
LOCAL_CFLAGS += -Wno-gnu-designator

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 30; echo $$?), 0)
LOCAL_SRC_FILES := gralloc_30.cpp
else
LOCAL_SRC_FILES := gralloc_old.cpp
endif

ifneq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))
LOCAL_SHARED_LIBRARIES += libdl
endif

$$(call emugl-end-module)
endef  # define gralloc_recipe

$(eval $(call gralloc_recipe,goldfish))
$(eval $(call gralloc_recipe,ranchu))
ifeq ($(TARGET_BOARD_PLATFORM),brilloemulator)
$(eval $(call gralloc_recipe,$(TARGET_BOARD_PLATFORM)))
endif  # defined(BRILLO)

endif # BUILD_EMULATOR_OPENGL_DRIVER != false
