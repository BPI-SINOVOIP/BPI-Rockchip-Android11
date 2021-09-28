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

#include "reactive_semaphore.h"

#include <error.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <functional>

#include "os/linux_generic/linux.h"
#include "os/log.h"

namespace bluetooth {
namespace os {

ReactiveSemaphore::ReactiveSemaphore(unsigned int value) : fd_(eventfd(value, EFD_SEMAPHORE | EFD_NONBLOCK)) {
  ASSERT(fd_ != -1);
}

ReactiveSemaphore::~ReactiveSemaphore() {
  int close_status;
  RUN_NO_INTR(close_status = close(fd_));
  ASSERT_LOG(close_status != -1, "close failed: %s", strerror(errno));
}

void ReactiveSemaphore::Decrease() {
  uint64_t val = 0;
  auto read_result = eventfd_read(fd_, &val);
  ASSERT_LOG(read_result != -1, "decrease failed: %s", strerror(errno));
}

void ReactiveSemaphore::Increase() {
  uint64_t val = 1;
  auto write_result = eventfd_write(fd_, val);
  ASSERT_LOG(write_result != -1, "increase failed: %s", strerror(errno));
}

int ReactiveSemaphore::GetFd() {
  return fd_;
}

}  // namespace os
}  // namespace bluetooth
