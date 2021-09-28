#
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

########################################################################
# Rules to build a smaller "core" image to support core libraries
# (that is, non-Android frameworks) testing on the host and target
#
# The main rules to build the default "boot" image are in
# build/core/dex_preopt_libart.mk

include art/build/Android.common_build.mk

LOCAL_DEX2OAT_HOST_INSTRUCTION_SET_FEATURES_OPTION :=
ifeq ($(DEX2OAT_HOST_INSTRUCTION_SET_FEATURES),)
  LOCAL_DEX2OAT_HOST_INSTRUCTION_SET_FEATURES_OPTION := --instruction-set-features=default
else
  LOCAL_DEX2OAT_HOST_INSTRUCTION_SET_FEATURES_OPTION := --instruction-set-features=$(DEX2OAT_HOST_INSTRUCTION_SET_FEATURES)
endif
LOCAL_$(HOST_2ND_ARCH_VAR_PREFIX)DEX2OAT_HOST_INSTRUCTION_SET_FEATURES_OPTION :=
ifeq ($($(HOST_2ND_ARCH_VAR_PREFIX)DEX2OAT_HOST_INSTRUCTION_SET_FEATURES),)
  LOCAL_$(HOST_2ND_ARCH_VAR_PREFIX)DEX2OAT_HOST_INSTRUCTION_SET_FEATURES_OPTION := --instruction-set-features=default
else
  LOCAL_$(HOST_2ND_ARCH_VAR_PREFIX)DEX2OAT_HOST_INSTRUCTION_SET_FEATURES_OPTION := --instruction-set-features=$($(HOST_2ND_ARCH_VAR_PREFIX)DEX2OAT_HOST_INSTRUCTION_SET_FEATURES)
endif

# Use dex2oat debug version for better error reporting
# $(1): compiler - optimizing, interpreter or interp-ac (interpreter-access-checks).
# $(2): 2ND_ or undefined, 2ND_ for 32-bit host builds.
define create-core-oat-host-rules
  core_compile_options :=
  core_image_name :=
  core_oat_name :=
  core_infix :=
  core_dex2oat_dependency := $(DEX2OAT)

  ifeq ($(1),optimizing)
    core_compile_options += --compiler-backend=Optimizing
  endif
  ifeq ($(1),interpreter)
    core_compile_options += --compiler-filter=quicken
    core_infix := -interpreter
  endif
  ifeq ($(1),interp-ac)
    core_compile_options += --compiler-filter=extract --runtime-arg -Xverify:softfail
    core_infix := -interp-ac
  endif
  ifneq ($(filter-out interpreter interp-ac optimizing,$(1)),)
    #Technically this test is not precise, but hopefully good enough.
    $$(error found $(1) expected interpreter, interp-ac, or optimizing)
  endif

  core_image_location := $(HOST_OUT_JAVA_LIBRARIES)/core$$(core_infix)$(CORE_IMG_SUFFIX)
  core_image_name := $($(2)HOST_CORE_IMG_OUT_BASE)$$(core_infix)$(CORE_IMG_SUFFIX)
  core_oat_name := $($(2)HOST_CORE_OAT_OUT_BASE)$$(core_infix)$(CORE_OAT_SUFFIX)

  # Using the bitness suffix makes it easier to add as a dependency for the run-test mk.
  ifeq ($(2),)
    HOST_CORE_IMAGE_$(1)_64 := $$(core_image_name)
  else
    HOST_CORE_IMAGE_$(1)_32 := $$(core_image_name)
  endif
  HOST_CORE_IMG_OUTS += $$(core_image_name)
  HOST_CORE_OAT_OUTS += $$(core_oat_name)

