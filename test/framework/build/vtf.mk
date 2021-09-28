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
LOCAL_PATH := $(call my-dir)

vtf_tradefed_modules := \
  compatibility-common-util-tests \
  compatibility-host-util \
  compatibility-host-util-tests \
  compatibility-tradefed-tests \
  hosttestlib \
  host-libprotobuf-java-full \
  loganalysis \
  tradefed \
  vts-tradefed \
  vts10-tradefed \
  vts10-tradefed-tests \

vtf_tradefed_copy_pairs := \
  $(foreach f,$(vtf_tradefed_modules),\
    $(HOST_OUT)/framework/$(f).jar:$(VTF_TOOLS_OUT)/$(f).jar)

vtf_tradefed_additional_deps_copy_pairs := \
  test/vts/tools/vts-tradefed/etc/vts10-tradefed:$(VTF_TOOLS_OUT)/vts10-tradefed

vtf_tradefed_additional_deps_copy_pairs += \
  $(foreach f,$(VTF_COPY_VTF_BINARY),\
    test/vts/tools/vts-tradefed/etc/$(f):$(VTF_TESTCASES_OUT)/$(f))

vtf_package_copy_pairs := \
  $(call copy-many-files,$(vtf_tradefed_copy_pairs)) \
  $(call copy-many-files,$(vtf_tradefed_additional_deps_copy_pairs)) \

.PHONY: vtf
vtf: $(vtf_copy_pairs) $(vtf_package_copy_pairs)
