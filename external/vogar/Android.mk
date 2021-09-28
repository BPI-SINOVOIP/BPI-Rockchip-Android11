# Copyright (C) 2014 The Android Open Source Project
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

# build vogar jar
# ============================================================

include $(CLEAR_VARS)

LOCAL_MODULE := vogar
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_SRC_FILES := $(call all-java-files-under, src/)
LOCAL_JAVA_RESOURCE_DIRS := resources

LOCAL_STATIC_JAVA_LIBRARIES := \
  caliper \
  caliper-gson \
  guavalib \
  junit \
  vogar-jsr305 \
  vogar-kxml-libcore-20110123

LOCAL_ADDITIONAL_DEPENDENCIES := \
  $(HOST_OUT_EXECUTABLES)/dx \
  $(HOST_OUT_EXECUTABLES)/d8 \
  $(HOST_OUT_JAVA_LIBRARIES)/desugar.jar

# Vogar uses android.jar.
LOCAL_CLASSPATH := $(call resolve-prebuilt-sdk-jar-path,current)

include $(BUILD_HOST_JAVA_LIBRARY)

# build vogar tests jar
# ============================================================
# Run the tests using using the following target.
.PHONY: run-vogar-tests
run-vogar-tests: vogar-tests
	ANDROID_BUILD_TOP=$$(pwd) \
	  java -cp ./out/host/linux-x86/framework/vogar-tests.jar \
	  org.junit.runner.JUnitCore vogar.AllTests

include $(CLEAR_VARS)

LOCAL_MODULE := vogar-tests
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_SRC_FILES := $(call all-java-files-under, test/)

LOCAL_STATIC_JAVA_LIBRARIES := \
	junit \
	mockito \
	objenesis \
	vogar

include $(BUILD_HOST_JAVA_LIBRARY)

# copy vogar script
# ============================================================
include $(CLEAR_VARS)
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE := vogar
LOCAL_SRC_FILES := bin/vogar-android
include $(BUILD_PREBUILT)

