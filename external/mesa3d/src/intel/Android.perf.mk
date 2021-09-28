# Copyright © 2018 Intel Corporation
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
# Build libmesa_intel_perf
# ---------------------------------------

include $(CLEAR_VARS)

LOCAL_MODULE := libmesa_intel_perf
LOCAL_LICENSE_KINDS := SPDX-license-identifier-ISC SPDX-license-identifier-MIT legacy_by_exception_only legacy_notice
LOCAL_LICENSE_CONDITIONS := by_exception_only notice
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/../../LICENSE

LOCAL_MODULE_CLASS := STATIC_LIBRARIES

intermediates := $(call local-generated-sources-dir)

LOCAL_C_INCLUDES := $(MESA_TOP)/include/drm-uapi

LOCAL_SRC_FILES := $(GEN_PERF_FILES)

LOCAL_GENERATED_SOURCES += $(addprefix $(intermediates)/, \
	$(GEN_PERF_GENERATED_FILES))

$(intermediates)/perf/gen_perf_metrics.c: $(prebuilt_intermediates)/perf/gen_perf_metrics.c
	@mkdir -p $(dir $@)
	@cp -f $< $@

$(intermediates)/perf/gen_perf_metrics.h: $(prebuilt_intermediates)/perf/gen_perf_metrics.h
	@mkdir -p $(dir $@)
	@cp -f $< $@


include $(MESA_COMMON_MK)
include $(BUILD_STATIC_LIBRARY)
