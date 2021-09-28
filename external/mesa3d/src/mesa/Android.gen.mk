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

# included by core mesa Android.mk for source generation

ifeq ($(LOCAL_MODULE_CLASS),)
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
endif

intermediates := $(call local-generated-sources-dir)
prebuilt_intermediates := $(MESA_TOP)/prebuilt-intermediates

# This is the list of auto-generated files: sources and headers
sources := \
	main/enums.c \
	main/api_exec.c \
	main/dispatch.h \
	main/format_fallback.c \
	main/format_pack.c \
	main/format_unpack.c \
	main/format_info.h \
	main/remap_helper.h \
	main/get_hash.h \
	main/marshal_generated0.c \
	main/marshal_generated1.c \
	main/marshal_generated2.c \
	main/marshal_generated3.c \
	main/marshal_generated4.c \
	main/marshal_generated5.c \
	main/marshal_generated6.c \
	main/marshal_generated7.c \
	main/marshal_generated.h

LOCAL_SRC_FILES := $(filter-out $(sources), $(LOCAL_SRC_FILES))

LOCAL_C_INCLUDES += $(intermediates)/main

sources := $(addprefix $(intermediates)/, $(sources))

LOCAL_GENERATED_SOURCES += $(sources)

glapi := $(MESA_TOP)/src/mapi/glapi/gen

