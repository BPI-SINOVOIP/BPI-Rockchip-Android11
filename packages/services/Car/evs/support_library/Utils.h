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
#ifndef CAR_LIB_EVS_SUPPORT_UTILS_H
#define CAR_LIB_EVS_SUPPORT_UTILS_H

#include <string>
#include <vector>

namespace android {
namespace automotive {
namespace evs {
namespace support {

using std::vector;
using std::string;

class Utils {
public:
    /**
     * Gets camera ids for all the available rear view cameras. For
     * now, we don't support dynamically adding/removing camera. In
     * other words, the camera list won't be updated after the first
     * time the camera list is obtained.
     *
     * An empty vector is returned if no rear view camera is found.
     */
    static vector<string> getRearViewCameraIds();

    /**
     * Gets camera id for the default rear view camera. For now, we
     * always assume that the first element in rear view camera list
     * is the default one.
     *
     * An empty string is returned if no rear view camera is found.
     */
    static string getDefaultRearViewCameraId();

private:
    static vector<string> sCameraIds;
};

}  // namespace support
}  // namespace evs
}  // namespace automotive
}  // namespace android

#endif  // CAR_LIB_EVS_SUPPORT_UTILS_H