$$(core_image_name): PRIVATE_CORE_COMPILE_OPTIONS := $$(core_compile_options)
$$(core_image_name): PRIVATE_CORE_IMAGE_LOCATION := $$(core_image_location)
$$(core_image_name): PRIVATE_CORE_IMG_NAME := $$(core_image_name)
$$(core_image_name): PRIVATE_CORE_OAT_NAME := $$(core_oat_name)
# In addition to the primary core image containing HOST_CORE_IMG_DEX_FILES,
# also build a boot image extension for the remaining HOST_CORE_DEX_FILES.
$$(core_image_name): $$(HOST_CORE_DEX_LOCATIONS) $$(core_dex2oat_dependency)
	@echo "host dex2oat: $$@"
	@mkdir -p $$(dir $$@)
	$$(hide) ANDROID_LOG_TAGS="*:e" $$(DEX2OAT) \
	  --runtime-arg -Xms$(DEX2OAT_IMAGE_XMS) \
	  --runtime-arg -Xmx$(DEX2OAT_IMAGE_XMX) \
	  $$(addprefix --dex-file=,$$(HOST_CORE_IMG_DEX_FILES)) \
	  $$(addprefix --dex-location=,$$(HOST_CORE_IMG_DEX_LOCATIONS)) \
	  --oat-file=$$(PRIVATE_CORE_OAT_NAME) \
	  --oat-location=$$(PRIVATE_CORE_OAT_NAME) \
          --image=$$(PRIVATE_CORE_IMG_NAME) \
	  --base=$$(LIBART_IMG_HOST_BASE_ADDRESS) \
	  --instruction-set=$$($(2)ART_HOST_ARCH) \
	  $$(LOCAL_$(2)DEX2OAT_HOST_INSTRUCTION_SET_FEATURES_OPTION) \
	  --host --android-root=$$(HOST_OUT) \
	  --generate-debug-info --generate-build-id \
	  --runtime-arg -XX:SlowDebug=true \
	  --no-inline-from=core-oj-hostdex.jar \
	  $$(PRIVATE_CORE_COMPILE_OPTIONS) && \
	ANDROID_LOG_TAGS="*:e" $$(DEX2OAT) \
	  --runtime-arg -Xms$(DEX2OAT_IMAGE_XMS) \
	  --runtime-arg -Xmx$(DEX2OAT_IMAGE_XMX) \
	  --runtime-arg -Xbootclasspath:$$(subst $$(space),:,$$(strip \
	        $$(HOST_CORE_DEX_FILES))) \
	  --runtime-arg -Xbootclasspath-locations:$$(subst $$(space),:,$$(strip \
	        $$(HOST_CORE_DEX_LOCATIONS))) \
	  $$(addprefix --dex-file=, \
	      $$(filter-out $$(HOST_CORE_IMG_DEX_FILES),$$(HOST_CORE_DEX_FILES))) \
	  $$(addprefix --dex-location=, \
	      $$(filter-out $$(HOST_CORE_IMG_DEX_LOCATIONS),$$(HOST_CORE_DEX_LOCATIONS))) \
	  --oat-file=$$(PRIVATE_CORE_OAT_NAME) \
	  --oat-location=$$(PRIVATE_CORE_OAT_NAME) \
	  --boot-image=$$(PRIVATE_CORE_IMAGE_LOCATION) \
	  --image=$$(PRIVATE_CORE_IMG_NAME) \
	  --instruction-set=$$($(2)ART_HOST_ARCH) \
	  $$(LOCAL_$(2)DEX2OAT_HOST_INSTRUCTION_SET_FEATURES_OPTION) \
	  --host --android-root=$$(HOST_OUT) \
	  --generate-debug-info --generate-build-id \
	  --runtime-arg -XX:SlowDebug=true \
	  --no-inline-from=core-oj-hostdex.jar \
	  $$(PRIVATE_CORE_COMPILE_OPTIONS)

$$(core_oat_name): $$(core_image_name)

  # Clean up locally used variables.
  core_dex2oat_dependency :=
  core_compile_options :=
  core_image_name :=
  core_oat_name :=
  core_infix :=
endef  # create-core-oat-host-rules

