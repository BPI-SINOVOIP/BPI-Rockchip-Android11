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

#include <cstdint>
#include <functional>
#include "osi/include/alarm.h"

namespace bluetooth {
namespace shim {

class Timer {
 public:
  /**
   * Set this timer using the osi timer alarm functionality.
   *
   * The alarm duration *must not* be zero.  The timer is set
   * on the bluetooth main message loop thread.
   *
   * @param duration_ms Duration in milliseconds (>0) before alarm pops
   * @param func Function to execute upon alarm pop.
   */
  void Set(uint64_t duration_ms, std::function<void()> func);

  /**
   * Cancel this previously set timer.
   *
   * The associated function call will *not* be executed.
   */
  void Cancel();

  /**
   * Determine if a given timer has been set or not.
   *
   * @return |true| if timer has been set, |false| otherwise.
   */
  bool IsActive();

  /**
   * @param name Arbitrary name passed to the osi module.
   */
  Timer(const char* name);
  ~Timer();

  /**
   * Pop this timer.
   *
   * Called from an internal trampoline timeout global function registered
   * with osi alarm.  This trampoline function will then post
   * the execution of the callback function onto the shim thread.
   */
  static void Pop(Timer* timer);

 private:
  std::function<void()> callback_{};
  alarm_t* timer_{nullptr};
};

}  // namespace shim
}  // namespace bluetooth
