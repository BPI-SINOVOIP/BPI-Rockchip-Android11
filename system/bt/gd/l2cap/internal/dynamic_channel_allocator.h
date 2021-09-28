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

#pragma once

#include <unordered_map>
#include <unordered_set>

#include "hci/acl_manager.h"
#include "l2cap/cid.h"
#include "l2cap/internal/ilink.h"
#include "l2cap/psm.h"
#include "l2cap/security_policy.h"
#include "os/handler.h"
#include "os/log.h"

namespace bluetooth {
namespace l2cap {
namespace internal {

class DynamicChannelImpl;

// Helper class for keeping channels in a Link. It allocates and frees Channel object, and supports querying whether a
// channel is in use
class DynamicChannelAllocator {
 public:
  DynamicChannelAllocator(l2cap::internal::ILink* link, os::Handler* l2cap_handler)
      : link_(link), l2cap_handler_(l2cap_handler) {
    ASSERT(link_ != nullptr);
    ASSERT(l2cap_handler_ != nullptr);
  }

  // Allocates a channel. If psm is used, OR the remote cid already exists, return nullptr.
  // NOTE: The returned DynamicChannelImpl object is still owned by the channel allocator, NOT the client.
  std::shared_ptr<DynamicChannelImpl> AllocateChannel(Psm psm, Cid remote_cid, SecurityPolicy security_policy);

  std::shared_ptr<DynamicChannelImpl> AllocateReservedChannel(Cid reserved_cid, Psm psm, Cid remote_cid,
                                                              SecurityPolicy security_policy);

  // Gives an unused Cid to be used for opening a channel. If a channel is used, call AllocateReservedChannel. If no
  // longer needed, use FreeChannel.
  Cid ReserveChannel();

  // Frees a channel. If psm doesn't exist, it will crash
  void FreeChannel(Cid cid);

  bool IsPsmUsed(Psm psm) const;

  std::shared_ptr<DynamicChannelImpl> FindChannelByCid(Cid cid);
  std::shared_ptr<DynamicChannelImpl> FindChannelByRemoteCid(Cid cid);

  // Returns number of open, but not reserved channels
  size_t NumberOfChannels() const;

  void OnAclDisconnected(hci::ErrorCode hci_status);

 private:
  l2cap::internal::ILink* link_;
  os::Handler* l2cap_handler_;
  std::unordered_set<Cid> used_cid_;
  std::unordered_map<Cid, std::shared_ptr<DynamicChannelImpl>> channels_;
  std::unordered_set<Cid> used_remote_cid_;
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
