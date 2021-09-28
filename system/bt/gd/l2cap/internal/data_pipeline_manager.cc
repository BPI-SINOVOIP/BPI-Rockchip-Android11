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

#include <unordered_map>

#include "l2cap/cid.h"
#include "l2cap/internal/channel_impl.h"
#include "l2cap/internal/data_controller.h"
#include "l2cap/internal/data_pipeline_manager.h"
#include "l2cap/internal/sender.h"
#include "os/log.h"

namespace bluetooth {
namespace l2cap {
namespace internal {

void DataPipelineManager::AttachChannel(Cid cid, std::shared_ptr<ChannelImpl> channel, ChannelMode mode) {
  ASSERT(sender_map_.find(cid) == sender_map_.end());
  sender_map_.emplace(std::piecewise_construct, std::forward_as_tuple(cid),
                      std::forward_as_tuple(handler_, link_, scheduler_.get(), channel, mode));
}

void DataPipelineManager::DetachChannel(Cid cid) {
  ASSERT(sender_map_.find(cid) != sender_map_.end());
  sender_map_.erase(cid);
}

DataController* DataPipelineManager::GetDataController(Cid cid) {
  ASSERT(sender_map_.find(cid) != sender_map_.end());
  return sender_map_.find(cid)->second.GetDataController();
}

void DataPipelineManager::OnPacketSent(Cid cid) {
  ASSERT(sender_map_.find(cid) != sender_map_.end());
  sender_map_.find(cid)->second.OnPacketSent();
}

void DataPipelineManager::UpdateClassicConfiguration(Cid cid, classic::internal::ChannelConfigurationState config) {
  ASSERT(sender_map_.find(cid) != sender_map_.end());
  sender_map_.find(cid)->second.UpdateClassicConfiguration(config);
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
