# Copyright (C) 2012 The Android Open Source Project
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

# makefile rules to copy jars to HOST_OUT/tradefed
# so tradefed.sh can automatically add to classpath

DEST_JAR := $(HOST_OUT)/tradefed/tf-contrib-tests.jar
BUILT_JAR := $(call intermediates-dir-for,JAVA_LIBRARIES,tf-contrib-tests,HOST)/javalib.jar
$(DEST_JAR): $(BUILT_JAR)
	$(copy-file-to-new-target)

# this dependency ensure the above rule will be executed if module is built
$(HOST_OUT_JAVA_LIBRARIES)/tf-contrib-tests.jar : $(DEST_JAR)
