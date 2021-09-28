
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

#include "common/callback.h"
#include "hci/address.h"
#include "l2cap/cid.h"
#include "os/handler.h"

namespace bluetooth {
namespace l2cap {
namespace classic {

namespace internal {
class FixedChannelServiceManagerImpl;
}  // namespace internal

class FixedChannelService {
 public:
  FixedChannelService() = default;

  using OnUnregisteredCallback = common::OnceCallback<void()>;

  /**
   * Unregister a service from L2CAP module. This operation cannot fail.
   * All channels opened for this service will be invalidated.
   *
   * @param on_unregistered will be triggered when unregistration is complete
   */
  void Unregister(OnUnregisteredCallback on_unregistered, os::Handler* on_unregistered_handler);

  friend internal::FixedChannelServiceManagerImpl;

 private:
  FixedChannelService(Cid cid, internal::FixedChannelServiceManagerImpl* manager, os::Handler* handler)
      : cid_(cid), manager_(manager), l2cap_layer_handler_(handler) {}
  Cid cid_ = kInvalidCid;
  internal::FixedChannelServiceManagerImpl* manager_ = nullptr;
  os::Handler* l2cap_layer_handler_;
  DISALLOW_COPY_AND_ASSIGN(FixedChannelService);
};

}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
