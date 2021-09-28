#!/bin/bash

# Copyright (C) 2010 The Android Open Source Project
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

# A simple helper script that runs the Trade Federation func tests

TF_DIR=`dirname $0`/..

TEST_CLASS="com.android.tradefed.FuncTests"
DEVICE_TEST_CLASS="com.android.tradefed.DeviceFuncTests"

CLASS_FILTER="--class ${TEST_CLASS} --class ${DEVICE_TEST_CLASS}"

FORWARDED_ARGS=()
while [[ $# -gt 0 ]]; do
  next="$1"
  case ${next} in
  --class)
    CLASS_FILTER="--class $2"
    shift
    ;;
  *)
    FORWARDED_ARGS+=("$1")
    ;;
  esac
  shift
done


${TF_DIR}/tradefed.sh run singleCommand host \
  --console-result-reporter:suppress-passed-tests \
  ${CLASS_FILTER} ${FORWARDED_ARGS[*]}
