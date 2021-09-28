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

#define LOG_TAG "bt_shim_timer"

#include <base/bind.h>
#include <cstdint>
#include <functional>

#include "main/shim/shim.h"
#include "main/shim/timer.h"
#include "osi/include/alarm.h"
#include "osi/include/log.h"

static void timer_timeout(void* data) {
  CHECK(data != nullptr);
  bluetooth::shim::Timer* timeout = static_cast<bluetooth::shim::Timer*>(data);
  bluetooth::shim::Post(
      base::Bind(&bluetooth::shim::Timer::Pop, base::Unretained(timeout)));
}

void bluetooth::shim::Timer::Set(uint64_t duration_ms,
                                 std::function<void()> func) {
  CHECK(duration_ms != 0);
  callback_ = func;
  alarm_set_on_mloop(timer_, duration_ms, timer_timeout, (void*)this);
}

void bluetooth::shim::Timer::Cancel() {
  alarm_cancel(timer_);
  callback_ = {};
}

bool bluetooth::shim::Timer::IsActive() { return callback_ != nullptr; }

bluetooth::shim::Timer::Timer(const char* name) {
  timer_ = alarm_new(name);
  CHECK(timer_ != nullptr);
}

bluetooth::shim::Timer::~Timer() { alarm_free(timer_); }

void bluetooth::shim::Timer::Pop(Timer* timer) {
  timer->callback_();
  timer->callback_ = {};
}
