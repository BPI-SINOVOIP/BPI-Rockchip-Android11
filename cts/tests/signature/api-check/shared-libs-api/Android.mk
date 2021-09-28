# Copyright (C) 2019 The Android Open Source Project
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

all_shared_libs_modules :=

$(foreach ver,$(call int_range_list,28,$(PLATFORM_SDK_VERSION)),\
  $(foreach api_level,public system,\
    $(foreach lib,$(filter-out android,$(filter-out %removed,$(filter-out incompatibilities,\
      $(basename $(notdir $(wildcard $(HISTORICAL_SDK_VERSIONS_ROOT)/$(ver)/$(api_level)/api/*.txt)))))),\
        $(eval all_shared_libs_modules += $(lib)-$(ver)-$(api_level).api))))

all_shared_libs_files := $(addprefix $(COMPATIBILITY_TESTCASES_OUT_cts)/,$(all_shared_libs_modules))

include $(CLEAR_VARS)
LOCAL_MODULE := cts-shared-libs-all.api
LOCAL_MODULE_STEM := shared-libs-all.api.zip
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH = $(TARGET_OUT_DATA_ETC)
include $(BUILD_SYSTEM)/base_rules.mk
$(LOCAL_BUILT_MODULE): $(SOONG_ZIP)
$(LOCAL_BUILT_MODULE): PRIVATE_SHARED_LIBS_FILES := $(all_shared_libs_files)
$(LOCAL_BUILT_MODULE): $(all_shared_libs_files)
	@echo "Zip API files $^ -> $@"
	@mkdir -p $(dir $@)
	$(hide) rm -f $@
	$(hide) $(SOONG_ZIP) -o $@ -P out -C $(OUT_DIR) $(addprefix -f ,$(PRIVATE_SHARED_LIBS_FILES))

all_shared_libs_zip_file := $(LOCAL_BUILT_MODULE)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_MODULE := cts-api-signature-multilib-test

LOCAL_SDK_VERSION := test_current

LOCAL_STATIC_JAVA_LIBRARIES := \
        cts-api-signature-test \
        compatibility-device-util-axt

include $(BUILD_STATIC_JAVA_LIBRARY)


include $(CLEAR_VARS)

LOCAL_PACKAGE_NAME := CtsSharedLibsApiSignatureTestCases

LOCAL_JAVA_RESOURCE_FILES := $(all_shared_libs_zip_file)

LOCAL_STATIC_JAVA_LIBRARIES := cts-api-signature-multilib-test

include $(LOCAL_PATH)/../build_signature_apk.mk

LOCAL_JAVA_SDK_LIBRARIES :=
all_shared_libs_files :=
all_shared_libs_modules :=
all_shared_libs_zip_file :=
