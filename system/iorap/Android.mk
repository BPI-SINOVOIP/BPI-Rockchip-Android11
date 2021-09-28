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

LOCAL_PATH := $(call my-dir)

# Build every binary target in system/iorap
.PHONY: iorap-nall
iorap-nall: \
  iorapd iorap.inode2filename iorapd-tests iorap.cmd.perfetto \
  iorap.cmd.compiler

# Build every binary target required for
# frameworks/base/startop/scripts/app_startup_runner
# to work with iorap.
.PHONY: iorap-app-startup-runner
iorap-app-startup-runner: \
  iorapd iorap.inode2filename \
  iorap.cmd.compiler


