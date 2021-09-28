/*
 * Copyright (C) 2018 The Android Open Source Project
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
#include <gtest/gtest.h>

namespace android {

namespace {
using AtomicBufferStateTest = ::testing::Test;
}  // namespace

// Part of the buffer synchronization mechanism is achieved by using atomic
// variables, e.g. buffer state, located in Android shared memory. This test is
// to makes sure that a variable of type std::atomic<uint32_t> is lock-free, and
// hopefully address-free, so that they would be suitable for communication
// between processes using shared memory.
TEST(AtomicBufferStateTest, AtomicsRequiresLockFree) {
  std::atomic<uint32_t> fake_buffer_state(~0U);
  EXPECT_TRUE(fake_buffer_state.is_lock_free());
}

}  // namespace android
