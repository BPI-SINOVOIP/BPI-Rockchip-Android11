#
# Copyright (C) 2018 The Android Open Source Project
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

# Build rules for VTF (Vendor Test Framework).
#
# It is designed as a customizable build rule where its callers can provide
# specifications. Before including it, please set the following variables:
#
# - VTF_OUT_ROOT for the base path of a local output directory base.
# - VTF_TESTCASES_OUT for the testcases sub directory of its output zip file.
# - VTF_TOOLS_OUT for tools sub directory of its output zip file.
# - VTF_EXTRA_SCRIPTS for the file name of any extra script to include.

build_list_dir := test/vts/tools/build/tasks/list
build_utils_dir := test/vts/tools/build/utils

include $(build_list_dir)/vts_apk_package_list.mk
include $(build_list_dir)/vts_bin_package_list.mk
include $(build_list_dir)/vts_lib_package_list.mk
include $(build_utils_dir)/vts_package_utils.mk

# Packaging rule for android-vts10.zip's testcases dir (DATA subdir).
vtf_target_native_modules := \
    $(vts_apk_packages) \
    $(vts_bin_packages) \
    $(vts_lib_packages) \

vtf_target_native_copy_pairs := \
  $(call target-native-copy-pairs,$(vtf_target_native_modules),$(VTF_TESTCASES_OUT))

# Packaging rule for host-side test native packages

target_hostdriven_modules := \
  $(vts_test_host_lib_packages) \
  $(vts_test_host_bin_packages) \

target_hostdriven_copy_pairs := \
  $(call host-native-copy-pairs,$(target_hostdriven_modules),$(VTF_TESTCASES_OUT))

host_additional_deps_copy_pairs := \
  test/vts/tools/vts-tradefed/etc/vts10-tradefed_win.bat:$(VTF_TOOLS_OUT)/vts10-tradefed_win.bat \
  test/vts/tools/vts-tradefed/DynamicConfig.xml:$(VTF_TESTCASES_OUT)/cts.dynamic

# Packaging rule for host-side Python logic, configs, and data files

host_framework_files := \
  $(call find-files-in-subdirs,test/vts,"*.py" -and -type f,.) \
  $(call find-files-in-subdirs,test/vts,"*.runner_conf" -and -type f,.) \
  $(call find-files-in-subdirs,test/vts,"*.push" -and -type f,.)

host_framework_copy_pairs := \
  $(foreach f,$(host_framework_files),\
    test/vts/$(f):$(VTF_TESTCASES_OUT)/vts/$(f))

host_testcase_files := \
  $(call find-files-in-subdirs,test/vts-testcase,"*.py" -and -type f,.) \
  $(call find-files-in-subdirs,test/vts-testcase,"*.runner_conf" -and -type f,.) \
  $(call find-files-in-subdirs,test/vts-testcase,"*.push" -and -type f,.) \
  $(call find-files-in-subdirs,test/vts-testcase,"*.dump" -and -type f,.)

host_testcase_copy_pairs := \
  $(foreach f,$(host_testcase_files),\
    test/vts-testcase/$(f):$(VTF_TESTCASES_OUT)/vts/testcases/$(f))

vts_test_core_copy_pairs := \
  $(call copy-many-files,$(host_testcase_copy_pairs)) \

ifneq ($(TARGET_BUILD_PDK),true)

host_camera_its_files := \
  $(call find-files-in-subdirs,cts/apps/CameraITS,"*.py" -and -type f,.) \
  $(call find-files-in-subdirs,cts/apps/CameraITS,"*.pdf" -and -type f,.) \
  $(call find-files-in-subdirs,cts/apps/CameraITS,"*.png" -and -type f,.)

host_camera_its_copy_pairs := \
  $(foreach f,$(host_camera_its_files),\
    cts/apps/CameraITS/$(f):$(VTF_TESTCASES_OUT)/CameraITS/$(f))

else

host_camera_its_copy_pairs :=

endif  # ifneq ($(TARGET_BUILD_PDK),true)

host_systrace_files := \
  $(filter-out .git/%, \
    $(call find-files-in-subdirs,external/chromium-trace,"*" -and -type f,.))

host_systrace_copy_pairs := \
  $(foreach f,$(host_systrace_files),\
    external/chromium-trace/$(f):$(VTF_TOOLS_OUT)/external/chromium-trace/$(f))

target_script_files := \
  $(call find-files-in-subdirs,test/vts/script/target,"*.sh" -and -type f,.)

target_script_copy_pairs := \
  $(foreach f,$(target_script_files),\
    test/vts/script/target/$(f):$(VTF_TESTCASES_OUT)/script/target/$(f))

vtf_copy_pairs := \
  $(call copy-many-files,$(host_framework_copy_pairs)) \
  $(call copy-many-files,$(host_additional_deps_copy_pairs)) \
  $(call copy-many-files,$(host_systrace_copy_pairs)) \
  $(call copy-many-files,$(host_camera_its_copy_pairs)) \
  $(call copy-many-files,$(vtf_target_native_copy_pairs)) \
  $(call copy-many-files,$(target_hostdriven_copy_pairs)) \
  $(call copy-many-files,$(target_hal_hash_copy_pairs)) \
  $(call copy-many-files,$(target_script_copy_pairs)) \

.PHONY: vts-test-core
vts-test-core: $(vts_test_core_copy_pairs) $(vtf_copy_pairs)
