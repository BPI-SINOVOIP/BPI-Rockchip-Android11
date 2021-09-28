# Copyright (C) 2018 Google Inc.
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

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := cts-dynamic-config
LOCAL_MODULE_CLASS := FAKE
LOCAL_IS_HOST_MODULE := true

LOCAL_COMPATIBILITY_SUITE := cts general-tests vts10 mts

# my_test_config_file := DynamicConfig.xml
# TODO (sbasi): Update to use BUILD_HOST_TEST_CONFIG when it's primary install
# location is the test case directory.
# include $(BUILD_HOST_TEST_CONFIG)

include $(BUILD_SYSTEM)/base_rules.mk

$(LOCAL_BUILT_MODULE): $(HOST_OUT_TESTCASES)/$(LOCAL_MODULE)/$(LOCAL_MODULE).dynamic
	@echo "cts-dynamic-config values: $(LOCAL_MODULE)"
	$(hide) touch $@
