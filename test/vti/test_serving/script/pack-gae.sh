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

if [ -d "$ANDROID_BUILD_TOP" ]; then
  TEST_SERVING_DIR="$ANDROID_BUILD_TOP/test/vti/test_serving"
else
  CURRENT_DIR_NAME="${PWD##*/}"
  if [ "${CURRENT_DIR_NAME}" = "test_serving" ]; then
    TEST_SERVING_DIR="${PWD}"
  elif [ "${CURRENT_DIR_NAME}" = "script" ]; then
    TEST_SERVING_DIR="${PWD}/.."
  else
    echo "Missing ANDROID_BUILD_TOP env variable. Run 'lunch' first."
    exit 1
  fi
fi

if [ ! -d "${TEST_SERVING_DIR}/gae" ]; then
  echo "Please run this script in 'test_serving' directory."
  exit 1
fi

pushd $TEST_SERVING_DIR/gae
echo "Removing unnecessary files in ${TEST_SERVING_DIR}/gae directory..."
git clean -f

echo "Updating python libraries..."
rm -rf lib/
./script/install-pip.sh
popd

pushd $TEST_SERVING_DIR/
zip vtslab-scheduler-$(git log -s -n 1 --format="%cd" --date=format:"%Y%m%d_%H%M%S")-$(git rev-parse --short HEAD).zip -r gae -x *.pyc "*/\.*" *.DS_Store* gae/frontend/node_modules**\*
popd
