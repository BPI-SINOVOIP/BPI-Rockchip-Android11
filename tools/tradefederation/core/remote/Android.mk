# Copyright (C) 2013 The Android Open Source Project
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

# makefile rules to copy jars to HOST_OUT/tradefed
# so tradefed.sh can automatically add to classpath
DEST_JAR := $(HOST_OUT)/tradefed/tf-remote-client.jar
BUILT_JAR := $(call intermediates-dir-for,JAVA_LIBRARIES,tf-remote-client,HOST)/javalib.jar
$(DEST_JAR): $(BUILT_JAR)
	$(copy-file-to-new-target)

# this dependency ensure the above rule will be executed if jar is built
$(HOST_OUT_JAVA_LIBRARIES)/tf-remote-client.jar : $(DEST_JAR)
