#!/bin/bash
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# helper to run tf presubmit locally
tfdir="${ANDROID_HOST_OUT}/bin"

source "${tfdir}/script_help.sh"
checkPath adb

$tfdir/tradefed.sh run singleCommand tf/local-unit-launcher --folder-path ${TF_JAR_DIR} "$@"