dispatch_deps := \
	$(wildcard $(glapi)/*.py) \
	$(wildcard $(glapi)/*.xml)

define es-gen
	@mkdir -p $(dir $@)
	@echo "Gen ES: $(PRIVATE_MODULE) <= $(notdir $(@))"
	$(hide) $(PRIVATE_SCRIPT) $(1) $(PRIVATE_XML) > $@
endef

matypes_deps := \
	$(LOCAL_PATH)/main/mtypes.h \
	$(LOCAL_PATH)/tnl/t_context.h

$(intermediates)/x86/matypes.h: $(prebuilt_intermediates)/x86/matypes.h
	@mkdir -p $(dir $@)
	cp -a $< $@

$(intermediates)/main/dispatch.h: PRIVATE_SCRIPT := $(MESA_PYTHON2) $(glapi)/gl_table.py
$(intermediates)/main/dispatch.h: PRIVATE_XML := -f $(glapi)/gl_and_es_API.xml

$(intermediates)/main/dispatch.h: $(dispatch_deps)
	$(call es-gen, $* -m remap_table)

$(intermediates)/main/remap_helper.h: PRIVATE_SCRIPT := $(MESA_PYTHON2) $(glapi)/remap_helper.py
$(intermediates)/main/remap_helper.h: PRIVATE_XML := -f $(glapi)/gl_and_es_API.xml

$(intermediates)/main/remap_helper.h: $(dispatch_deps)
	$(call es-gen, $*)

$(intermediates)/main/enums.c: PRIVATE_SCRIPT :=$(MESA_PYTHON2) $(glapi)/gl_enums.py
$(intermediates)/main/enums.c: PRIVATE_XML := -f $(glapi)/../registry/gl.xml

$(intermediates)/main/enums.c: $(dispatch_deps)
	$(call es-gen)

$(intermediates)/main/api_exec.c: PRIVATE_SCRIPT := $(MESA_PYTHON2) $(glapi)/gl_genexec.py
$(intermediates)/main/api_exec.c: PRIVATE_XML := -f $(glapi)/gl_and_es_API.xml

$(intermediates)/main/api_exec.c: $(dispatch_deps)
	$(call es-gen)

$(intermediates)/main/marshal_generated0.c: PRIVATE_SCRIPT := $(MESA_PYTHON2) $(glapi)/gl_marshal.py
$(intermediates)/main/marshal_generated0.c: PRIVATE_XML := -f $(glapi)/gl_and_es_API.xml -i 0 -n 8

$(intermediates)/main/marshal_generated0.c: $(dispatch_deps)
	$(call es-gen)

$(intermediates)/main/marshal_generated1.c: PRIVATE_SCRIPT := $(MESA_PYTHON2) $(glapi)/gl_marshal.py
$(intermediates)/main/marshal_generated1.c: PRIVATE_XML := -f $(glapi)/gl_and_es_API.xml -i 1 -n 8

$(intermediates)/main/marshal_generated1.c: $(dispatch_deps)
	$(call es-gen)

$(intermediates)/main/marshal_generated2.c: PRIVATE_SCRIPT := $(MESA_PYTHON2) $(glapi)/gl_marshal.py
$(intermediates)/main/marshal_generated2.c: PRIVATE_XML := -f $(glapi)/gl_and_es_API.xml -i 2 -n 8

$(intermediates)/main/marshal_generated2.c: $(dispatch_deps)
	$(call es-gen)

$(intermediates)/main/marshal_generated3.c: PRIVATE_SCRIPT := $(MESA_PYTHON2) $(glapi)/gl_marshal.py
$(intermediates)/main/marshal_generated3.c: PRIVATE_XML := -f $(glapi)/gl_and_es_API.xml -i 3 -n 8

$(intermediates)/main/marshal_generated3.c: $(dispatch_deps)
	$(call es-gen)

$(intermediates)/main/marshal_generated4.c: PRIVATE_SCRIPT := $(MESA_PYTHON2) $(glapi)/gl_marshal.py
$(intermediates)/main/marshal_generated4.c: PRIVATE_XML := -f $(glapi)/gl_and_es_API.xml -i 4 -n 8

$(intermediates)/main/marshal_generated4.c: $(dispatch_deps)
	$(call es-gen)

$(intermediates)/main/marshal_generated5.c: PRIVATE_SCRIPT := $(MESA_PYTHON2) $(glapi)/gl_marshal.py
$(intermediates)/main/marshal_generated5.c: PRIVATE_XML := -f $(glapi)/gl_and_es_API.xml -i 5 -n 8

$(intermediates)/main/marshal_generated5.c: $(dispatch_deps)
	$(call es-gen)

$(intermediates)/main/marshal_generated6.c: PRIVATE_SCRIPT := $(MESA_PYTHON2) $(glapi)/gl_marshal.py
$(intermediates)/main/marshal_generated6.c: PRIVATE_XML := -f $(glapi)/gl_and_es_API.xml -i 6 -n 8

$(intermediates)/main/marshal_generated6.c: $(dispatch_deps)
	$(call es-gen)

$(intermediates)/main/marshal_generated7.c: PRIVATE_SCRIPT := $(MESA_PYTHON2) $(glapi)/gl_marshal.py
$(intermediates)/main/marshal_generated7.c: PRIVATE_XML := -f $(glapi)/gl_and_es_API.xml -i 7 -n 8

$(intermediates)/main/marshal_generated7.c: $(dispatch_deps)
	$(call es-gen)

$(intermediates)/main/marshal_generated.h: PRIVATE_SCRIPT := $(MESA_PYTHON2) $(glapi)/gl_marshal_h.py
$(intermediates)/main/marshal_generated.h: PRIVATE_XML := -f $(glapi)/gl_and_es_API.xml

$(intermediates)/main/marshal_generated.h: $(dispatch_deps)
	$(call es-gen)

GET_HASH_GEN := $(LOCAL_PATH)/main/get_hash_generator.py

$(intermediates)/main/get_hash.h: PRIVATE_SCRIPT := $(MESA_PYTHON2) $(GET_HASH_GEN)
$(intermediates)/main/get_hash.h: PRIVATE_XML := -f $(glapi)/gl_and_es_API.xml
$(intermediates)/main/get_hash.h: $(glapi)/gl_and_es_API.xml \
               $(LOCAL_PATH)/main/get_hash_params.py $(GET_HASH_GEN)
	$(call es-gen)

$(intermediates)/main/format_fallback.c: $(prebuilt_intermediates)/main/format_fallback.c
	cp -a $< $@

FORMAT_INFO := $(LOCAL_PATH)/main/format_info.py
format_info_deps := \
	$(LOCAL_PATH)/main/formats.csv \
	$(LOCAL_PATH)/main/format_parser.py \
	$(FORMAT_INFO)

$(intermediates)/main/format_info.h: PRIVATE_SCRIPT := $(MESA_PYTHON2) $(FORMAT_INFO)
$(intermediates)/main/format_info.h: PRIVATE_XML :=
$(intermediates)/main/format_info.h: $(format_info_deps)
	$(call es-gen, $<)

$(intermediates)/main/format_pack.c: $(prebuilt_intermediates)/main/format_pack.c
	cp -a $< $@

$(intermediates)/main/format_unpack.c: $(prebuilt_intermediates)/main/format_unpack.c
	cp -a $< $@


