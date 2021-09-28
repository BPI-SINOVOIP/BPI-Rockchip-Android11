/**
 * Copyright (c) 2020, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Experiments.h"

#include <android-base/format.h>
#include <netdutils/DumpWriter.h>
#include <string>

#include "util.h"

namespace android::net {

using netdutils::DumpWriter;
using netdutils::ScopedIndent;

Experiments* Experiments::getInstance() {
    // Instantiated on first use.
    static Experiments instance{getExperimentFlagInt};
    return &instance;
}

Experiments::Experiments(GetExperimentFlagIntFunction getExperimentFlagIntFunction)
    : mGetExperimentFlagIntFunction(std::move(getExperimentFlagIntFunction)) {
    updateInternal();
}

void Experiments::update() {
    updateInternal();
}

void Experiments::dump(DumpWriter& dw) const {
    std::lock_guard guard(mMutex);
    dw.println("Experiments list: ");
    for (const auto& [key, value] : mFlagsMapInt) {
        ScopedIndent indentStats(dw);
        if (value == Experiments::kFlagIntDefault) {
            dw.println(fmt::format("{}: UNSET", key));
        } else {
            dw.println(fmt::format("{}: {}", key, value));
        }
    }
}

void Experiments::updateInternal() {
    std::lock_guard guard(mMutex);
    for (const auto& key : kExperimentFlagKeyList) {
        mFlagsMapInt[key] = mGetExperimentFlagIntFunction(key, Experiments::kFlagIntDefault);
    }
}

int Experiments::getFlag(std::string_view key, int defaultValue) const {
    std::lock_guard guard(mMutex);
    auto it = mFlagsMapInt.find(key);
    if (it != mFlagsMapInt.end() && it->second != Experiments::kFlagIntDefault) {
        return it->second;
    }
    return defaultValue;
}

}  // namespace android::net
