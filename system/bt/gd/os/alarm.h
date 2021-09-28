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

#include <functional>
#include <memory>
#include <mutex>

#include "common/callback.h"
#include "os/handler.h"
#include "os/thread.h"
#include "os/utils.h"

namespace bluetooth {
namespace os {

// A single-shot alarm for reactor-based thread, implemented by Linux timerfd.
// When it's constructed, it will register a reactable on the specified thread; when it's destroyed, it will unregister
// itself from the thread.
class Alarm {
 public:
  // Create and register a single-shot alarm on a given handler
  explicit Alarm(Handler* handler);

  // Unregister this alarm from the thread and release resource
  ~Alarm();

  DISALLOW_COPY_AND_ASSIGN(Alarm);

  // Schedule the alarm with given delay
  void Schedule(OnceClosure task, std::chrono::milliseconds delay);

  // Cancel the alarm. No-op if it's not armed.
  void Cancel();

 private:
  OnceClosure task_;
  Handler* handler_;
  int fd_ = 0;
  Reactor::Reactable* token_;
  mutable std::mutex mutex_;
  void on_fire();
};

}  // namespace os

}  // namespace bluetooth
