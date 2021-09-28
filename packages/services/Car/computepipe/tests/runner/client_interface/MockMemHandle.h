/*
 * Copyright 2020 The Android Open Source Project
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

#ifndef COMPUTEPIPE_TESTS_RUNNER_CLIENTINTERFACE_MOCKMEMHANDLE_H
#define COMPUTEPIPE_TESTS_RUNNER_CLIENTINTERFACE_MOCKMEMHANDLE_H

#include <gmock/gmock.h>

#include "MemHandle.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace tests {

class MockMemHandle : public MemHandle {
  public:
    MOCK_CONST_METHOD0(getStreamId, int());
    MOCK_CONST_METHOD0(getBufferId, int());
    MOCK_CONST_METHOD0(getType, proto::PacketType());
    MOCK_CONST_METHOD0(getTimeStamp, uint64_t());
    MOCK_CONST_METHOD0(getSize, uint32_t());
    MOCK_CONST_METHOD0(getData, const char*());
    MOCK_CONST_METHOD0(getHardwareBuffer, AHardwareBuffer*());
};

}  // namespace tests
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_TESTS_RUNNER_CLIENTINTERFACE_MOCKMEMHANDLE_H
