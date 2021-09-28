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

class LeAdvertisingInterface {
 public:
  LeAdvertisingInterface() = default;
  virtual ~LeAdvertisingInterface() = default;
  DISALLOW_COPY_AND_ASSIGN(LeAdvertisingInterface);

  virtual void EnqueueCommand(std::unique_ptr<LeAdvertisingCommandBuilder> command,
                              common::OnceCallback<void(CommandCompleteView)> on_complete, os::Handler* handler) = 0;

  virtual void EnqueueCommand(std::unique_ptr<LeAdvertisingCommandBuilder> command,
                              common::OnceCallback<void(CommandStatusView)> on_status, os::Handler* handler) = 0;

  static constexpr hci::SubeventCode LeAdvertisingEvents[] = {
      hci::SubeventCode::SCAN_REQUEST_RECEIVED,
      hci::SubeventCode::ADVERTISING_SET_TERMINATED,
  };
};
}  // namespace hci
}  // namespace bluetooth
