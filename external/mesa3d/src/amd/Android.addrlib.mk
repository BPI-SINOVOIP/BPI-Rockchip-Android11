# Copyright © 2016 Red Hat.
# Copyright © 2016 Mauro Rossi <issor.oruam@gmail.com>
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

# ---------------------------------------
# Build libmesa_amdgpu_addrlib
# ---------------------------------------

include $(CLEAR_VARS)

LOCAL_MODULE := libmesa_amdgpu_addrlib
LOCAL_LICENSE_KINDS := SPDX-license-identifier-ISC SPDX-license-identifier-MIT
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/../../LICENSE

LOCAL_SRC_FILES := $(ADDRLIB_FILES)

LOCAL_C_INCLUDES := \
	$(MESA_TOP)/src \
	$(MESA_TOP)/src/amd/common \
	$(MESA_TOP)/src/amd/addrlib/inc \
	$(MESA_TOP)/src/amd/addrlib/src \
	$(MESA_TOP)/src/amd/addrlib/src/core \
	$(MESA_TOP)/src/amd/addrlib/src/chip/gfx9 \
	$(MESA_TOP)/src/amd/addrlib/src/chip/gfx10 \
	$(MESA_TOP)/src/amd/addrlib/src/chip/r800

LOCAL_EXPORT_C_INCLUDE_DIRS := \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/addrlib/core \
	$(LOCAL_PATH)/addrlib/inc/chip/r800 \
	$(LOCAL_PATH)/addrlib/r800/chip

include $(MESA_COMMON_MK)
include $(BUILD_STATIC_LIBRARY)
