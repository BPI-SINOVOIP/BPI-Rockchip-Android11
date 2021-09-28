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

#include "l2cap/internal/receiver.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <future>

#include "l2cap/internal/channel_impl_mock.h"
#include "l2cap/l2cap_packets.h"
#include "os/handler.h"
#include "os/queue.h"
#include "os/thread.h"
#include "packet/raw_builder.h"

namespace bluetooth {
namespace l2cap {
namespace internal {
namespace {}  // namespace
}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
