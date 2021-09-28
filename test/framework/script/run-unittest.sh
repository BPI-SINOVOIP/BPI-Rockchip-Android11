#!/bin/bash
#
# Copyright 2018 The Android Open Source Project
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

TEST_FRAMEWORK_DIR=`dirname $0`
TEST_FRAMEWORK_DIR=`dirname $TEST_FRAMEWORK_DIR`

if [ -z "$ANDROID_BUILD_TOP" ]; then
    echo "Missing ANDROID_BUILD_TOP env variable. Run 'lunch' first."
    exit 1
fi

touch $ANDROID_BUILD_TOP/test/vti/__init__.py

avoid_list=(
  # known failures
  "./harnesses/host_controller/console_test.py"
  "./harnesses/host_controller/invocation_thread_test.py"
  # not unit tests
  "./harnesses/host_controller/command_processor/command_test.py"
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

# Runs all unit tests under test/framework.
for t in $(find $TEST_FRAMEWORK_DIR -type f -name "*_test.py");
do
    if contains_file avoid_list $t; then
        continue
    fi
    echo "UNIT TEST", $t
    echo PYTHONPATH=$ANDROID_BUILD_TOP/test/framework/harnesses:$ANDROID_BUILD_TOP/test python $t;
    PYTHONPATH=$ANDROID_BUILD_TOP/test/framework/harnesses:$ANDROID_BUILD_TOP/test python $t;
done
