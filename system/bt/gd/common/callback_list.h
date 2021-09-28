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

#include <utility>
#include "base/callback_list.h"
#include "os/handler.h"

/* This file contains CallbackList implementation that will execute callback on provided Handler thread

Example usage inside your class:

private:
  common::CallbackList<void(int)> callbacks_list_;
public:
  std::unique_ptr<common::CallbackList<void(int)>::Subscription> RegisterCallback(
      const base::RepeatingCallback<void(int)>& cb, os::Handler* handler) {
    return callbacks_list_.Add({cb, handler});
  }

  void NotifyAllCallbacks(int value) {
    callbacks_list_.Notify(value);
  }
*/

namespace bluetooth {
namespace common {

namespace {
template <typename CallbackType>
struct CallbackWithHandler {
  CallbackWithHandler(base::RepeatingCallback<CallbackType> callback, os::Handler* handler)
      : callback(callback), handler(handler) {}

  bool is_null() const {
    return callback.is_null();
  }

  void Reset() {
    callback.Reset();
  }

  base::RepeatingCallback<CallbackType> callback;
  os::Handler* handler;
};

}  // namespace

template <typename Sig>
class CallbackList;
template <typename... Args>
class CallbackList<void(Args...)> : public base::internal::CallbackListBase<CallbackWithHandler<void(Args...)>> {
 public:
  using CallbackType = CallbackWithHandler<void(Args...)>;
  CallbackList() = default;
  template <typename... RunArgs>
  void Notify(RunArgs&&... args) {
    auto it = this->GetIterator();
    CallbackType* cb;
    while ((cb = it.GetNext()) != nullptr) {
      cb->handler->Post(base::Bind(cb->callback, args...));
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CallbackList);
};

}  // namespace common
}  // namespace bluetooth
