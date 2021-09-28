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
#include "hci/hci_packets.h"
#include "os/handler.h"
#include "os/utils.h"

namespace bluetooth {
namespace hci {

class LeSecurityInterface {
 public:
  LeSecurityInterface() = default;
  virtual ~LeSecurityInterface() = default;
  DISALLOW_COPY_AND_ASSIGN(LeSecurityInterface);

  virtual void EnqueueCommand(std::unique_ptr<LeSecurityCommandBuilder> command,
                              common::OnceCallback<void(CommandCompleteView)> on_complete, os::Handler* handler) = 0;

  virtual void EnqueueCommand(std::unique_ptr<LeSecurityCommandBuilder> command,
                              common::OnceCallback<void(CommandStatusView)> on_status, os::Handler* handler) = 0;

  static constexpr hci::SubeventCode LeSecurityEvents[] = {
      hci::SubeventCode::LONG_TERM_KEY_REQUEST,
      hci::SubeventCode::READ_LOCAL_P256_PUBLIC_KEY_COMPLETE,
      hci::SubeventCode::GENERATE_DHKEY_COMPLETE,
  };
};
}  // namespace hci
}  // namespace bluetooth
