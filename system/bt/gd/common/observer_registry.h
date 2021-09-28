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

#include <array>

#include "common/bind.h"
#include "common/callback.h"
#include "os/log.h"

namespace bluetooth {
namespace common {

// Tracks an observer registration on client (observer) code. Register() returns a wrapped callback object which can
// be passed to server's register API. Unregister() invalidates the wrapped callback so all callbacks that are posted
// to the client handler after the client called Unregister() call and before the server processed the Unregister()
// call on its handler, are dropped.
// Note: Register() invalidates the previous registration.
class SingleObserverRegistry {
 public:
  template <typename R, typename... T>
  decltype(auto) Register(Callback<R(T...)> callback) {
    session_++;
    return Bind(&SingleObserverRegistry::callback_wrapper<R, T...>, Unretained(this), session_, callback);
  }

  void Unregister() {
    session_++;
  }

 private:
  template <typename R, typename... T>
  void callback_wrapper(int session, Callback<R(T...)> callback, T... t) {
    if (session == session_) {
      callback.Run(std::forward<T>(t)...);
    }
  }

  uint8_t session_ = 0;
};

// Tracks observer registration for multiple event type. Each event type is represented as an integer in [0, Capacity).
template <int Capacity = 10>
class MultipleObserverRegistry {
 public:
  template <typename R, typename... T>
  decltype(auto) Register(int event_type, Callback<R(T...)> callback) {
    ASSERT(event_type < Capacity);
    return registry_[event_type].Register(callback);
  }

  void Unregister(int event_type) {
    ASSERT(event_type < Capacity);
    registry_[event_type].Unregister();
  }

  std::array<SingleObserverRegistry, Capacity> registry_;
};

}  // namespace common
}  // namespace bluetooth
