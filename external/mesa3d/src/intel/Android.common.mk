# Copyright © 2016 Intel Corporation
# Copyright © 2016 Mauro Rossi <issor.oruam@gmail.com>
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

# ---------------------------------------
# Build libmesa_intel_common
# ---------------------------------------

include $(CLEAR_VARS)

LOCAL_MODULE := libmesa_intel_common
LOCAL_LICENSE_KINDS := SPDX-license-identifier-ISC SPDX-license-identifier-MIT legacy_by_exception_only legacy_notice
LOCAL_LICENSE_CONDITIONS := by_exception_only notice
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/../../LICENSE

LOCAL_MODULE_CLASS := STATIC_LIBRARIES

LOCAL_SRC_FILES := $(COMMON_FILES)

LOCAL_C_INCLUDES := \
	external/zlib \
	$(MESA_TOP)/src/gallium/include \
	$(MESA_TOP)/src/gallium/auxiliary \
	$(MESA_TOP)/src/mapi \
	$(MESA_TOP)/src/mesa \
	$(call generated-sources-dir-for,STATIC_LIBRARIES,libmesa_git_sha1,,)

LOCAL_SHARED_LIBRARIES := libz liblog

# If Android version >=8 MESA should static link libexpat else should dynamic link
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 27; echo $$?), 0)
LOCAL_STATIC_LIBRARIES := \
	libexpat
else
LOCAL_SHARED_LIBRARIES += \
	libexpat
endif


LOCAL_WHOLE_STATIC_LIBRARIES := libmesa_genxml

include $(MESA_COMMON_MK)
include $(BUILD_STATIC_LIBRARY)