# $(1): compiler - optimizing, interpreter or interp-ac (interpreter-access-checks).
define create-core-oat-host-rule-combination
  $(call create-core-oat-host-rules,$(1),)

  ifneq ($(HOST_PREFER_32_BIT),true)
    $(call create-core-oat-host-rules,$(1),2ND_)
  endif
endef

$(eval $(call create-core-oat-host-rule-combination,optimizing))
$(eval $(call create-core-oat-host-rule-combination,interpreter))
$(eval $(call create-core-oat-host-rule-combination,interp-ac))

.PHONY: test-art-host-dex2oat-host
test-art-host-dex2oat-host: $(HOST_CORE_IMG_OUTS)

# $(1): compiler - optimizing, interpreter or interp-ac (interpreter-access-checks).
# $(2): 2ND_ or undefined
define create-core-oat-target-rules
  core_compile_options :=
  core_image_name :=
  core_oat_name :=
  core_infix :=
  core_dex2oat_dependency := $(DEX2OAT)

  ifeq ($(1),optimizing)
    core_compile_options += --compiler-backend=Optimizing
  endif
  ifeq ($(1),interpreter)
    core_compile_options += --compiler-filter=quicken
    core_infix := -interpreter
  endif
  ifeq ($(1),interp-ac)
    core_compile_options += --compiler-filter=extract --runtime-arg -Xverify:softfail
    core_infix := -interp-ac
  endif
  ifneq ($(filter-out interpreter interp-ac optimizing,$(1)),)
    # Technically this test is not precise, but hopefully good enough.
    $$(error found $(1) expected interpreter, interp-ac, or optimizing)
  endif

  core_image_location := $(ART_TARGET_TEST_OUT)/core$$(core_infix)$(CORE_IMG_SUFFIX)
  core_image_name := $($(2)TARGET_CORE_IMG_OUT_BASE)$$(core_infix)$(CORE_IMG_SUFFIX)
  core_oat_name := $($(2)TARGET_CORE_OAT_OUT_BASE)$$(core_infix)$(CORE_OAT_SUFFIX)

  # Using the bitness suffix makes it easier to add as a dependency for the run-test mk.
  ifeq ($(2),)
    ifdef TARGET_2ND_ARCH
      TARGET_CORE_IMAGE_$(1)_64 := $$(core_image_name)
    else
      TARGET_CORE_IMAGE_$(1)_32 := $$(core_image_name)
    endif
  else
    TARGET_CORE_IMAGE_$(1)_32 := $$(core_image_name)
  endif
  TARGET_CORE_IMG_OUTS += $$(core_image_name)
  TARGET_CORE_OAT_OUTS += $$(core_oat_name)

