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

#include "module.h"
#include "os/thread.h"
#include "os/handler.h"

namespace bluetooth {

class StackManager {
 public:
  void StartUp(ModuleList *modules, os::Thread* stack_thread);
  void ShutDown();

  template <class T>
  T* GetInstance() const {
    return static_cast<T*>(registry_.Get(&T::Factory));
  }

 private:
  os::Thread* management_thread_ = nullptr;
  os::Handler* handler_ = nullptr;
  ModuleRegistry registry_;

  void handle_start_up(ModuleList* modules, os::Thread* stack_thread, std::promise<void> promise);
  void handle_shut_down(std::promise<void> promise);
};

}  // namespace bluetooth
