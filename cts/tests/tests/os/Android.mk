# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# platform version check (b/32056228)
# ============================================================
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := cts-platform-version-check
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA_APPS)
# Tag this module as a cts test artifact
LOCAL_COMPATIBILITY_SUITE := cts vts10 general-tests

cts_platform_version_path := cts/tests/tests/os/assets/platform_versions.txt
cts_platform_version_string := $(shell cat $(cts_platform_version_path))
cts_platform_release_path := cts/tests/tests/os/assets/platform_releases.txt
cts_platform_release_string := $(shell cat $(cts_platform_release_path))

include $(BUILD_SYSTEM)/base_rules.mk

$(LOCAL_BUILT_MODULE) : $(cts_platform_version_path) build/core/version_defaults.mk
	$(hide) if [ -z "$(findstring $(PLATFORM_VERSION),$(cts_platform_version_string))" ]; then \
		echo "============================================================" 1>&2; \
		echo "Could not find version \"$(PLATFORM_VERSION)\" in CTS platform version file:" 1>&2; \
		echo "" 1>&2; \
		echo "	$(cts_platform_version_path)" 1>&2; \
		echo "" 1>&2; \
		echo "Most likely PLATFORM_VERSION in build/core/version_defaults.mk" 1>&2; \
		echo "has changed and a new version must be added to this CTS file." 1>&2; \
		echo "============================================================" 1>&2; \
		exit 1; \
	fi
	$(hide) if [ -z "$(findstring $(PLATFORM_VERSION_LAST_STABLE),$(cts_platform_release_string))" ]; then \
		echo "============================================================" 1>&2; \
		echo "Could not find version \"$(PLATFORM_VERSION_LAST_STABLE)\" in CTS platform release file:" 1>&2; \
		echo "" 1>&2; \
		echo "	$(cts_platform_release_path)" 1>&2; \
		echo "" 1>&2; \
		echo "Most likely PLATFORM_VERSION_LAST_STABLE in build/core/version_defaults.mk" 1>&2; \
		echo "has changed and a new version must be added to this CTS file." 1>&2; \
		echo "============================================================" 1>&2; \
		exit 1; \
	fi
	@mkdir -p $(dir $@)
	echo $(cts_platform_version_string) > $@
