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
#ifndef EVS_SUPPORT_LIBRARY_BASERENDERCALLBACK_H_
#define EVS_SUPPORT_LIBRARY_BASERENDERCALLBACK_H_

#include "Frame.h"

namespace android {
namespace automotive {
namespace evs {
namespace support {

class BaseRenderCallback {
  public:
    // TODO(b/130246434): Rename the callback to a more accurate name since
    // the callback itself is about image inline processing. Also avoid
    // passing in two frames, since the two frames are almost identical except
    // for the data pointer. Instead, pass in one input frame and one output
    // data pointer.
    virtual void render(const Frame& in, const Frame& out) = 0;
    virtual ~BaseRenderCallback() {
    }
};

}  // namespace support
}  // namespace evs
}  // namespace automotive
}  // namespace android

#endif  // EVS_SUPPORT_LIBRARY_BASERENDERCALLBACK_H_
