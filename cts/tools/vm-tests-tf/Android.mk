# Copyright (C) 2011 The Android Open Source Project
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

# test dex library
# ============================================================
include $(CLEAR_VARS)

# custom variables used to generate test description. do not touch!
LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_MODULE := cts-tf-dalvik-lib
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE_TAGS := optional
LOCAL_JAVA_LIBRARIES := junit
include $(BUILD_JAVA_LIBRARY)

cts-tf-dalvik-lib.jar := $(full_classes_jar)

# buildutil java library
# ============================================================
include $(CLEAR_VARS)

# custom variables used by cts/tools/utils/CollectAllTests.java to generate test description. do not touch!
LOCAL_TEST_TYPE := vmHostTest
LOCAL_JAR_PATH := android.core.vm-tests-tf.jar

LOCAL_SRC_FILES := $(call all-java-files-under, src build/src)

LOCAL_MODULE := cts-tf-dalvik-buildutil
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE_TAGS := optional

LOCAL_JAVA_LIBRARIES := junit jsr305 d8 smali

LOCAL_CLASSPATH := $(HOST_JDK_TOOLS_JAR)

include $(BUILD_HOST_JAVA_LIBRARY)

# Buid android.core.vm-tests-tf.jar
# ============================================================
#
include $(CLEAR_VARS)

LOCAL_MODULE := vm-tests-tf
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE_SUFFIX := $(COMMON_JAVA_PACKAGE_SUFFIX)
LOCAL_IS_HOST_MODULE := true
LOCAL_BUILT_MODULE_STEM := javalib.jar
intermediates := $(call local-intermediates-dir)
# Install the module as $(intermediates)/android.core.vm-tests-tf.jar.
LOCAL_INSTALLED_MODULE_STEM := android.core.vm-tests-tf.jar
LOCAL_MODULE_PATH := $(intermediates)

# Tag this module as a cts test artifact
LOCAL_COMPATIBILITY_SUITE := cts vts10 general-tests

include $(BUILD_SYSTEM)/base_rules.mk

vmteststf_dep_jars := \
    $(HOST_JDK_TOOLS_JAR) \
    $(cts-tf-dalvik-lib.jar) \
    $(addprefix $(HOST_OUT_JAVA_LIBRARIES)/, cts-tf-dalvik-buildutil.jar junit.jar d8.jar smali.jar) \
    $(call intermediates-dir-for,JAVA_LIBRARIES,cts-vmtests-dot,HOST,COMMON)/classes.jar

vmtests_generated_resources_jar := \
    $(call intermediates-dir-for,JAVA_LIBRARIES,vmtests-generated-resources,HOST)/javalib.jar
vmtests_mains_generated_jar := \
    $(call intermediates-dir-for,JAVA_LIBRARIES,vmtests-mains,,COMMON)/javalib.jar

$(LOCAL_BUILT_MODULE): PRIVATE_SRC_FOLDER := $(LOCAL_PATH)/src
$(LOCAL_BUILT_MODULE): PRIVATE_INTERMEDIATES_CLASSES := $(call intermediates-dir-for,JAVA_LIBRARIES,cts-tf-dalvik-buildutil,HOST)/classes
$(LOCAL_BUILT_MODULE): PRIVATE_INTERMEDIATES := $(intermediates)/tests
$(LOCAL_BUILT_MODULE): PRIVATE_INTERMEDIATES_DEXCORE_JAR := $(intermediates)/tests/dot/junit/dexcore.jar
$(LOCAL_BUILT_MODULE): PRIVATE_CLASS_PATH := $(call normalize-path-list, $(vmteststf_dep_jars))
$(LOCAL_BUILT_MODULE): PRIVATE_VMTESTS_GENERATED_RESOURCES := $(vmtests_generated_resources_jar)
$(LOCAL_BUILT_MODULE): PRIVATE_VMTESTS_MAINS_GENERATED := $(vmtests_mains_generated_jar)
$(LOCAL_BUILT_MODULE) : $(vmteststf_dep_jars) $(HOST_OUT_JAVA_LIBRARIES)/tradefed.jar $(DX) $(vmtests_generated_resources_jar) $(vmtests_mains_generated_jar)
	$(hide) rm -rf $(dir $@) && mkdir -p $(dir $@)
	$(hide) mkdir -p $(dir $(PRIVATE_INTERMEDIATES_DEXCORE_JAR))
	# generated and compile the host side junit tests
	@echo "Write generated Main_*.java"
	$(hide) $(JAVA) \
	    -cp $(PRIVATE_CLASS_PATH) util.build.BuildDalvikSuite $(PRIVATE_SRC_FOLDER) $(PRIVATE_INTERMEDIATES) \
		$(PRIVATE_INTERMEDIATES_CLASSES)
	@echo "Generate $(PRIVATE_INTERMEDIATES_DEXCORE_JAR)"
	$(hide) $(JAR) -cf $(PRIVATE_INTERMEDIATES_DEXCORE_JAR)-class.jar \
		$(addprefix -C $(PRIVATE_INTERMEDIATES_CLASSES) , dot/junit/DxUtil.class dot/junit/DxAbstractMain.class dot/junit/AssertionFailedException.class)
	$(hide) mkdir -p $(PRIVATE_INTERMEDIATES_DEXCORE_JAR).tmp
	$(hide) $(DX_COMMAND) --output $(PRIVATE_INTERMEDIATES_DEXCORE_JAR).tmp \
		$(if $(NO_OPTIMIZE_DX), --debug) $(PRIVATE_INTERMEDIATES_DEXCORE_JAR)-class.jar && rm -f $(PRIVATE_INTERMEDIATES_DEXCORE_JAR).jar
	$(hide) cd $(PRIVATE_INTERMEDIATES_DEXCORE_JAR).tmp && zip -q -r $(abspath $(PRIVATE_INTERMEDIATES_DEXCORE_JAR)) .
	$(hide) cp $(PRIVATE_VMTESTS_GENERATED_RESOURCES) $@
	$(hide) cp $(PRIVATE_VMTESTS_MAINS_GENERATED) $(dir $@)/tests/mains.jar
	$(hide) cd $(dir $@) && zip -q -r $(notdir $@) tests

# Clean up temp vars
intermediates :=
vmteststf_dep_jars :=
vmtests_generated_resources_jar :=
vmtests_mains_generated_jar :=

include $(call all-makefiles-under,$(LOCAL_PATH))
