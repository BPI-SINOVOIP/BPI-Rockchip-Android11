// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "AidlClient.h"
#include "ClientInterface.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace client_interface {

using aidl_client::AidlClient;

namespace {

std::unique_ptr<AidlClient> buildAidlClient(const proto::Options& options,
                                            const std::shared_ptr<ClientEngineInterface>& engine) {
    return std::make_unique<AidlClient>(options, engine);
}

}  // namespace

std::unique_ptr<ClientInterface> ClientInterfaceFactory::createClientInterface(
    std::string iface, const proto::Options graphOptions,
    const std::shared_ptr<ClientEngineInterface>& engine) {
    if (iface == "aidl") {
        return buildAidlClient(graphOptions, engine);
    }
    return nullptr;
}

}  // namespace client_interface
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
