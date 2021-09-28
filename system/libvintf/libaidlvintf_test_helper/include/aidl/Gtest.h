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
#include <gtest/gtest.h>

namespace android {

static inline std::string PrintInstanceNameToString(
    const testing::TestParamInfo<std::string>& info) {
    // test names need to be unique -> index prefix
    std::string name = std::to_string(info.index) + "/" + info.param;

    for (size_t i = 0; i < name.size(); i++) {
        // gtest test names must only contain alphanumeric characters
        if (!std::isalnum(name[i])) name[i] = '_';
    }

    return name;
}

}  // namespace android
