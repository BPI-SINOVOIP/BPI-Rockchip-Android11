# Copyright Â© 2016 Red Hat.
# Copyright Â© 2016 Mauro Rossi <issor.oruam@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

ifeq ($(MESA_ENABLE_LLVM),true)

# ---------------------------------------
# Build libmesa_amd_common
# ---------------------------------------

include $(CLEAR_VARS)

LOCAL_MODULE := libmesa_amd_common
LOCAL_LICENSE_KINDS := SPDX-license-identifier-ISC SPDX-license-identifier-MIT
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/../../LICENSE

LOCAL_SRC_FILES := \
	$(AMD_COMMON_FILES) \
	$(AMD_COMMON_LLVM_FILES) \
	$(AMD_DEBUG_FILES)

LOCAL_CFLAGS += -DFORCE_BUILD_AMDGPU   # instructs LLVM to declare LLVMInitializeAMDGPU* functions

# generate sources
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
intermediates := $(call local-generated-sources-dir)
LOCAL_GENERATED_SOURCES := $(addprefix $(intermediates)/, $(AMD_GENERATED_FILES))

AMD_JSON_FILES := \
	$(LOCAL_PATH)/registers/gfx6.json \
	$(LOCAL_PATH)/registers/gfx7.json \
	$(LOCAL_PATH)/registers/gfx8.json \
	$(LOCAL_PATH)/registers/gfx81.json \
	$(LOCAL_PATH)/registers/gfx9.json \
	$(LOCAL_PATH)/registers/gfx10.json \
	$(LOCAL_PATH)/registers/gfx103.json \
	$(LOCAL_PATH)/registers/pkt3.json \
	$(LOCAL_PATH)/registers/gfx10-rsrc.json \
	$(LOCAL_PATH)/registers/registers-manually-defined.json

SID_TABLES := $(LOCAL_PATH)/common/sid_tables.py

SID_TABLES_INPUTS := \
	$(LOCAL_PATH)/common/sid.h \
	$(AMD_JSON_FILES)

$(intermediates)/common/sid_tables.h: $(SID_TABLES) $(SID_TABLES_INPUTS)
	@mkdir -p $(dir $@)
	@echo "Gen Header: $(PRIVATE_MODULE) <= $(notdir $(@))"
	$(hide) $(MESA_PYTHON2) $(SID_TABLES) $(SID_TABLES_INPUTS) > $@ || ($(RM) $@; false)

AMDGFXREGS := $(LOCAL_PATH)/registers/makeregheader.py

AMDGFXREGS_INPUTS := \
	$(AMD_JSON_FILES)

$(intermediates)/common/amdgfxregs.h: $(AMDGFXREGS) $(AMDGFXREGS_INPUTS)
	@mkdir -p $(dir $@)
	@echo "Gen Header: $(PRIVATE_MODULE) <= $(notdir $(@))"
	$(hide) $(MESA_PYTHON2) $(AMDGFXREGS) $(AMDGFXREGS_INPUTS) --sort address --guard AMDGFXREGS_H > $@ || ($(RM) $@; false)

GEN10_FORMAT_TABLE_INPUTS := \
	$(MESA_TOP)/src/util/format/u_format.csv \
	$(MESA_TOP)/src/amd/registers/gfx10-rsrc.json

GEN10_FORMAT_TABLE_DEP := \
	$(MESA_TOP)/src/amd/registers/regdb.py

GEN10_FORMAT_TABLE := $(LOCAL_PATH)/common/gfx10_format_table.py

$(intermediates)/common/gfx10_format_table.c: $(GEN10_FORMAT_TABLE) $(GEN10_FORMAT_TABLE_INPUTS) $(GEN10_FORMAT_TABLE_DEP)
	@mkdir -p $(dir $@)
	@echo "Gen Header: $(PRIVATE_MODULE) <= $(notdir $(@))"
	$(hide) $(MESA_PYTHON2) $(GEN10_FORMAT_TABLE) $(GEN10_FORMAT_TABLE_INPUTS) > $@ || ($(RM) $@; false)

LOCAL_C_INCLUDES := \
	$(MESA_TOP)/include \
	$(MESA_TOP)/src \
	$(MESA_TOP)/src/amd/common \
	$(MESA_TOP)/src/amd/llvm \
	$(MESA_TOP)/src/compiler \
	$(call generated-sources-dir-for,STATIC_LIBRARIES,libmesa_nir,,)/nir \
	$(MESA_TOP)/src/gallium/include \
	$(MESA_TOP)/src/gallium/auxiliary \
	$(MESA_TOP)/src/mesa \
	$(intermediates)/common

LOCAL_EXPORT_C_INCLUDE_DIRS := \
	$(LOCAL_PATH)/common \
	$(LOCAL_PATH)/llvm \
	$(intermediates)/common

LOCAL_SHARED_LIBRARIES := \
	libdrm_amdgpu

LOCAL_STATIC_LIBRARIES := \
	libmesa_nir

LOCAL_WHOLE_STATIC_LIBRARIES := \
	libelf

$(call mesa-build-with-llvm)

include $(MESA_COMMON_MK)
include $(BUILD_STATIC_LIBRARY)

endif # MESA_ENABLE_LLVM == true
