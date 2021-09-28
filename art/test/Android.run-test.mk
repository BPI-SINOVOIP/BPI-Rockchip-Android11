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
#

LOCAL_PATH := $(call my-dir)

include art/build/Android.common_test.mk

# Dependencies for actually running a run-test.
TEST_ART_RUN_TEST_DEPENDENCIES := \
  $(HOST_OUT_EXECUTABLES)/dx \
  $(HOST_OUT_EXECUTABLES)/d8 \
  $(HOST_OUT_EXECUTABLES)/hiddenapi \
  $(HOST_OUT_EXECUTABLES)/jasmin \
  $(HOST_OUT_EXECUTABLES)/smali

# We need the ART Testing APEX (which is a superset of the Release
# and Debug APEXes) -- which contains dex2oat, dalvikvm, their
# dependencies and ART gtests -- on the target, as well as the core
# images (all images as we sync only once).
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES := $(TESTING_ART_APEX) $(TARGET_CORE_IMG_OUTS)

# Also need libartagent.
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += libartagent-target libartagentd-target

# Also need libtiagent.
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += libtiagent-target libtiagentd-target

# Also need libtistress.
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += libtistress-target libtistressd-target

# Also need libarttest.
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += libarttest-target libarttestd-target

# Also need libnativebridgetest.
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += libnativebridgetest-target libnativebridgetestd-target

# Also need signal_dumper.
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += signal_dumper-target

ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += \
  $(foreach jar,$(TARGET_TEST_CORE_JARS),$(TARGET_OUT_JAVA_LIBRARIES)/$(jar).jar)

# All tests require the host executables. The tests also depend on the core images, but on
# specific version depending on the compiler.
ART_TEST_HOST_RUN_TEST_DEPENDENCIES := \
  $(ART_HOST_EXECUTABLES) \
  $(HOST_OUT_EXECUTABLES)/hprof-conv \
  $(HOST_OUT_EXECUTABLES)/signal_dumper \
  $(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libtiagent) \
  $(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libtiagentd) \
  $(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libtistress) \
  $(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libtistressd) \
  $(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libartagent) \
  $(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libartagentd) \
  $(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libarttest) \
  $(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libarttestd) \
  $(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libnativebridgetest) \
  $(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libnativebridgetestd) \
  $(ART_HOST_OUT_SHARED_LIBRARIES)/libicu_jni$(ART_HOST_SHLIB_EXTENSION) \
  $(ART_HOST_OUT_SHARED_LIBRARIES)/libjavacore$(ART_HOST_SHLIB_EXTENSION) \
  $(ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdk$(ART_HOST_SHLIB_EXTENSION) \
  $(ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdkd$(ART_HOST_SHLIB_EXTENSION) \
  $(ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdkjvmti$(ART_HOST_SHLIB_EXTENSION) \
  $(ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdkjvmtid$(ART_HOST_SHLIB_EXTENSION) \
  $(HOST_CORE_DEX_LOCATIONS) \

ifneq ($(HOST_PREFER_32_BIT),true)
ART_TEST_HOST_RUN_TEST_DEPENDENCIES += \
  $(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libtiagent) \
  $(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libtiagentd) \
  $(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libtistress) \
  $(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libtistressd) \
  $(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libartagent) \
  $(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libartagentd) \
  $(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libarttest) \
  $(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libarttestd) \
  $(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libnativebridgetest) \
  $(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libnativebridgetestd) \
  $(2ND_ART_HOST_OUT_SHARED_LIBRARIES)/libicu_jni$(ART_HOST_SHLIB_EXTENSION) \
  $(2ND_ART_HOST_OUT_SHARED_LIBRARIES)/libjavacore$(ART_HOST_SHLIB_EXTENSION) \
  $(2ND_ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdk$(ART_HOST_SHLIB_EXTENSION) \
  $(2ND_ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdkd$(ART_HOST_SHLIB_EXTENSION) \
  $(2ND_ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdkjvmti$(ART_HOST_SHLIB_EXTENSION) \
  $(2ND_ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdkjvmtid$(ART_HOST_SHLIB_EXTENSION) \

endif

# Host executables.
host_prereq_rules := $(ART_TEST_HOST_RUN_TEST_DEPENDENCIES)

# Required for jasmin and smali.
host_prereq_rules += $(TEST_ART_RUN_TEST_DEPENDENCIES)

define core-image-dependencies
  image_suffix := $(3)
  ifeq ($(3),regalloc_gc)
    image_suffix:=optimizing
  else
    ifeq ($(3),jit)
      image_suffix:=interpreter
    endif
  endif
  ifeq ($(2),no-image)
    $(1)_prereq_rules += $$($(call to-upper,$(1))_CORE_IMAGE_$$(image_suffix)_$(4))
  else
    ifeq ($(2),picimage)
      $(1)_prereq_rules += $$($(call to-upper,$(1))_CORE_IMAGE_$$(image_suffix)_$(4))
    else
      ifeq ($(2),multipicimage)
        $(1)_prereq_rules += $$($(call to-upper,$(1))_CORE_IMAGE_$$(image_suffix)_multi_$(4))
      endif
    endif
  endif
endef

TARGET_TYPES := host target
COMPILER_TYPES := jit interpreter optimizing regalloc_gc jit interp-ac speed-profile
IMAGE_TYPES := picimage no-image multipicimage
ALL_ADDRESS_SIZES := 64 32

# Add core image dependencies required for given target - HOST or TARGET,
# IMAGE_TYPE, COMPILER_TYPE and ADDRESS_SIZE to the prereq_rules.
$(foreach target, $(TARGET_TYPES), \
  $(foreach image, $(IMAGE_TYPES), \
    $(foreach compiler, $(COMPILER_TYPES), \
      $(foreach address_size, $(ALL_ADDRESS_SIZES), $(eval \
        $(call core-image-dependencies,$(target),$(image),$(compiler),$(address_size)))))))

test-art-host-run-test-dependencies : $(host_prereq_rules)
.PHONY: test-art-host-run-test-dependencies
test-art-target-run-test-dependencies :
.PHONY: test-art-target-run-test-dependencies
test-art-run-test-dependencies : test-art-host-run-test-dependencies test-art-target-run-test-dependencies
.PHONY: test-art-run-test-dependencies

# Create a rule to build and run a test group of the following form:
# test-art-{1: host target}-run-test
define define-test-art-host-or-target-run-test-group
  build_target := test-art-$(1)-run-test
  .PHONY: $$(build_target)

  $$(build_target) : args := --$(1) --verbose
  $$(build_target) : test-art-$(1)-run-test-dependencies
	./art/test/testrunner/testrunner.py $$(args)
  build_target :=
  args :=
endef  # define-test-art-host-or-target-run-test-group

$(foreach target, $(TARGET_TYPES), $(eval \
  $(call define-test-art-host-or-target-run-test-group,$(target))))

test-art-run-test : test-art-host-run-test test-art-target-run-test

host_prereq_rules :=
core-image-dependencies :=
define-test-art-host-or-target-run-test-group :=
TARGET_TYPES :=
COMPILER_TYPES :=
IMAGE_TYPES :=
ALL_ADDRESS_SIZES :=
LOCAL_PATH :=
