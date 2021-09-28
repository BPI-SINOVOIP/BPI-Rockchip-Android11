#
# Copyright (C) 2019 The Android Open Source Project
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
####################################################################
# Car Ui test mapping target to check resources. #
####################################################################
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

intermediates := $(call intermediates-dir-for, PACKAGING, Android,,COMMON)
car-ui-stamp := $(intermediates)/car-ui-stamp
script := $(LOCAL_PATH)/auto-generate-resources.py

$(car-ui-stamp): PRIVATE_SCRIPT := $(script)
$(car-ui-stamp) : $(script)
	python $(PRIVATE_SCRIPT) -f=updated-resources.xml -c
	$(hide) mkdir -p $(dir $@) && touch $@

.PHONY: generate-resources
LOCAL_COMPATIBILITY_SUITE := device-tests

generate-resources : $(car-ui-stamp)
