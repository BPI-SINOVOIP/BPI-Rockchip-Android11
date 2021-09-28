/*
 * Copyright 2020 The Android Open Source Project
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

#define LOG_TAG "bt_headless_sdp"

#include <future>

#include "base/logging.h"  // LOG() stdout and android log
#include "btif/include/btif_api.h"
#include "osi/include/log.h"  // android log only
#include "test/headless/get_options.h"
#include "test/headless/headless.h"
#include "test/headless/pairing/pairing.h"
#include "types/raw_address.h"

int bluetooth::test::headless::Pairing::Run() {
  if (options_.loop_ < 1) {
    fprintf(stdout, "This test requires at least a single loop");
    options_.Usage();
    return -1;
  }
  if (options_.device_.size() != 1) {
    fprintf(stdout, "This test requires a single device specified");
    options_.Usage();
    return -1;
  }

  RawAddress raw_address = options_.device_.front();

  return RunOnHeadlessStack<int>([raw_address]() {
    bt_status_t status = btif_dm_create_bond(&raw_address, BT_TRANSPORT_BR_EDR);
    switch (status) {
      case BT_STATUS_SUCCESS:
        break;
      default:
        fprintf(stdout, "Failed to create bond status:%d", status);
        break;
    }
    return status;
  });
}
