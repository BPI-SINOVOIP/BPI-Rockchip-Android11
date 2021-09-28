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
#ifndef ANDROID_HARDWARE_VIBRATOR_TEST_TYPES_H
#define ANDROID_HARDWARE_VIBRATOR_TEST_TYPES_H

#include <aidl/android/hardware/vibrator/IVibrator.h>

using EffectIndex = uint16_t;
using EffectLevel = uint32_t;
using EffectAmplitude = float;
using EffectScale = uint16_t;
using EffectDuration = uint32_t;
using EffectQueue = std::tuple<std::string, EffectDuration>;
using EffectTuple = std::tuple<::aidl::android::hardware::vibrator::Effect,
                               ::aidl::android::hardware::vibrator::EffectStrength>;

using QueueEffect = std::tuple<EffectIndex, EffectLevel>;
using QueueDelay = uint32_t;

#endif  // ANDROID_HARDWARE_VIBRATOR_TEST_TYPES_H
