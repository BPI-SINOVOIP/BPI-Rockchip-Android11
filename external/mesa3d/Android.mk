# Mesa 3-D graphics library
#
# Copyright (C) 2010-2011 Chia-I Wu <olvaffe@gmail.com>
# Copyright (C) 2010-2011 LunarG Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

# BOARD_GPU_DRIVERS should be defined.  The valid values are
#
#   classic drivers: i915 i965
#   gallium drivers: swrast freedreno i915g nouveau kmsro r300g r600g radeonsi vc4 virgl vmwgfx etnaviv iris lima panfrost tegra
#
# The main target is libGLES_mesa.  For each classic driver enabled, a DRI
# module will also be built.  DRI modules will be loaded by libGLES_mesa.

LOCAL_PATH := $(call my-dir)

ifneq ($(BOARD_USE_CUSTOMIZED_MESA), true)
ifneq ($(BOARD_GPU_DRIVERS),)

MESA_TOP := $(LOCAL_PATH)

ifeq ($(filter $(MESA_TOP),$(PRODUCT_SOONG_NAMESPACES)),)
  $(error $(MESA_TOP) must be in PRODUCT_SOONG_NAMESPACES)
endif

MESA_ANDROID_MAJOR_VERSION := $(word 1, $(subst ., , $(PLATFORM_VERSION)))
ifneq ($(filter 2 4, $(MESA_ANDROID_MAJOR_VERSION)),)
$(error "Android 4.4 and earlier not supported")
endif

MESA_DRI_MODULE_REL_PATH := dri
MESA_DRI_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(MESA_DRI_MODULE_REL_PATH)
MESA_DRI_MODULE_UNSTRIPPED_PATH := $(TARGET_OUT_SHARED_LIBRARIES_UNSTRIPPED)/$(MESA_DRI_MODULE_REL_PATH)
MESA_DRI_LDFLAGS := -Wl,--build-id=sha1

MESA_COMMON_MK := $(MESA_TOP)/Android.common.mk
MESA_PYTHON2 := python
MESA_PYTHON3 := python3
ifeq ($(filter 5 6 7 8 9 10, $(MESA_ANDROID_MAJOR_VERSION)),)
MESA_LEX     := M4=$(M4) $(LEX)
else
MESA_LEX     := $(LEX)
endif

# Lists to convert driver names to boolean variables
# in form of <driver name>.<boolean make variable>
classic_drivers := i915.HAVE_I915_DRI i965.HAVE_I965_DRI
gallium_drivers := \
	swrast.HAVE_GALLIUM_SOFTPIPE \
	freedreno.HAVE_GALLIUM_FREEDRENO \
	i915g.HAVE_GALLIUM_I915 \
	nouveau.HAVE_GALLIUM_NOUVEAU \
	tegra.HAVE_GALLIUM_TEGRA \
	kmsro.HAVE_GALLIUM_KMSRO \
	r300g.HAVE_GALLIUM_R300 \
	r600g.HAVE_GALLIUM_R600 \
	radeonsi.HAVE_GALLIUM_RADEONSI \
	vmwgfx.HAVE_GALLIUM_VMWGFX \
	vc4.HAVE_GALLIUM_VC4 \
	virgl.HAVE_GALLIUM_VIRGL \
	etnaviv.HAVE_GALLIUM_ETNAVIV \
	iris.HAVE_GALLIUM_IRIS \
	lima.HAVE_GALLIUM_LIMA \
	panfrost.HAVE_GALLIUM_PANFROST

ifeq ($(BOARD_GPU_DRIVERS),all)
MESA_BUILD_CLASSIC := $(filter HAVE_%, $(subst ., , $(classic_drivers)))
MESA_BUILD_GALLIUM := $(filter HAVE_%, $(subst ., , $(gallium_drivers)))
else
# Warn if we have any invalid driver names
$(foreach d, $(BOARD_GPU_DRIVERS), \
	$(if $(findstring $(d).,$(classic_drivers) $(gallium_drivers)), \
		, \
		$(warning invalid GPU driver: $(d)) \
	) \
)
MESA_BUILD_CLASSIC := $(strip $(foreach d, $(BOARD_GPU_DRIVERS), $(patsubst $(d).%,%, $(filter $(d).%, $(classic_drivers)))))
MESA_BUILD_GALLIUM := $(strip $(foreach d, $(BOARD_GPU_DRIVERS), $(patsubst $(d).%,%, $(filter $(d).%, $(gallium_drivers)))))
endif
ifeq ($(filter x86%,$(TARGET_ARCH)),)
	MESA_BUILD_CLASSIC :=
endif

$(foreach d, $(MESA_BUILD_CLASSIC) $(MESA_BUILD_GALLIUM), $(eval $(d) := true))

ifneq ($(filter true, $(HAVE_GALLIUM_RADEONSI)),)
MESA_ENABLE_LLVM := true
endif

define mesa-build-with-llvm
  $(if $(filter $(MESA_ANDROID_MAJOR_VERSION), 4 5 6 7), \
    $(warning Unsupported LLVM version in Android $(MESA_ANDROID_MAJOR_VERSION)),) \
  $(eval LOCAL_CFLAGS += -DLLVM_AVAILABLE -DLLVM_IS_SHARED=1 -DMESA_LLVM_VERSION_STRING=\"3.9\") \
  $(eval LOCAL_SHARED_LIBRARIES += libLLVM)
endef

# add subdirectories
SUBDIRS := \
	src/etnaviv \
	src/freedreno \
	src/gbm \
	src/loader \
	src/mapi \
	src/compiler \
	src/mesa \
	src/util \
	src/egl \
	src/amd \
	src/broadcom \
	src/intel \
	src/mesa/drivers/dri \
	src/vulkan \
	src/panfrost \

INC_DIRS := $(call all-named-subdir-makefiles,$(SUBDIRS))
INC_DIRS += $(call all-named-subdir-makefiles,src/gallium)
include $(INC_DIRS)

else
  ifneq ($(filter $(LOCAL_PATH),$(PRODUCT_SOONG_NAMESPACES)),)
    $(error $(LOCAL_PATH) in PRODUCT_SOONG_NAMESPACES, but not configured in Make)
  endif
endif # BOARD_GPU_DRIVERS != ""
else
  ifneq ($(filter $(LOCAL_PATH),$(PRODUCT_SOONG_NAMESPACES)),)
    $(error $(LOCAL_PATH) in PRODUCT_SOONG_NAMESPACES, but not configured in Make)
  endif
endif # BOARD_USE_CUSTOMIZED_MESA != true
