# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Build all sub-directories
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

gamecore_dist_host_jar := GameCoreHelperHost GameCoreHostTestCases GameCorePerformanceTest truth-prebuilt
gamecore_dist_test_exe := GameCoreNativeTestCases
gamecore_dist_test_apk := GameCoreDevice GameCoreSampleApp GameCoreJavaTestCases GameCoreAllocStress

tradefed_jars := tradefed tools-common-prebuilt
tradefed_files := \
    tools/tradefederation/core/tradefed.sh \
    tools/tradefederation/core/tradefed_win.bat  \
    tools/tradefederation/core/script_help.sh \

config_files := \
    $(LOCAL_PATH)/AndroidTest.xml \
    $(LOCAL_PATH)/dist/certification-tests.xml \
    $(LOCAL_PATH)/dist/run_gamecore.sh \
    $(LOCAL_PATH)/dist/README

bit_suffix := $(if $(TARGET_IS_64_BIT),64,32)

gamecore_dist_copy_pairs := $(foreach m, $(gamecore_dist_host_jar) $(tradefed_jars), \
    $(call intermediates-dir-for,JAVA_LIBRARIES,$(m),HOST,COMMON)/javalib.jar:gamecore/bin/$(m).jar)
gamecore_dist_copy_pairs += $(foreach m, $(gamecore_dist_test_exe), \
    $(call intermediates-dir-for,NATIVE_TESTS,$(m))/$(m)$(bit_suffix):gamecore/testcases/$(TARGET_ARCH)/$(m)$(bit_suffix))
gamecore_dist_copy_pairs += $(foreach m, $(gamecore_dist_test_apk), \
    $(call intermediates-dir-for,APPS,$(m))/package.apk:gamecore/testcases/$(m).apk)
gamecore_dist_copy_pairs += $(foreach f, $(tradefed_files),$(f):gamecore/bin/$(notdir $(f)))
gamecore_dist_copy_pairs += $(foreach f, $(config_files),$(f):gamecore/$(notdir $(f)))

gamecore_dist_host_jar :=
gamecore_dist_test_exe :=
gamecore_dist_test_apk :=
tradefed_jars :=
tradefed_files :=
config_files :=
bit_suffix :=

gamecore_dist_intermediates := $(call intermediates-dir-for,PACKAGING,gamecore_dist,HOST,COMMON)
gamecore_dist_zip := $(gamecore_dist_intermediates)/gamecore.zip
$(gamecore_dist_zip) : PRIVATE_COPY_PAIRS := $(gamecore_dist_copy_pairs)
$(gamecore_dist_zip) : $(SOONG_ZIP) $(foreach p,$(gamecore_dist_copy_pairs),$(call word-colon,1,$(p)))
	rm -rf $(dir $@) && mkdir -p $(dir $@)/gamecore
	mkdir -p $(dir $@)/gamecore/bin
	mkdir -p $(dir $@)/gamecore/testcases/$(TARGET_ARCH)
	$(foreach p,$(PRIVATE_COPY_PAIRS), \
	  cp -f $(call word-colon,1,$(p)) $(dir $@)/$(call word-colon,2,$(p)) &&) true
	echo $(BUILD_NUMBER_FROM_FILE) > $(dir $@)/gamecore/version.txt
	$(SOONG_ZIP) -o $@ -C $(dir $@) -f $(dir $@)/gamecore/version.txt \
	  $(foreach p,$(PRIVATE_COPY_PAIRS),-f $(dir $@)/$(call word-colon,2,$(p)))

.PHONY: gamecore
gamecore: $(gamecore_dist_host_jar) $(gamecore_dist_test_apk)

.PHONY: gamecore-test
gamecore-test: GameCorePerformanceTestTest GameCoreHelperTest

.PHONY: gamecore-all
gamecore-all: gamecore gamecore-test

$(call dist-for-goals, gamecore, $(gamecore_dist_zip))

gamecore_dist_copy_pairs :=
gamecore_dist_intermediates :=
gamecore_dist_zip :=

include $(call all-makefiles-under,$(LOCAL_PATH))
