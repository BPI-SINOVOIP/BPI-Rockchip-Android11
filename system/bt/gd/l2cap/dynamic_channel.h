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

#include "common/bidi_queue.h"
#include "common/callback.h"
#include "hci/acl_manager.h"
#include "os/handler.h"
#include "packet/base_packet_builder.h"
#include "packet/packet_view.h"

namespace bluetooth {
namespace l2cap {

namespace internal {
class DynamicChannelImpl;
}  // namespace internal

/**
 * L2CAP Dynamic channel object. User needs to call Close() when user no longer wants to use it. Otherwise the link
 * won't be disconnected.
 */
class DynamicChannel {
 public:
  // Should only be constructed by modules that have access to LinkManager
  DynamicChannel(std::shared_ptr<l2cap::internal::DynamicChannelImpl> impl, os::Handler* l2cap_handler)
      : impl_(std::move(impl)), l2cap_handler_(l2cap_handler) {
    ASSERT(impl_ != nullptr);
    ASSERT(l2cap_handler_ != nullptr);
  }

  hci::Address GetDevice() const;

  /**
   * Register close callback. If close callback is registered, when a channel is closed, the channel's resource will
   * only be freed after on_close callback is invoked. Otherwise, if no on_close callback is registered, the channel's
   * resource will be freed immediately after closing.
   *
   * @param user_handler The handler used to invoke the callback on
   * @param on_close_callback The callback invoked upon channel closing.
   */
  using OnCloseCallback = common::OnceCallback<void(hci::ErrorCode)>;
  void RegisterOnCloseCallback(os::Handler* user_handler, OnCloseCallback on_close_callback);

  /**
   * Indicate that this Dynamic Channel should be closed. OnCloseCallback will be invoked when channel close is done.
   * L2cay layer may terminate this ACL connection to free the resource after channel is closed.
   */
  void Close();

  /**
   * This method will retrieve the data channel queue to send and receive packets.
   *
   * {@see BidiQueueEnd}
   *
   * @return The upper end of a bi-directional queue.
   */
  common::BidiQueueEnd<packet::BasePacketBuilder, packet::PacketView<packet::kLittleEndian>>* GetQueueUpEnd() const;

 private:
  std::shared_ptr<l2cap::internal::DynamicChannelImpl> impl_;
  os::Handler* l2cap_handler_;
};

}  // namespace l2cap
}  // namespace bluetooth
