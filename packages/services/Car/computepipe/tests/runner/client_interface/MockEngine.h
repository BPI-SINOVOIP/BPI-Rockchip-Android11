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

#ifndef COMPUTEPIPE_TESTS_RUNNER_CLIENTINTERFACE_MOCKENGINE_H
#define COMPUTEPIPE_TESTS_RUNNER_CLIENTINTERFACE_MOCKENGINE_H

#include <gmock/gmock.h>

#include "runner/client_interface/include/ClientEngineInterface.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace client_interface {
namespace tests {

class MockEngine : public ClientEngineInterface {
  public:
    MOCK_METHOD1(processClientConfigUpdate, Status(const proto::ConfigurationCommand&));
    MOCK_METHOD1(processClientCommand, Status(const proto::ControlCommand&));
    MOCK_METHOD2(freePacket, Status(int bufferId, int streamId));
};

}  // namespace tests
}  // namespace client_interface
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_TESTS_RUNNER_CLIENTINTERFACE_MOCKENGINE_H
