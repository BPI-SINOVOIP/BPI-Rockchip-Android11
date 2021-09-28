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

#include "RandomGraphGeneratorUtils.h"

#include <algorithm>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "RandomGraphGenerator.h"
#include "RandomVariable.h"

namespace android {
namespace nn {
namespace fuzzing_test {

std::mt19937 RandomNumberGenerator::generator;

std::string Logger::getElapsedTime() {
    auto end = std::chrono::high_resolution_clock::now();
    int ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - mStart).count();
    int hour = ms / 3600000;
    int minutes = (ms % 3600000) / 60000;
    int seconds = (ms % 60000) / 1000;
    int milli = ms % 1000;
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hour << ":" << std::setw(2) << minutes << ":"
        << std::setw(2) << seconds << "." << std::setw(3) << milli << " ";
    return oss.str();
}

}  // namespace fuzzing_test
}  // namespace nn
}  // namespace android
