/*
 * Copyright 2019 The Android Open Source Project
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
#pragma once

namespace bluetooth {
namespace l2cap {

class SecurityPolicy {
 public:
  enum class Level {
    LEVEL_0,  // Encryption not needed. Only applies to SDP.
    LEVEL_2,  // Encryption desired. Only needs unauthenticated link key.
    LEVEL_3,  // Encryption required and authenticated link key required.
  };
  Level security_level_ = Level::LEVEL_0;

  bool RequiresAuthentication() const {
    return security_level_ != SecurityPolicy::Level::LEVEL_0;
  }
};

}  // namespace l2cap
}  // namespace bluetooth
