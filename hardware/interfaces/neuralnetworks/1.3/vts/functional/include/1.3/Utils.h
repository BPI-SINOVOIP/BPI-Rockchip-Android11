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

#ifndef ANDROID_HARDWARE_NEURALNETWORKS_V1_3_UTILS_H
#define ANDROID_HARDWARE_NEURALNETWORKS_V1_3_UTILS_H

#include <android/hardware/neuralnetworks/1.3/types.h>
#include <iosfwd>

namespace android::hardware::neuralnetworks {

inline constexpr V1_3::Priority kDefaultPriority = V1_3::Priority::MEDIUM;

// Returns the amount of space needed to store a value of the specified type.
//
// Aborts if the specified type is an extension type or OEM type.
uint32_t sizeOfData(V1_3::OperandType type);

// Returns the amount of space needed to store a value of the dimensions and
// type of this operand. For a non-extension, non-OEM tensor with unspecified
// rank or at least one unspecified dimension, returns zero.
//
// Aborts if the specified type is an extension type or OEM type.
uint32_t sizeOfData(const V1_3::Operand& operand);

}  // namespace android::hardware::neuralnetworks

namespace android::hardware::neuralnetworks::V1_3 {

// pretty-print values for error messages
::std::ostream& operator<<(::std::ostream& os, ErrorStatus errorStatus);

}  // namespace android::hardware::neuralnetworks::V1_3

#endif  // ANDROID_HARDWARE_NEURALNETWORKS_V1_3_UTILS_H
