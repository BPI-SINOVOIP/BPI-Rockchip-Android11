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

include $(CLEAR_VARS)
LOCAL_MODULE := cts_signature_query_service
LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_SRC_FILES += $(call all-Iaidl-files-under, src)
LOCAL_SDK_VERSION := current
include $(BUILD_STATIC_JAVA_LIBRARY)

# This is the first version of the test app signed with the initial signing
# key. This app exports the bound service from the cts_signature_query_service
# library and is used to verify end to end updates with key rotation.
include $(CLEAR_VARS)
LOCAL_PACKAGE_NAME := CtsSignatureQueryService
LOCAL_SDK_VERSION := current
LOCAL_STATIC_JAVA_LIBRARIES := cts_signature_query_service
LOCAL_COMPATIBILITY_SUITE := cts vts10 general-tests
LOCAL_CERTIFICATE := $(cert_dir)/ec-p256
include $(BUILD_CTS_SUPPORT_PACKAGE)

# This is the second version of the test app signed with the rotated signing
# key with an updated version number. This app is intended to verify that an
# app continues to function as expected after an update with a rotated key.
include $(CLEAR_VARS)
LOCAL_PACKAGE_NAME := CtsSignatureQueryService_v2
LOCAL_MANIFEST_FILE = AndroidManifest_v2.xml
LOCAL_SDK_VERSION := current
LOCAL_STATIC_JAVA_LIBRARIES := cts_signature_query_service
LOCAL_COMPATIBILITY_SUITE := cts vts10 general-tests
LOCAL_CERTIFICATE := $(cert_dir)/ec-p256_2
LOCAL_ADDITIONAL_CERTIFICATES := $(cert_dir)/ec-p256
LOCAL_CERTIFICATE_LINEAGE := $(cert_dir)/ec-p256-por_1_2-default-caps
include $(BUILD_CTS_SUPPORT_PACKAGE)

# This is the third version of the test app signed with the same rotated
# signing key as v2. This app is intended to verify that an app can still
# be updated and function as expected after the signing key has been rotated.
include $(CLEAR_VARS)
LOCAL_PACKAGE_NAME := CtsSignatureQueryService_v3
LOCAL_MANIFEST_FILE = AndroidManifest_v3.xml
LOCAL_SDK_VERSION := current
LOCAL_STATIC_JAVA_LIBRARIES := cts_signature_query_service
LOCAL_COMPATIBILITY_SUITE := cts vts10 general-tests
LOCAL_CERTIFICATE := $(cert_dir)/ec-p256_2
LOCAL_ADDITIONAL_CERTIFICATES := $(cert_dir)/ec-p256
LOCAL_CERTIFICATE_LINEAGE := $(cert_dir)/ec-p256-por_1_2-default-caps
include $(BUILD_CTS_SUPPORT_PACKAGE)

cert_dir :=
include $(call all-makefiles-under,$(LOCAL_PATH))

