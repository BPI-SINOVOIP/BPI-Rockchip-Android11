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

# TODO: Move this mk file to actual vts-core repo directory once it's created.

# Test XMLs, native executables, and packages will be placed in this
# directory before creating the final VTS distribution.
COMPATIBILITY_TESTCASES_OUT_vts := $(HOST_OUT)/vts/android-vts/testcases
# Force test binaries to be output to a hierarchical structure as
# testcases/module_nanme/arch/, v.s., a flat list under testcases directory.
COMPATIBILITY_TESTCASES_OUT_INCLUDE_MODULE_FOLDER_vts := true