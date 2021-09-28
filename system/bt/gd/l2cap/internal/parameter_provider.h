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

#include <chrono>

namespace bluetooth {
namespace l2cap {
namespace internal {

/**
 * A class that provide constant parameters to the L2CAP stack
 *
 * All methods are virtual so that they can be override in unit tests
 */
class ParameterProvider {
 public:
  virtual ~ParameterProvider() = default;
  virtual std::chrono::milliseconds GetClassicLinkIdleDisconnectTimeout() {
    return std::chrono::seconds(20);
  }
  virtual std::chrono::milliseconds GetLeLinkIdleDisconnectTimeout() {
    return std::chrono::seconds(20);
  }
  virtual uint16_t GetLeMps() {
    return 251;
  }
  virtual uint16_t GetLeInitialCredit() {
    return 100;
  }
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
