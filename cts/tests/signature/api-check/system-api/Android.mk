# Copyright (C) 2017 The Android Open Source Project
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

all_system_api_modules :=
$(foreach ver,$(PLATFORM_SYSTEMSDK_VERSIONS),\
  $(if $(call math_is_number,$(ver)),\
    $(if $(wildcard prebuilts/sdk/$(ver)/system/api/android.txt),\
      $(eval all_system_api_modules += system-$(ver).api)\
    )\
  )\
)
all_system_api_files := $(addprefix $(COMPATIBILITY_TESTCASES_OUT_cts)/,$(all_system_api_modules))

include $(CLEAR_VARS)
LOCAL_MODULE := cts-system-all.api
LOCAL_MODULE_STEM := system-all.api.zip
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH = $(TARGET_OUT_DATA_ETC)
LOCAL_COMPATIBILITY_SUITE := arcts cts vts10 general-tests
include $(BUILD_SYSTEM)/base_rules.mk
$(LOCAL_BUILT_MODULE): $(SOONG_ZIP)
$(LOCAL_BUILT_MODULE): PRIVATE_SYSTEM_API_FILES := $(all_system_api_files)
$(LOCAL_BUILT_MODULE): $(all_system_api_files)
	@echo "Zip API files $^ -> $@"
	@mkdir -p $(dir $@)
	$(hide) rm -f $@
	$(hide) $(SOONG_ZIP) -o $@ -P out -C $(OUT_DIR) $(addprefix -f ,$(PRIVATE_SYSTEM_API_FILES))

all_system_api_zip_file := $(LOCAL_BUILT_MODULE)

include $(CLEAR_VARS)

LOCAL_PACKAGE_NAME := CtsSystemApiSignatureTestCases

LOCAL_JAVA_RESOURCE_FILES := $(all_system_api_zip_file)

LOCAL_SIGNATURE_API_FILES := \
    current.api.gz \
    android-test-mock-current.api.gz \
    android-test-runner-current.api.gz \
    system-current.api.gz \
    system-removed.api.gz \

include $(LOCAL_PATH)/../build_signature_apk.mk

all_system_api_files :=
all_system_api_modules :=
all_system_api_zip_file :=
