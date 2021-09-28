#!/bin/bash
# Copyright (C) 2020 The Android Open Source Project
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

tensorflow_project="$ANDROID_BUILD_TOP/external/tensorflow"

files_with_absl=(
  "$tensorflow_project/tensorflow/core/platform/tstring.h"
  "$tensorflow_project/tensorflow/lite/delegates/nnapi/acceleration_test_util.cc"
  "$tensorflow_project/tensorflow/lite/delegates/nnapi/nnapi_delegate.cc"
  "$tensorflow_project/tensorflow/lite/delegates/nnapi/nnapi_delegate.h"
  "$tensorflow_project/tensorflow/lite/kernels/lstm.cc"
  "$tensorflow_project/tensorflow/lite/delegates/nnapi/acceleration_test_util.h"
  "$tensorflow_project/tensorflow/lite/kernels/test_util.cc"
  "$tensorflow_project/tensorflow/lite/kernels/acceleration_test_util.cc",
  "$tensorflow_project/tensorflow/lite/kernels/acceleration_test_util_internal.h"
)

for file in "${files_with_absl[@]}"
do
    echo $file
    sed -i '/^#include.*absl.*$/d' $file
    sed -i '/\/\/.*absl::.*/!s/absl::/std::/g' $file
done
