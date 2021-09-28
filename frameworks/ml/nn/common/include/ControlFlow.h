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

#ifndef ANDROID_FRAMEWORKS_ML_NN_COMMON_CONTROLFLOW_H
#define ANDROID_FRAMEWORKS_ML_NN_COMMON_CONTROLFLOW_H

namespace android {
namespace nn {
namespace operation_if {

constexpr uint32_t kCondBoolOperand = 0;
constexpr uint32_t kThenModelOperand = 1;
constexpr uint32_t kElseModelOperand = 2;
constexpr uint32_t kFirstInput = 3;

}  // namespace operation_if

namespace operation_while {

constexpr uint32_t kCondModelOperand = 0;
constexpr uint32_t kBodyModelOperand = 1;
constexpr uint32_t kFirstInput = 2;

// See ANeuralNetworksExecution_setLoopTimeout.
constexpr uint64_t kTimeoutNsDefault = 2'000'000'000;
constexpr uint64_t kTimeoutNsMaximum = 15'000'000'000;

}  // namespace operation_while
}  // namespace nn
}  // namespace android

#endif  // ANDROID_FRAMEWORKS_ML_NN_COMMON_CONTROLFLOW_H
