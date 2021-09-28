/*
 * Copyright (C) 2015 The Android Open Source Project
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

#pragma once

#include <utils/String8.h>
#include <utils/Vector.h>
#include <utils/RefBase.h>
#include <utils/Errors.h>
#include <system/audio.h>
#include <cutils/config_utils.h>

namespace android {

class PolicyAudioPort;
class AudioRoute;

using PolicyAudioPortVector = Vector<sp<PolicyAudioPort>>;
using AudioRouteVector = Vector<sp<AudioRoute>>;

sp<PolicyAudioPort> findByTagName(const PolicyAudioPortVector& policyAudioPortVector,
                                  const std::string &tagName);

void dumpAudioRouteVector(const AudioRouteVector& audioRouteVector, String8 *dst, int spaces);

} // namespace android