$$(core_image_name): PRIVATE_CORE_COMPILE_OPTIONS := $$(core_compile_options)
$$(core_image_name): PRIVATE_CORE_IMAGE_LOCATION := $$(core_image_location)
$$(core_image_name): PRIVATE_CORE_IMG_NAME := $$(core_image_name)
$$(core_image_name): PRIVATE_CORE_OAT_NAME := $$(core_oat_name)
# In addition to the primary core image containing TARGET_CORE_IMG_DEX_FILES,
# also build a boot image extension for the remaining TARGET_CORE_DEX_FILES.
$$(core_image_name): $$(TARGET_CORE_DEX_FILES) $$(core_dex2oat_dependency)
	@echo "target dex2oat: $$@"
	@mkdir -p $$(dir $$@)
	$$(hide) $$(DEX2OAT) \
	  --runtime-arg -Xms$(DEX2OAT_IMAGE_XMS) \
	  --runtime-arg -Xmx$(DEX2OAT_IMAGE_XMX) \
	  $$(addprefix --dex-file=,$$(TARGET_CORE_IMG_DEX_FILES)) \
	  $$(addprefix --dex-location=,$$(TARGET_CORE_IMG_DEX_LOCATIONS)) \
	  --oat-file=$$(PRIVATE_CORE_OAT_NAME) \
	  --oat-location=$$(PRIVATE_CORE_OAT_NAME) \
	  --image=$$(PRIVATE_CORE_IMG_NAME) \
	  --base=$$(LIBART_IMG_TARGET_BASE_ADDRESS) \
	  --instruction-set=$$($(2)TARGET_ARCH) \
	  --instruction-set-variant=$$($(2)DEX2OAT_TARGET_CPU_VARIANT) \
	  --instruction-set-features=$$($(2)DEX2OAT_TARGET_INSTRUCTION_SET_FEATURES) \
	  --android-root=$$(PRODUCT_OUT)/system \
	  --generate-debug-info --generate-build-id \
	  --runtime-arg -XX:SlowDebug=true \
	  $$(PRIVATE_CORE_COMPILE_OPTIONS) && \
	$$(DEX2OAT) \
	  --runtime-arg -Xms$(DEX2OAT_IMAGE_XMS) \
	  --runtime-arg -Xmx$(DEX2OAT_IMAGE_XMX) \
	  --runtime-arg -Xbootclasspath:$$(subst $$(space),:,$$(strip \
	        $$(TARGET_CORE_DEX_FILES))) \
	  --runtime-arg -Xbootclasspath-locations:$$(subst $$(space),:,$$(strip \
	        $$(TARGET_CORE_DEX_LOCATIONS))) \
	  $$(addprefix --dex-file=, \
	       $$(filter-out $$(TARGET_CORE_IMG_DEX_FILES),$$(TARGET_CORE_DEX_FILES))) \
	  $$(addprefix --dex-location=, \
	       $$(filter-out $$(TARGET_CORE_IMG_DEX_LOCATIONS),$$(TARGET_CORE_DEX_LOCATIONS))) \
	  --oat-file=$$(PRIVATE_CORE_OAT_NAME) \
	  --oat-location=$$(PRIVATE_CORE_OAT_NAME) \
	  --boot-image=$$(PRIVATE_CORE_IMAGE_LOCATION) \
	  --image=$$(PRIVATE_CORE_IMG_NAME) \
	  --instruction-set=$$($(2)TARGET_ARCH) \
	  --instruction-set-variant=$$($(2)DEX2OAT_TARGET_CPU_VARIANT) \
	  --instruction-set-features=$$($(2)DEX2OAT_TARGET_INSTRUCTION_SET_FEATURES) \
	  --android-root=$$(PRODUCT_OUT)/system \
	  --generate-debug-info --generate-build-id \
	  --runtime-arg -XX:SlowDebug=true \
	  $$(PRIVATE_CORE_COMPILE_OPTIONS) || \
	(rm $$(PRIVATE_CORE_OAT_NAME); exit 1)

$$(core_oat_name): $$(core_image_name)

  # Clean up locally used variables.
  core_dex2oat_dependency :=
  core_compile_options :=
  core_image_name :=
  core_oat_name :=
  core_infix :=
endef  # create-core-oat-target-rules

# $(1): compiler - optimizing, interpreter or interp-ac (interpreter-access-checks).
define create-core-oat-target-rule-combination
  $(call create-core-oat-target-rules,$(1),)

  ifdef TARGET_2ND_ARCH
    $(call create-core-oat-target-rules,$(1),2ND_)
  endif
endef

$(eval $(call create-core-oat-target-rule-combination,optimizing))
$(eval $(call create-core-oat-target-rule-combination,interpreter))
$(eval $(call create-core-oat-target-rule-combination,interp-ac))

# Define a default core image that can be used for things like gtests that
# need some image to run, but don't otherwise care which image is used.
HOST_CORE_IMAGE_DEFAULT_32 := $(HOST_CORE_IMAGE_optimizing_32)
HOST_CORE_IMAGE_DEFAULT_64 := $(HOST_CORE_IMAGE_optimizing_64)
TARGET_CORE_IMAGE_DEFAULT_32 := $(TARGET_CORE_IMAGE_optimizing_32)
TARGET_CORE_IMAGE_DEFAULT_64 := $(TARGET_CORE_IMAGE_optimizing_64)
