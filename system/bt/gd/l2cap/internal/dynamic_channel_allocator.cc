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
#include "l2cap/classic/internal/link.h"
#include "l2cap/internal/dynamic_channel_allocator.h"
#include "l2cap/internal/dynamic_channel_impl.h"
#include "l2cap/security_policy.h"
#include "os/log.h"

namespace bluetooth {
namespace l2cap {
namespace internal {

std::shared_ptr<DynamicChannelImpl> DynamicChannelAllocator::AllocateChannel(Psm psm, Cid remote_cid,
                                                                             SecurityPolicy security_policy) {
  ASSERT_LOG(IsPsmValid(psm), "Psm 0x%x is invalid", psm);

  if (used_remote_cid_.find(remote_cid) != used_remote_cid_.end()) {
    LOG_INFO("Remote cid 0x%x is used", remote_cid);
    return nullptr;
  }
  Cid cid = kFirstDynamicChannel;
  for (; cid <= kLastDynamicChannel; cid++) {
    if (used_cid_.find(cid) == used_cid_.end()) break;
  }
  if (cid > kLastDynamicChannel) {
    LOG_WARN("All cid are used");
    return nullptr;
  }
  auto elem =
      channels_.try_emplace(cid, std::make_shared<DynamicChannelImpl>(psm, cid, remote_cid, link_, l2cap_handler_));
  ASSERT_LOG(elem.second, "Failed to create channel for psm 0x%x device %s", psm,
             link_->GetDevice().ToString().c_str());
  ASSERT(elem.first->second != nullptr);
  used_remote_cid_.insert(remote_cid);
  used_cid_.insert(cid);
  return elem.first->second;
}

std::shared_ptr<DynamicChannelImpl> DynamicChannelAllocator::AllocateReservedChannel(Cid reserved_cid, Psm psm,
                                                                                     Cid remote_cid,
                                                                                     SecurityPolicy security_policy) {
  ASSERT_LOG(IsPsmValid(psm), "Psm 0x%x is invalid", psm);

  if (used_remote_cid_.find(remote_cid) != used_remote_cid_.end()) {
    LOG_INFO("Remote cid 0x%x is used", remote_cid);
    return nullptr;
  }
  auto elem = channels_.try_emplace(
      reserved_cid, std::make_shared<DynamicChannelImpl>(psm, reserved_cid, remote_cid, link_, l2cap_handler_));
  ASSERT_LOG(elem.second, "Failed to create channel for psm 0x%x device %s", psm,
             link_->GetDevice().ToString().c_str());
  ASSERT(elem.first->second != nullptr);
  used_remote_cid_.insert(remote_cid);
  return elem.first->second;
}

Cid DynamicChannelAllocator::ReserveChannel() {
  Cid cid = kFirstDynamicChannel;
  for (; cid <= kLastDynamicChannel; cid++) {
    if (used_cid_.find(cid) == used_cid_.end()) break;
  }
  if (cid > kLastDynamicChannel) {
    LOG_WARN("All cid are used");
    return kInvalidCid;
  }
  used_cid_.insert(cid);
  return cid;
}

void DynamicChannelAllocator::FreeChannel(Cid cid) {
  used_cid_.erase(cid);
  auto channel = FindChannelByCid(cid);
  if (channel == nullptr) {
    LOG_INFO("Channel is not in use: psm %d, device %s", cid, link_->GetDevice().ToString().c_str());
    return;
  }
  used_remote_cid_.erase(channel->GetRemoteCid());
  channels_.erase(cid);
}

bool DynamicChannelAllocator::IsPsmUsed(Psm psm) const {
  for (const auto& channel : channels_) {
    if (channel.second->GetPsm() == psm) {
      return true;
    }
  }
  return false;
}

std::shared_ptr<DynamicChannelImpl> DynamicChannelAllocator::FindChannelByCid(Cid cid) {
  if (channels_.find(cid) == channels_.end()) {
    LOG_WARN("Can't find cid %d", cid);
    return nullptr;
  }
  return channels_.find(cid)->second;
}

std::shared_ptr<DynamicChannelImpl> DynamicChannelAllocator::FindChannelByRemoteCid(Cid remote_cid) {
  for (auto& channel : channels_) {
    if (channel.second->GetRemoteCid() == remote_cid) {
      return channel.second;
    }
  }
  return nullptr;
}

size_t DynamicChannelAllocator::NumberOfChannels() const {
  return channels_.size();
}

void DynamicChannelAllocator::OnAclDisconnected(hci::ErrorCode reason) {
  for (auto& elem : channels_) {
    elem.second->OnClosed(reason);
  }
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
