#!/bin/bash
#
# Copyright 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if [ -z "$ANDROID_BUILD_TOP" ]; then
  echo "Missing ANDROID_BUILD_TOP env variable. Run 'lunch' first."
  exit 1
fi

avoid_list=(
  "./testcases/template/host_binary_test/host_binary_test.py"
  "./testcases/template/llvmfuzzer_test/llvmfuzzer_test.py"
  "./testcases/template/hal_hidl_replay_test/hal_hidl_replay_test.py"
  "./testcases/template/binary_test/binary_test.py"
  "./testcases/template/gtest_binary_test/gtest_binary_test.py"
  "./testcases/template/hal_hidl_host_test/hal_hidl_host_test.py"
  "./testcases/template/mobly/mobly_test.py"
  "./testcases/template/param_test/param_test.py"
  "./testcases/template/cts_test/cts_test.py"
  "./runners/host/base_test.py"
  "./utils/python/controllers/android_device_test.py"
  )

#######################################
# Checks if a given file is included in the list of files to avoid
# Globals:
# Arguments:
#   $1: list of files to avoid
#   $2: the given file
# Returns:
#   SUCCESS, if the given file exists in the list
#   FAILURE, otherwise
#######################################
function contains_file() {
  local -n arr=$1
  for avoid in "${arr[@]}"; do
    if [ "$2" = "$avoid" ]; then
      return  # contains
    fi
  done
  false  # not contains
}

# Runs all unit tests under test/vts.
for t in $(find $VTS_FRAMEWORK_DIR -type f -name "*_test.py"); do
  if contains_file avoid_list $t; then
    continue
  fi
  echo "UNIT TEST", $t
  PYTHONPATH=$ANDROID_BUILD_TOP/test:$ANDROID_BUILD_TOP/tools/test/connectivity/acts/framework python $t;
done
