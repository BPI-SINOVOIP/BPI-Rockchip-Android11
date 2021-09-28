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

#include "common/observer_registry.h"

#include "common/bind.h"
#include "gtest/gtest.h"

namespace bluetooth {
namespace common {

class SingleObserverRegistryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    registry_ = new SingleObserverRegistry;
  }

  void TearDown() override {
    delete registry_;
  }

  SingleObserverRegistry* registry_;
};

void Increment(int* count) {
  (*count)++;
}

void IncrementBy(int* count, int n) {
  (*count) += n;
}

TEST_F(SingleObserverRegistryTest, wrapped_callback) {
  int count = 0;
  auto wrapped_callback = registry_->Register(Bind(&Increment, Unretained(&count)));
  wrapped_callback.Run();
  EXPECT_EQ(count, 1);
  wrapped_callback.Run();
  EXPECT_EQ(count, 2);
  wrapped_callback.Run();
  EXPECT_EQ(count, 3);
  registry_->Unregister();
}

TEST_F(SingleObserverRegistryTest, unregister) {
  int count = 0;
  auto wrapped_callback = registry_->Register(Bind(&Increment, Unretained(&count)));
  registry_->Unregister();
  wrapped_callback.Run();
  EXPECT_EQ(count, 0);
}

TEST_F(SingleObserverRegistryTest, second_register) {
  int count = 0;
  auto wrapped_callback = registry_->Register(Bind(&Increment, Unretained(&count)));
  registry_->Unregister();
  auto wrapped_callback2 = registry_->Register(Bind(&Increment, Unretained(&count)));
  wrapped_callback.Run();
  EXPECT_EQ(count, 0);
  wrapped_callback2.Run();
  EXPECT_EQ(count, 1);
}

class MultipleObserverRegistryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    registry_ = new MultipleObserverRegistry<2>;
  }

  void TearDown() override {
    delete registry_;
  }

  MultipleObserverRegistry<2>* registry_;
};

TEST_F(MultipleObserverRegistryTest, single_wrapped_callback) {
  int count = 0;
  auto wrapped_callback = registry_->Register(0, Bind(&Increment, Unretained(&count)));
  wrapped_callback.Run();
  EXPECT_EQ(count, 1);
  wrapped_callback.Run();
  EXPECT_EQ(count, 2);
  wrapped_callback.Run();
  EXPECT_EQ(count, 3);
  registry_->Unregister(0);
}

TEST_F(MultipleObserverRegistryTest, multiple_wrapped_callback) {
  int count = 0;
  auto wrapped_callback0 = registry_->Register(0, Bind(&Increment, Unretained(&count)));
  auto wrapped_callback1 = registry_->Register(1, Bind(&IncrementBy, Unretained(&count), 10));
  wrapped_callback0.Run();
  EXPECT_EQ(count, 1);
  wrapped_callback1.Run();
  EXPECT_EQ(count, 11);
  registry_->Unregister(0);
  wrapped_callback0.Run();
  EXPECT_EQ(count, 11);
  wrapped_callback1.Run();
  EXPECT_EQ(count, 21);
  registry_->Unregister(1);
  EXPECT_EQ(count, 21);
}

}  // namespace common
}  // namespace bluetooth
