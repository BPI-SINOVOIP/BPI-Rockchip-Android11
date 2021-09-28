/*
 * Copyright 2020 The Android Open Source Project
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

#ifndef WATCHDOG_SERVER_TESTS_PROCPIDDIR_H_
#define WATCHDOG_SERVER_TESTS_PROCPIDDIR_H_

#include <android-base/result.h>
#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace android {
namespace automotive {
namespace watchdog {
namespace testing {

android::base::Result<void> populateProcPidDir(
        const std::string& procDirPath,
        const std::unordered_map<uint32_t, std::vector<uint32_t>>& pidToTids,
        const std::unordered_map<uint32_t, std::string>& processStat,
        const std::unordered_map<uint32_t, std::string>& processStatus,
        const std::unordered_map<uint32_t, std::string>& threadStat);

}  // namespace testing
}  // namespace watchdog
}  // namespace automotive
}  // namespace android

#endif  //  WATCHDOG_SERVER_TESTS_PROCPIDDIR_H_
