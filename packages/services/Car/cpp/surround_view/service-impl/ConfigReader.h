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

#ifndef SURROUND_VIEW_SERVICE_IMPL_CONFIGREADER_H_
#define SURROUND_VIEW_SERVICE_IMPL_CONFIGREADER_H_

#include <string>

#include "IOModuleCommon.h"
#include "core_lib.h"

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

// Parses the surround view config xml into struct SurroundViewConfig.
IOStatus ReadSurroundViewConfig(const std::string& configFile, SurroundViewConfig* svConfig);

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif  // SURROUND_VIEW_SERVICE_IMPL_CONFIGREADER_H_
