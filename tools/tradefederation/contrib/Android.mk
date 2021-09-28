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
COMPATIBILITY.tradefed_tests_dir := \
  $(COMPATIBILITY.tradefed_tests_dir) $(LOCAL_PATH)/res/config

# makefile rules to copy jars to HOST_OUT/tradefed
# so tradefed.sh can automatically add to classpath
DEST_JAR := $(HOST_OUT)/tradefed/tradefed-contrib.jar
BUILT_JAR := $(call intermediates-dir-for,JAVA_LIBRARIES,tradefed-contrib,HOST)/javalib.jar
$(DEST_JAR): $(BUILT_JAR)
	$(copy-file-to-new-target)

# this dependency ensure the above rule will be executed if jar is built
$(HOST_OUT_JAVA_LIBRARIES)/tradefed-contrib.jar : $(DEST_JAR)

# Build all sub-directories
include $(call all-makefiles-under,$(LOCAL_PATH))
