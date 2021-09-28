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

set -Eeuox pipefail
cd "$(dirname "$0")/.."  # runtime/test

NNAPI_VERSIONS="V1_0 V1_1 V1_2 V1_3 V1_3_cts_only"
EXAMPLE_GENERATOR="../../tools/test_generator/example_generator.py"

for source_version in $NNAPI_VERSIONS; do
  generated_dir="generated/spec_$source_version"
  mkdir -p "$generated_dir"
  "$EXAMPLE_GENERATOR" "specs/$source_version" \
    --example="$generated_dir" \
    "$@"
done
