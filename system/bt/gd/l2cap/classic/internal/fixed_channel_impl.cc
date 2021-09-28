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
#include "l2cap/classic/internal/fixed_channel_impl.h"
#include "l2cap/classic/internal/link.h"
#include "l2cap/security_policy.h"
#include "os/handler.h"
#include "os/log.h"

namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {

FixedChannelImpl::FixedChannelImpl(Cid cid, Link* link, os::Handler* l2cap_handler)
    : cid_(cid), device_(link->GetDevice()), link_(link), l2cap_handler_(l2cap_handler) {
  ASSERT_LOG(cid_ >= kFirstFixedChannel && cid_ <= kLastFixedChannel, "Invalid cid: %d", cid_);
  ASSERT(link_ != nullptr);
  ASSERT(l2cap_handler_ != nullptr);
}

void FixedChannelImpl::RegisterOnCloseCallback(os::Handler* user_handler,
                                               FixedChannel::OnCloseCallback on_close_callback) {
  ASSERT_LOG(user_handler_ == nullptr, "OnCloseCallback can only be registered once");
  // If channel is already closed, call the callback immediately without saving it
  if (closed_) {
    user_handler->Post(common::BindOnce(std::move(on_close_callback), close_reason_));
    return;
  }
  user_handler_ = user_handler;
  on_close_callback_ = std::move(on_close_callback);
}

void FixedChannelImpl::OnClosed(hci::ErrorCode status) {
  ASSERT_LOG(!closed_, "Device %s Cid 0x%x closed twice, old status 0x%x, new status 0x%x", device_.ToString().c_str(),
             cid_, static_cast<int>(close_reason_), static_cast<int>(status));
  closed_ = true;
  close_reason_ = status;
  acquired_ = false;
  link_ = nullptr;
  l2cap_handler_ = nullptr;
  if (user_handler_ == nullptr) {
    return;
  }
  // On close callback can only be called once
  user_handler_->Post(common::BindOnce(std::move(on_close_callback_), status));
  user_handler_ = nullptr;
  on_close_callback_.Reset();
}

void FixedChannelImpl::Acquire() {
  ASSERT_LOG(user_handler_ != nullptr, "Must register OnCloseCallback before calling any methods");
  if (closed_) {
    LOG_WARN("%s is already closed", ToString().c_str());
    ASSERT(!acquired_);
    return;
  }
  if (acquired_) {
    LOG_DEBUG("%s was already acquired", ToString().c_str());
    return;
  }
  acquired_ = true;
  link_->RefreshRefCount();
}

void FixedChannelImpl::Release() {
  ASSERT_LOG(user_handler_ != nullptr, "Must register OnCloseCallback before calling any methods");
  if (closed_) {
    LOG_WARN("%s is already closed", ToString().c_str());
    ASSERT(!acquired_);
    return;
  }
  if (!acquired_) {
    LOG_DEBUG("%s was already released", ToString().c_str());
    return;
  }
  acquired_ = false;
  link_->RefreshRefCount();
}

}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
