/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef CAR_LIB_EVS_SUPPORT_BASE_ANALYZE_CALLBACK_H
#define CAR_LIB_EVS_SUPPORT_BASE_ANALYZE_CALLBACK_H

#include "Frame.h"

namespace android {
namespace automotive {
namespace evs {
namespace support {

class BaseAnalyzeCallback{
    public:
        virtual void analyze(const Frame&) = 0;
        virtual ~BaseAnalyzeCallback() {};
};

}  // namespace support
}  // namespace evs
}  // namespace automotive
}  // namespace android

#endif // CAR_LIB_EVS_SUPPORT_BASE_ANALYZE_CALLBACK_H
