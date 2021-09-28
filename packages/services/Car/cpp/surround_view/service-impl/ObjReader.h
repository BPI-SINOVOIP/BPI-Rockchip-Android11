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

#ifndef SURROUND_VIEW_SERVICE_IMPL_OBJREADER_H_
#define SURROUND_VIEW_SERVICE_IMPL_OBJREADER_H_

#include <map>
#include <string>

#include "core_lib.h"

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

using android_auto::surround_view::CarPart;

// ReadObjOptions for processing obj's vertex coordinates.
// Sequence of processing ReadObjOptions:
// 1. coordinate_mapping
// 2. scales
// 3. offsets
struct ReadObjOptions {
    // Maps obj coordinates to the output overlay coordinate.
    // 0 <-> x, 1 <-> y, 2 <-> z
    // Default is {0, 1, 2}, without coordinate changes.
    int coordinateMapping[3] = {0, 1, 2};

    // scale of each coordinate (after offsets).
    float scales[3] = {1.0f, 1.0f, 1.0f};

    // offset of each coordinate (after mapping).
    float offsets[3] = {0, 0, 0};

    // Optional mtl filename. String name is obj file is used if this is empty.
    std::string mtlFilename;
};

// Reads obj file to vector of OverlayVertex.
// |obj_filename| is the full path and name of the obj file.
// |car_parts_map| is a map containing all car parts.
// Now it only supports two face formats:
// 1. f x/x/x x/x/x x/x/x ...
// 2. f x//x x//x x//x ...
// b/
bool ReadObjFromFile(const std::string& objFilename, std::map<std::string, CarPart>* carPartsMap);

// Reads obj file to vector of OverlayVertex.
// |obj_filename| is the full path and name of the obj file.
// |option| provides optional changes on the coordinates.
// |car_parts_map| is a map containing all car parts.
bool ReadObjFromFile(const std::string& obFilename, const ReadObjOptions& option,
                     std::map<std::string, CarPart>* carPartsMap);

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif  // SURROUND_VIEW_SERVICE_IMPL_OBJREADER_H_
