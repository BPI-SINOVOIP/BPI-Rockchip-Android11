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

#include "l2cap/classic/fixed_channel.h"
#include "common/bind.h"
#include "l2cap/classic/internal/fixed_channel_impl.h"

namespace bluetooth {
namespace l2cap {
namespace classic {

hci::Address FixedChannel::GetDevice() const {
  return impl_->GetDevice();
}

void FixedChannel::RegisterOnCloseCallback(os::Handler* user_handler, FixedChannel::OnCloseCallback on_close_callback) {
  l2cap_handler_->Post(common::BindOnce(&internal::FixedChannelImpl::RegisterOnCloseCallback, impl_, user_handler,
                                        std::move(on_close_callback)));
}

void FixedChannel::Acquire() {
  l2cap_handler_->Post(common::BindOnce(&internal::FixedChannelImpl::Acquire, impl_));
}

void FixedChannel::Release() {
  l2cap_handler_->Post(common::BindOnce(&internal::FixedChannelImpl::Release, impl_));
}

common::BidiQueueEnd<packet::BasePacketBuilder, packet::PacketView<packet::kLittleEndian>>*
FixedChannel::GetQueueUpEnd() const {
  return impl_->GetQueueUpEnd();
}
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth