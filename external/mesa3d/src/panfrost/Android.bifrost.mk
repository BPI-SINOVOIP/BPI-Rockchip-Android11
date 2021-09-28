# Copyright © 2019 Collabora Ltd.
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

# build libpanfrost_bifrost_disasm
include $(CLEAR_VARS)

LOCAL_MODULE := libpanfrost_bifrost_disasm
LOCAL_LICENSE_KINDS := SPDX-license-identifier-MIT
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/../../LICENSE

LOCAL_SRC_FILES := \
	$(bifrost_disasm_FILES)

LOCAL_C_INCLUDES := \
	$(MESA_TOP)/include \
	$(MESA_TOP)/src/compiler/nir/ \
	$(MESA_TOP)/src/gallium/auxiliary/ \
	$(MESA_TOP)/src/gallium/include/ \
	$(MESA_TOP)/src/mapi/ \
	$(MESA_TOP)/src/mesa/ \
	$(MESA_TOP)/src/panfrost/bifrost/ \
	$(MESA_TOP)/src/panfrost/include/

LOCAL_EXPORT_C_INCLUDE_DIRS := \
	$(MESA_TOP)/src/panfrost/bifrost/ \

LOCAL_MODULE_CLASS := STATIC_LIBRARIES
intermediates := $(call local-generated-sources-dir)
prebuilt_intermediates := $(MESA_TOP)/prebuilt-intermediates

LOCAL_GENERATED_SOURCES := \
	$(intermediates)/bifrost_gen_disasm.c

bifrost_gen_disasm_gen := $(LOCAL_PATH)/bifrost/gen_disasm.py
bifrost_gen_disasm_deps := $(LOCAL_PATH)/bifrost/ISA.xml

$(intermediates)/bifrost_gen_disasm.c: $(prebuilt_intermediates)/bifrost/bifrost_gen_disasm.c
	@mkdir -p $(dir $@)
	@cp -f $< $@

include $(MESA_COMMON_MK)
include $(BUILD_STATIC_LIBRARY)

# build libpanfrost_bifrost
include $(CLEAR_VARS)

LOCAL_MODULE := libpanfrost_bifrost
LOCAL_LICENSE_KINDS := SPDX-license-identifier-MIT
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/../../LICENSE
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
intermediates := $(call local-generated-sources-dir)
prebuilt_intermediates := $(MESA_TOP)/prebuilt-intermediates

LOCAL_SRC_FILES := \
	$(bifrost_FILES)

LOCAL_C_INCLUDES := \
	$(MESA_TOP)/include \
	$(MESA_TOP)/src/compiler/nir/ \
	$(MESA_TOP)/src/gallium/auxiliary/ \
	$(MESA_TOP)/src/gallium/include/ \
	$(MESA_TOP)/src/mapi/ \
	$(MESA_TOP)/src/mesa/ \
	$(MESA_TOP)/src/panfrost/bifrost/ \
	$(MESA_TOP)/src/panfrost/include/

LOCAL_STATIC_LIBRARIES := \
	libmesa_glsl \
	libmesa_nir \
	libmesa_st_mesa \
	libpanfrost_lib

LOCAL_GENERATED_SOURCES := \
	$(intermediates)/bifrost_nir_algebraic.c \
	$(intermediates)/bi_generated_pack.h \
	$(MESA_GEN_GLSL_H)

bifrost_nir_algebraic_gen := $(LOCAL_PATH)/bifrost/bifrost_nir_algebraic.py
bifrost_nir_algebraic_deps := \
	$(MESA_TOP)/src/compiler/nir/

$(intermediates)/bifrost_nir_algebraic.c: $(prebuilt_intermediates)/bifrost/bifrost_nir_algebraic.c
	@mkdir -p $(dir $@)
	@cp -f $< $@

bi_generated_pack_gen := $(LOCAL_PATH)/bifrost/gen_pack.py
bi_generated_pack_deps := $(LOCAL_PATH)/bifrost/ISA.xml

$(intermediates)/bi_generated_pack.h: $(prebuilt_intermediates)/bifrost/bi_generated_pack.h
	@mkdir -p $(dir $@)
	@cp -f $< $@

LOCAL_EXPORT_C_INCLUDE_DIRS := \
	$(MESA_TOP)/src/panfrost/bifrost/ \

include $(MESA_COMMON_MK)
include $(BUILD_STATIC_LIBRARY)
