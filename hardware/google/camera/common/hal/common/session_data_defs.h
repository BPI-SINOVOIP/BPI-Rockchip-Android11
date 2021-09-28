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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_SESSION_DATA_DEFS_H
#define HARDWARE_GOOGLE_CAMERA_HAL_SESSION_DATA_DEFS_H

namespace android {
namespace google_camera_hal {

// The session data API is a generic low-overhead mechanism for
// internal data exchange across the (user-mode) driver stack. It
// allows any component to set a key/value per a device session
// and any other component to get that value in that session. It
// does not care about the semantics of the key/value pair: it is
// up to the usage parties (i.e. setters/getters) to agree on
// the semantics, including the life cycle and validity of the
// values. This is acceptable since this API is intended for
// internal communication and therefore, it's expected that its
// usage is part of a design where the assumptions / expectations
// and roles of the parties are clearly defined.
// Since there is no "type" associated with the keys, it is a
// good practice to include hints about the type in the key name.
//
// The keys defined in here are visible across GCH and HWL. The
// keys that are only visible within the HWL should be defined
// in HWL but extend below key domain (i.e. respect the defined
// range to avoid clashes.)

enum SessionDataKey : uint64_t {
  kSessionDataKeyInvalidKey = 0,
  // Add HAL-visible keys here (visible to GCH and HWL).
  // A pointer to an int variable that indicates current
  // Eis frame delay number.
  // The pointer will be valid for the entire session.
  kSessionDataKeyEisFrameDelayIntPtr,
  kSessionDataKeyBeginHwlKeys = 0xFFFFFFFF
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_SESSION_DATA_DEFS_H