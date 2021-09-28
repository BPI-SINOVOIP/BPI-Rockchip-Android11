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

#include "base/logging.h"     // LOG() stdout and android log
#include "osi/include/log.h"  // android log only
#include "stack/include/btm_api.h"
#include "stack/include/btm_api_types.h"
#include "test/headless/get_options.h"
#include "test/headless/headless.h"
#include "test/headless/read/name.h"
#include "types/raw_address.h"

std::promise<tBTM_REMOTE_DEV_NAME> promise_;

void RemoteNameCallback(void* data) {
  promise_.set_value(*static_cast<tBTM_REMOTE_DEV_NAME*>(data));
}

int bluetooth::test::headless::Name::Run() {
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

  const RawAddress& raw_address = options_.device_.front();

  return RunOnHeadlessStack<int>([&raw_address]() {
    promise_ = std::promise<tBTM_REMOTE_DEV_NAME>();

    auto future = promise_.get_future();

    tBTM_STATUS status = BTM_ReadRemoteDeviceName(
        raw_address, &RemoteNameCallback, BT_TRANSPORT_BR_EDR);
    if (status != BTM_CMD_STARTED) {
      fprintf(stdout, "Failure to start read remote device\n");
      return -1;
    }

    tBTM_REMOTE_DEV_NAME name_packet = future.get();
    switch (name_packet.status) {
      case BTM_SUCCESS: {
        char buf[BD_NAME_LEN];
        memcpy(buf, name_packet.remote_bd_name, BD_NAME_LEN);
        std::string name(buf);
        fprintf(stdout, "Name result mac:%s name:%s\n",
                raw_address.ToString().c_str(), name.c_str());
      } break;
      case BTM_BAD_VALUE_RET:
        fprintf(stdout, "Name Timeout or other failure");
        return -2;
      default:
        fprintf(stdout, "Unexpected remote name request failure status:%hd",
                name_packet.status);
        return -2;
    }
    return 0;
  });
}
