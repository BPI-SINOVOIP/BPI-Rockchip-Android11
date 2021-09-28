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

#include "l2cap/cid.h"
#include "l2cap/classic/fixed_channel_service.h"
#include "l2cap/classic/internal/fixed_channel_service_impl.h"
#include "os/handler.h"

namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {

class FixedChannelServiceManagerImpl {
 public:
  explicit FixedChannelServiceManagerImpl(os::Handler* l2cap_layer_handler)
      : l2cap_layer_handler_(l2cap_layer_handler) {}
  virtual ~FixedChannelServiceManagerImpl() = default;

  // All APIs must be invoked in L2CAP layer handler

  virtual void Register(Cid cid, FixedChannelServiceImpl::PendingRegistration pending_registration);
  virtual void Unregister(Cid cid, FixedChannelService::OnUnregisteredCallback callback, os::Handler* handler);
  virtual bool IsServiceRegistered(Cid cid) const;
  virtual FixedChannelServiceImpl* GetService(Cid cid);
  virtual std::vector<std::pair<Cid, FixedChannelServiceImpl*>> GetRegisteredServices();
  virtual uint64_t GetSupportedFixedChannelMask();

 private:
  os::Handler* l2cap_layer_handler_ = nullptr;
  std::unordered_map<Cid, FixedChannelServiceImpl> service_map_;
};
}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
