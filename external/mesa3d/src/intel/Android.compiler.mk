#
# Copyright (C) 2011 Intel Corporation
# Copyright (C) 2010-2011 Chia-I Wu <olvaffe@gmail.com>
# Copyright (C) 2010-2011 LunarG
# Copyright (C) 2016 Linaro, Ltd., Rob Herring <robh@kernel.org>
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
#

# ---------------------------------------
# Build libmesa_intel_compiler
# ---------------------------------------

include $(CLEAR_VARS)

LOCAL_MODULE := libmesa_intel_compiler
LOCAL_LICENSE_KINDS := SPDX-license-identifier-ISC SPDX-license-identifier-MIT legacy_by_exception_only legacy_notice
LOCAL_LICENSE_CONDITIONS := by_exception_only notice
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/../../LICENSE
LOCAL_MODULE_CLASS := STATIC_LIBRARIES

LOCAL_SRC_FILES := \
	$(COMPILER_FILES)

LOCAL_C_INCLUDES := \
	$(MESA_TOP)/src/intel \
	$(MESA_TOP)/src/mapi \
	$(MESA_TOP)/src/mesa \
	$(MESA_TOP)/src/gallium/auxiliary \
	$(MESA_TOP)/src/gallium/include \
	$(call generated-sources-dir-for,STATIC_LIBRARIES,libmesa_glsl,,)/glsl \
	$(call generated-sources-dir-for,STATIC_LIBRARIES,libmesa_nir,,)/nir \
	$(MESA_TOP)/src/intel/compiler \
	$(MESA_TOP)/src/compiler/nir

intermediates := $(call local-generated-sources-dir)
prebuilt_intermediates := $(MESA_TOP)/prebuilt-intermediates

$(intermediates)/compiler/brw_nir_trig_workarounds.c: $(prebuilt_intermediates)/compiler/brw_nir_trig_workarounds.c
	@mkdir -p $(dir $@)
	@cp -f $< $@

LOCAL_STATIC_LIBRARIES = libmesa_genxml

LOCAL_GENERATED_SOURCES += $(addprefix $(intermediates)/, \
	$(COMPILER_GENERATED_FILES))

LOCAL_GENERATED_SOURCES += $(MESA_GEN_GLSL_H)

include $(MESA_COMMON_MK)
include $(BUILD_STATIC_LIBRARY)
