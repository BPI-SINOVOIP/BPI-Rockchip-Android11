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

#include "stack_manager.h"

#include "gtest/gtest.h"
#include "os/thread.h"

namespace bluetooth {
namespace {

TEST(StackManagerTest, start_and_shutdown_no_module) {
  StackManager stack_manager;
  ModuleList module_list;
  os::Thread thread{"test_thread", os::Thread::Priority::NORMAL};
  stack_manager.StartUp(&module_list, &thread);
  stack_manager.ShutDown();
}

class TestModuleNoDependency : public Module {
 public:
  static const ModuleFactory Factory;

 protected:
  void ListDependencies(ModuleList* list) override {}
  void Start() override {}
  void Stop() override {}
};

const ModuleFactory TestModuleNoDependency::Factory = ModuleFactory([]() { return new TestModuleNoDependency(); });

TEST(StackManagerTest, get_module_instance) {
  StackManager stack_manager;
  ModuleList module_list;
  module_list.add<TestModuleNoDependency>();
  os::Thread thread{"test_thread", os::Thread::Priority::NORMAL};
  stack_manager.StartUp(&module_list, &thread);
  EXPECT_NE(stack_manager.GetInstance<TestModuleNoDependency>(), nullptr);
  stack_manager.ShutDown();
}

}  // namespace
}  // namespace bluetooth
