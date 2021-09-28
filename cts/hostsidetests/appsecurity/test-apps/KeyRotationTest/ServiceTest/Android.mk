#
# Copyright (C) 2020 The Android Open Source Project
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
#

LOCAL_PATH := $(call my-dir)

cert_dir := cts/hostsidetests/appsecurity/certs/pkgsigverify

# This is the instrumentation test package for the CtsSignatureQueryService
# app. This app verifies that the standalone app is functioning as expected
# after a key rotation and provides a companion package that can be used for
# the PackageManager checkSignatures APIs.
include $(CLEAR_VARS)
LOCAL_PACKAGE_NAME := CtsSignatureQueryServiceTest
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_STATIC_JAVA_LIBRARIES := cts_signature_query_service androidx.test.core androidx.test.rules
LOCAL_JAVA_LIBRARIES := android.test.runner.stubs android.test.base.stubs
LOCAL_COMPATIBILITY_SUITE := cts vts10 general-tests
LOCAL_CERTIFICATE := $(cert_dir)/ec-p256
include $(BUILD_CTS_SUPPORT_PACKAGE)

# This is the instrumentation test package signed with the same signing key and
# lineage as v2 and v3 of the CtsSignatureQueryService test app.
include $(CLEAR_VARS)
LOCAL_PACKAGE_NAME := CtsSignatureQueryServiceTest_v2
LOCAL_SDK_VERSION := current
LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_STATIC_JAVA_LIBRARIES := cts_signature_query_service androidx.test.core androidx.test.rules
LOCAL_JAVA_LIBRARIES := android.test.runner.stubs android.test.base.stubs
LOCAL_COMPATIBILITY_SUITE := cts vts10 general-tests
LOCAL_CERTIFICATE := $(cert_dir)/ec-p256_2
LOCAL_ADDITIONAL_CERTIFICATES := $(cert_dir)/ec-p256
LOCAL_CERTIFICATE_LINEAGE := $(cert_dir)/ec-p256-por_1_2-default-caps
include $(BUILD_CTS_SUPPORT_PACKAGE)

cert_dir :=

