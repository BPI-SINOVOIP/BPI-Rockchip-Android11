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

#include "os/utils.h"

namespace bluetooth {
namespace os {

// A event_fd work in non-blocking and Semaphore mode
class ReactiveSemaphore {
 public:
  // Creates a new ReactiveSemaphore with an initial value of |value|.
  explicit ReactiveSemaphore(unsigned int value);
  ~ReactiveSemaphore();
  // Decrements the value of |fd_|, this will cause a crash if |fd_| unreadable.
  void Decrease();
  // Increase the value of |fd_|, this will cause a crash if |fd_| unwritable.
  void Increase();
  int GetFd();

  DISALLOW_COPY_AND_ASSIGN(ReactiveSemaphore);

 private:
  int fd_;
};

}  // namespace os
}  // namespace bluetooth
