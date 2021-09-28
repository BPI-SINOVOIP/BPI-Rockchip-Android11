#
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
#

# $(1): List of target native files to copy.
# $(2): Copy destination directory.
# Evaluates to a list of ":"-separated pairs src:dst.
define target-native-copy-pairs
$(foreach m,$(1),\
  $(eval _built_files := $(strip $(ALL_MODULES.$(m).BUILT_INSTALLED)\
    $(ALL_MODULES.$(m)$(TARGET_2ND_ARCH_MODULE_SUFFIX).BUILT_INSTALLED)))\
  $(eval _module_class_folder := $($(strip MODULE_CLASS_$(word 1, $(strip $(ALL_MODULES.$(m).CLASS)\
    $(ALL_MODULES.$(m)$(TARGET_2ND_ARCH_MODULE_SUFFIX).CLASS))))))\
  $(foreach i, $(_built_files),\
    $(eval bui_ins := $(subst :,$(space),$(i)))\
    $(eval ins := $(word 2,$(bui_ins)))\
    $(if $(filter $(TARGET_OUT_ROOT)/%,$(ins)),\
      $(eval bui := $(word 1,$(bui_ins)))\
      $(if $(filter $(_module_class_folder), nativetest benchmarktest),\
        $(eval module_class_folder_stem := $(_module_class_folder)$(findstring 64, $(patsubst $(PRODUCT_OUT)/%,%,$(ins)))),\
        $(eval module_class_folder_stem := $(_module_class_folder)))\
      $(eval my_copy_dest := $(patsubst data/%,DATA/%,\
                               $(patsubst testcases/%,DATA/$(module_class_folder_stem)/%,\
                                 $(patsubst testcases/$(m)/$(TARGET_ARCH)/%,DATA/$(module_class_folder_stem)/$(m)/%,\
                                   $(patsubst testcases/$(m)/$(TARGET_2ND_ARCH)/%,DATA/$(module_class_folder_stem)/$(m)/%,\
                                     $(patsubst system/%,DATA/%,\
                                       $(patsubst $(PRODUCT_OUT)/%,%,$(ins))))))))\
      $(bui):$(2)/$(my_copy_dest))))
endef

# $(1): List of host native files to copy.
# $(2): Copy destination directory.
# Evaluates to a list of ":"-separated pairs src:dst.
define host-native-copy-pairs
$(foreach m,$(1),\
  $(eval _built_files := $(strip $(ALL_MODULES.$(m).BUILT_INSTALLED)\
  $(ALL_MODULES.$(m)$(HOST_2ND_ARCH_MODULE_SUFFIX).BUILT_INSTALLED)))\
  $(foreach i, $(_built_files),\
    $(eval bui_ins := $(subst :,$(space),$(i)))\
    $(eval ins := $(word 2,$(bui_ins)))\
    $(if $(filter $(HOST_OUT)/% $(HOST_CROSS_OUT)/%,$(ins)),\
      $(eval bui := $(word 1,$(bui_ins)))\
      $(eval my_copy_dest := $(patsubst $(HOST_OUT)/%,%,\
                               $(patsubst $(HOST_CROSS_OUT)/%,%,$(ins))))\
      $(bui):$(2)/host/$(my_copy_dest))))
endef

# $(1): The path to the lsdump.
# $(2): The path to the output dump.
define lsdump-to-dump
$(2) : $(1) $(HOST_OUT_EXECUTABLES)/extract_lsdump
	@echo "Generate:" $(notdir $(2))
	@mkdir -p $(dir $(2))
	@rm -f $(2)
	$(HOST_OUT_EXECUTABLES)/extract_lsdump $(1) $(2)
endef

# $(1): The target tuple. e.g., arm:arm:armv7-a-neon:32
# $(2): The output directory. e.g., $(VTS10_TESTCASES_OUT)/vts/testcases/vndk/golden.
# Evaluates to a list of destination files. (i.e. suitable for dependency list)
define create-vndk-abi-dump-from-target
$(strip \
  $(eval target_tuple := $(subst :, ,$(1))) \
  $(eval primary_arch := $(word 1, $(target_tuple))) \
  $(eval arch := $(word 2, $(target_tuple))) \
  $(eval arch_variant := $(word 3, $(target_tuple))) \
  $(eval binder_bitness := $(word 4, $(target_tuple))) \
  $(eval target_arch_variant := \
    $(arch)$(if $(filter $(arch_variant),$(arch)),,_$(arch_variant))) \
  $(eval lsdump_dir := \
    prebuilts/abi-dumps/vndk/$(PLATFORM_VNDK_VERSION)/$(binder_bitness)/$(target_arch_variant)/source-based) \
  $(if $(wildcard $(lsdump_dir)),\
    $(eval lsdump_names := \
      $(call find-files-in-subdirs,$(lsdump_dir),"*.lsdump" -and -type f,.)) \
    $(eval abi_dump_dir := \
      $(2)/$(PLATFORM_VNDK_VERSION)/binder$(binder_bitness)/$(primary_arch)/$(if $(findstring 64,$(arch)),lib64,lib)) \
    $(foreach f,$(lsdump_names),\
      $(eval copy_src := $(lsdump_dir)/$(f)) \
      $(eval copy_dst := $(abi_dump_dir)/$(f:%.lsdump=%.dump)) \
      $(eval $(call lsdump-to-dump,$(copy_src),$(copy_dst))) \
      $(copy_dst))))
endef
