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

#include "OutputConfig.pb.h"
#include "PixelStreamManager.h"
#include "SemanticManager.h"
#include "StreamEngineInterface.h"
#include "StreamManager.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace stream_manager {

namespace {

/**
 * Build an instance of the Semantic Manager and initialize it
 */
std::unique_ptr<SemanticManager> buildSemanticManager(const proto::OutputConfig& config,
                                                      std::shared_ptr<StreamEngineInterface> engine,
                                                      uint32_t maxPackets) {
    std::unique_ptr<SemanticManager> semanticManager =
        std::make_unique<SemanticManager>(config.stream_name(), config.stream_id(), config.type());
    semanticManager->setEngineInterface(engine);
    if (semanticManager->setMaxInFlightPackets(maxPackets) != SUCCESS) {
        return nullptr;
    }
    return semanticManager;
}

std::unique_ptr<PixelStreamManager> buildPixelStreamManager(
    const proto::OutputConfig& config, std::shared_ptr<StreamEngineInterface> engine,
    uint32_t maxPackets) {
    std::unique_ptr<PixelStreamManager> pixelStreamManager =
        std::make_unique<PixelStreamManager>(config.stream_name(), config.stream_id());
    pixelStreamManager->setEngineInterface(engine);
    if (pixelStreamManager->setMaxInFlightPackets(maxPackets) != Status::SUCCESS) {
        return nullptr;
    }
    return pixelStreamManager;
}

}  // namespace

std::unique_ptr<StreamManager> StreamManagerFactory::getStreamManager(
    const proto::OutputConfig& config, std::shared_ptr<StreamEngineInterface> engine,
    uint32_t maxPackets) {
    if (!config.has_type()) {
        return nullptr;
    }
    switch (config.type()) {
        case proto::PacketType::SEMANTIC_DATA:
            return buildSemanticManager(config, engine, maxPackets);
        case proto::PacketType::PIXEL_DATA:
            return buildPixelStreamManager(config, engine, maxPackets);
        default:
            return nullptr;
    }
    return nullptr;
}

}  // namespace stream_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
