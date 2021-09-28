#
# Copyright 2017 The Android Open Source Project
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

ifeq ($(BUILD_QEMU_IMAGES),true)
  QEMU_CUSTOMIZATIONS := true
endif
ifeq ($(QEMU_CUSTOMIZATIONS),true)
  INSTALLED_EMULATOR_INFO_TXT_TARGET := $(PRODUCT_OUT)/emulator-info.txt
  emulator_info_txt := $(wildcard ${LOCAL_PATH}/emulator-info.txt)

  $(INSTALLED_EMULATOR_INFO_TXT_TARGET): $(emulator_info_txt)
	$(call pretty,"Generated: ($@)")
	$(hide) grep -v '#' $< > $@

  $(call dist-for-goals, dist_files, $(INSTALLED_EMULATOR_INFO_TXT_TARGET))
  $(call dist-for-goals, sdk, $(INSTALLED_EMULATOR_INFO_TXT_TARGET))

  subdir_makefiles=$(call first-makefiles-under,$(LOCAL_PATH))
  $(foreach mk,$(subdir_makefiles),$(info including $(mk) ...)$(eval include $(mk)))
endif
