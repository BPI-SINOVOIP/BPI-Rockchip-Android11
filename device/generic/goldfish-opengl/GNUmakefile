# Copyright 2018 The Android Open Source Project
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

# Check that we have at least GNU Make 3.81
# We do this by detecting whether 'lastword' is supported
#
MAKE_TEST := $(lastword a b c d e f)
ifneq ($(MAKE_TEST),f)
    $(error,This build system requires GNU Make 3.81 or higher to run !)
endif

# Find the source installation path, should be this file's location.
_BUILD_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))
_BUILD_ROOT := $(_BUILD_ROOT:%/=%)

# Complain if the path contains spaces
ifneq ($(words $(_BUILD_ROOT)),1)
    $(info,The source installation path contains spaces: '$(_BUILD_ROOT)')
    $(error,Please fix the problem by reinstalling to a different location.)
endif

# We are going to generate a JSON representation from the build
GOLDFISH_OPENGL_BUILD_FOR_HOST := true
CMAKE_GENERATE := true
_BUILD_CORE_DIR  := ../../../external/qemu/android/build

# We need the emulator's android makefile defs, so we can understand
# the makefiles.
include $(_BUILD_CORE_DIR)/emulator/definitions.make

# We need the ability to dump json.
include $(_BUILD_ROOT)/json-dump.mk

# And we are going to build like we are an emulator host.
include $(_BUILD_ROOT)/common.mk
include $(_BUILD_ROOT)/Android.mk

JSON_FILE := /tmp/build.json
JSON_DUMP := [ "" $(JSON_DUMP) ]

# And we are going to transform our generated json list into a set of 
# cmake files.

# This is the first target, so also the default target
cmake:
	@rm -f $(JSON_FILE)
	$(call write-to-file,$(JSON_FILE),30,$(JSON_DUMP))
	$(hide) python cmake_transform.py -i $(JSON_FILE) -c $(JSON_FILE) -o ${_BUILD_ROOT} 
