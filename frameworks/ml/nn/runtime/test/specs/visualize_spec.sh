#!/bin/bash

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

set -Eeuo pipefail

# usage: visualize_spec.sh spec

SPEC_FILE=$1
SPEC_NAME=`basename -s .mod.py $1`
VISUALIZER_DIR=$ANDROID_BUILD_TOP/frameworks/ml/nn/tools/test_generator

# Create tmp dir for the generated HTML.
LOG_DIR=$(mktemp -d)/nnapi-spec-html
mkdir -p $LOG_DIR
$VISUALIZER_DIR/spec_visualizer.py $SPEC_FILE -o $LOG_DIR/${SPEC_NAME}.html

google-chrome $LOG_DIR/${SPEC_NAME}.html
