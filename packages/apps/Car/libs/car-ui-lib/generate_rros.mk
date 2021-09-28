#
# Copyright (C) 2019 The Android Open-Source Project
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

# Generates one RRO for a given package.
# $(1) target package name
# $(2) name of the RRO set (e.g. "base")
# $(3) resources folder
define generate-rro
  include $$(CLEAR_VARS)

  rro_package_name := $(subst .,-,$(2))-$(subst .,-,$(1))
  manifest_file := $(4)
  LOCAL_RESOURCE_DIR := $(3)
  LOCAL_RRO_THEME := $$(rro_package_name)
  LOCAL_PACKAGE_NAME := $$(rro_package_name)
  LOCAL_CERTIFICATE := platform
  LOCAL_SDK_VERSION := current
  LOCAL_USE_AAPT2 := true

  # Add --no-resource-deduping to prevent overlays to "values-port" with the same
  # value as in "values" from being removed
  LOCAL_AAPT_FLAGS := --no-resource-deduping

  gen := $$(call intermediates-dir-for,ETC,$$(rro_package_name))/AndroidManifest.xml
  $$(gen): $$(manifest_file)
	@echo Generate $$@
	$$(hide) mkdir -p $$(dir $$@)
	$$(hide) sed -e "s/{{TARGET_PACKAGE_NAME}}/$(1)/" \
	             -e "s/{{RRO_PACKAGE_NAME}}/$(1).$(2).rro/" $$< > $$@
  LOCAL_FULL_MANIFEST_FILE := $$(gen)

  include $$(BUILD_RRO_PACKAGE)
endef

ifndef CAR_UI_RRO_MANIFEST_FILE
CAR_UI_RRO_MANIFEST_FILE = $(LOCAL_PATH)/AndroidManifest.xml
endif

$(foreach t,\
  $(CAR_UI_RRO_TARGETS),\
  $(eval $(call generate-rro,$(t),$(CAR_UI_RRO_SET_NAME),$(CAR_UI_RESOURCE_DIR),$(CAR_UI_RRO_MANIFEST_FILE))) \
  )

# Clear variables
CAR_UI_RRO_SET_NAME :=
CAR_UI_RRO_MANIFEST_FILE :=
CAR_UI_RESOURCE_DIR :=
CAR_UI_RRO_TARGETS :=
