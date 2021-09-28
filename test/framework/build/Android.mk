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

LOCAL_PATH := $(call my-dir)
VTF_MK := $(LOCAL_PATH)/vtf.mk
VTF_PACKAGE_MK := test/vts/tools/build/tasks/framework/vtf_package.mk

include $(CLEAR_VARS)
include $(LOCAL_PATH)/tasks/list/vtslab_apk_package_list.mk
include $(LOCAL_PATH)/tasks/list/vtslab_bin_package_list.mk
include $(LOCAL_PATH)/tasks/list/vtslab_lib_package_list.mk

VTSLAB_OUT_ROOT := $(HOST_OUT)/vtslab
VTSLAB_TESTCASES_OUT := $(VTSLAB_OUT_ROOT)/android-vtslab/testcases
VTSLAB_TOOLS_OUT := $(VTSLAB_OUT_ROOT)/android-vtslab/tools
VTSLAB_BIN_LIB_OUT := $(VTSLAB_OUT_ROOT)/android-vtslab/
VTSLAB_TIMESTAMP := $(shell git -C $(LOCAL_PATH) log -s -n 1 --format="%cd" \
  --date=format:"%Y%m%d_%H%M%S" 2>/dev/null)
VTSLAB_SHORTHASH := $(shell git -C $(LOCAL_PATH) rev-parse --short HEAD 2>/dev/null)
VTSLAB_VERSION := $(VTSLAB_TIMESTAMP):$(VTSLAB_SHORTHASH)

# Packaging rule for android-vtslab.zip
test_suite_name := vtslab
test_suite_readme := test/framework/README.md

include $(LOCAL_PATH)/package.mk
include $(LOCAL_PATH)/utils/vtslab_package_utils.mk

# TODO: instead of an alias, deprecate vtslab build target
.PHONY: lab
lab: vtslab
.PHONY: vtslab
vtslab: $(compatibility_zip)
$(call dist-for-goals, vtslab, $(compatibility_zip))

# Packaging rule for android-vtslab.zip's testcases dir (DATA subdir).
vtslab_apk_modules := \
  $(vtslab_apk_packages) \

vtslab_apk_modules_copy_pairs := \
  $(call target-native-copy-pairs,$(vtslab_apk_modules),$(VTSLAB_TESTCASES_OUT))

# host controller files.
host_hc_files := \
  $(call find-files-in-subdirs,test/framework/harnesses/host_controller,"*.py" -and -type f,.) \

host_hc_copy_pairs := \
  $(foreach f,$(host_hc_files),\
      test/framework/harnesses/host_controller/$(f):$(VTSLAB_TESTCASES_OUT)/host_controller/$(f))

# gsi security patch scripts.
host_hc_gsispl_files := \
  $(call find-files-in-subdirs,test/framework/harnesses/host_controller/gsi,"*.sh" -and -type f,.) \

host_hc_gsispl_copy_pairs := \
  $(foreach f,$(host_hc_gsispl_files),\
    test/framework/harnesses/host_controller/gsi/$(f):$(VTSLAB_BIN_LIB_OUT)/bin/$(f))

# host controller scripts.
host_hc_extra_copy_pairs := \
  test/framework/tools/host_controller/run:$(VTSLAB_TOOLS_OUT)/run \
  test/framework/tools/host_controller/make_screen:$(VTSLAB_TOOLS_OUT)/make_screen \
  test/vts/script/diagnose.py:$(VTSLAB_BIN_LIB_OUT)/bin/diagnose.py \
  test/vts/script/pip_requirements.txt:$(VTSLAB_BIN_LIB_OUT)/bin/pip_requirements.txt \
  test/vts/script/setup.sh:$(VTSLAB_BIN_LIB_OUT)/bin/setup.sh \

host_acloud_files := \
  $(call find-files-in-subdirs,tools/acloud,"*.py" -and -type f,.) \
  $(call find-files-in-subdirs,tools/acloud,"*.config" -and -type f,.)

host_acloud_copy_pairs := \
  $(foreach f,$(host_acloud_files),\
    tools/acloud/$(f):$(VTSLAB_TESTCASES_OUT)/acloud/$(f))

host_vti_dashboard_proto_files := \
  $(call find-files-in-subdirs,test/vti/dashboard/proto,"*.py" -and -type f,.)

host_vti_test_serving_proto_files := \
  $(call find-files-in-subdirs,test/vti/test_serving/proto,"*.py" -and -type f,.)

host_vti_copy_pairs := \
  $(foreach f,$(host_vti_test_serving_proto_files),\
    test/vti/test_serving/proto/$(f):$(VTSLAB_TESTCASES_OUT)/vti/test_serving/proto/$(f)) \
  $(foreach f,$(host_vti_dashboard_proto_files),\
    test/vti/dashboard/proto/$(f):$(VTSLAB_TESTCASES_OUT)/vti/dashboard/proto/$(f)) \
  test/vti/dashboard/__init__.py:$(VTSLAB_TESTCASES_OUT)/vti/dashboard/__init__.py \
  test/vti/test_serving/__init__.py:$(VTSLAB_TESTCASES_OUT)/vti/test_serving/__init__.py \

$(VTSLAB_TESTCASES_OUT)/vti/__init__.py:
	@mkdir -p $(VTSLAB_TESTCASES_OUT)/vti
	@touch $(VTSLAB_TESTCASES_OUT)/vti/__init__.py

host_vti_extra_copy_pairs := \
  $(VTSLAB_TESTCASES_OUT)/vti/__init__.py \

vts_host_python_files := \
  $(call find-files-in-subdirs,test/vts,"*.py" -and -type f,.)

vts_host_python_copy_pairs := \
  $(foreach f,$(vts_host_python_files),\
    test/vts/$(f):$(VTSLAB_TESTCASES_OUT)/vts/$(f))

# Packaging rule for host-controller's dependencies
host_hc_bin_lib := \
  $(vtslab_bin_packages) \
  $(vtslab_lib_packages) \

host_hc_bin_lib_copy_pairs := \
  $(call host-native-copy-pairs,$(host_hc_bin_lib),$(VTSLAB_BIN_LIB_OUT))

vtslab_copy_pairs := \
  $(call copy-many-files,$(vtslab_apk_modules_copy_pairs)) \
  $(call copy-many-files,$(host_hc_copy_pairs)) \
  $(call copy-many-files,$(host_hc_gsispl_copy_pairs)) \
  $(call copy-many-files,$(host_hc_extra_copy_pairs)) \
  $(call copy-many-files,$(host_acloud_copy_pairs)) \
  $(call copy-many-files,$(host_vti_copy_pairs)) \
  $(call copy-many-files,$(vts_host_python_copy_pairs)) \
  $(call copy-many-files,$(host_hc_bin_lib_copy_pairs)) \
  $(host_vti_extra_copy_pairs) \

$(compatibility_zip): $(vtslab_copy_pairs) $(VTSLAB_TESTCASES_OUT)/version.txt

$(VTSLAB_TESTCASES_OUT)/version.txt:
	@rm -f $@
	@echo $(VTSLAB_VERSION) > $@

# for VTF (Vendor Test Framework)
VTF_OUT_ROOT := $(HOST_OUT)/vtf
VTF_TESTCASES_OUT := $(VTF_OUT_ROOT)/android-vtf/testcases
VTF_TOOLS_OUT := $(VTF_OUT_ROOT)/android-vtf/tools
VTF_EXTRA_SCRIPTS := vtf

include $(VTF_PACKAGE_MK)
include $(VTF_MK)

# clears local vars
VTF_MK :=
VTF_PACKAGE_MK :=
