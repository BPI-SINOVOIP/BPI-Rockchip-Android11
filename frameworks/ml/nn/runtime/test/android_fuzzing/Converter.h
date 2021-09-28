/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_ANDROID_FUZZING_CONVERTER_H
#define ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_ANDROID_FUZZING_CONVERTER_H

#include "Model.pb.h"
#include "TestHarness.h"

namespace android::nn::fuzz {

test_helper::TestModel convertToTestModel(const android_nn_fuzz::Test& model);

}  // namespace android::nn::fuzz

#endif  // ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_ANDROID_FUZZING_CONVERTER_H
